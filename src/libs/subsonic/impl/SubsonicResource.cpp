/*
 * copyright (c) 2019 emeric poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SubsonicResource.hpp"

#include <atomic>
#include <ctime>
#include <map>
#include <unordered_map>

#include <Wt/WDateTime.h>

#include "services/auth/IPasswordService.hpp"
#include "services/auth/IEnvService.hpp"
#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackBookmark.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Random.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "utils/Utils.hpp"

#include "entrypoints/AlbumSongLists.hpp"
#include "entrypoints/Bookmarks.hpp"
#include "entrypoints/MediaAnnotation.hpp"
#include "entrypoints/MediaLibraryScanning.hpp"
#include "entrypoints/MediaRetrieval.hpp"
#include "entrypoints/Searching.hpp"
#include "entrypoints/UserManagement.hpp"
#include "responses/Artist.hpp"
#include "responses/Album.hpp"
#include "responses/Song.hpp"
#include "ParameterParsing.hpp"
#include "ProtocolVersion.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "Utils.hpp"


using namespace Database;

static const std::string_view	genreClusterName {"GENRE"};
static const std::string_view	reportedStarredDate {"2000-01-01T00:00:00"};
static const std::string_view	reportedDummyDate {"2000-01-01T00:00:00"};
static const unsigned long long	reportedDummyDateULong {946684800000ULL}; // 2000-01-01T00:00:00 UTC

namespace API::Subsonic
{

std::unique_ptr<Wt::WResource>
createSubsonicResource(Database::Db& db)
{
	return std::make_unique<SubsonicResource>(db);
}


static
std::string
makeNameFilesystemCompatible(const std::string& name)
{
	return StringUtils::replaceInString(name, "/", "_");
}

static
std::unordered_map<std::string, ProtocolVersion>
readConfigProtocolVersions()
{
	std::unordered_map<std::string, ProtocolVersion> res;

	Service<IConfig>::get()->visitStrings("api-subsonic-report-old-server-protocol",
			[&](std::string_view client)
			{
				res.emplace(std::string {client}, ProtocolVersion {1, 12, 0});
			}, {"DSub"});

	return res;
}

SubsonicResource::SubsonicResource(Db& db)
: _serverProtocolVersionsByClient {readConfigProtocolVersions()}
, _db {db}
{
}

static
std::string parameterMapToDebugString(const Wt::Http::ParameterMap& parameterMap)
{
	auto censorValue = [](const std::string& type, const std::string& value) -> std::string
	{
		if (type == "p" || type == "password")
			return "*REDACTED*";
		else
			return value;
	};

	std::string res;

	for (const auto& params : parameterMap)
	{
		res += "{" + params.first + "=";
		if (params.second.size() == 1)
		{
			res += censorValue(params.first, params.second.front());
		}
		else
		{
			res += "{";
			for (const std::string& param : params.second)
				res += censorValue(params.first, param) + ",";
			res += "}";
		}
		res += "}, ";
	}

	return res;
}

static
void
checkUserTypeIsAllowed(RequestContext& context, EnumSet<Database::UserType> allowedUserTypes)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer currentUser {User::find(context.dbSession, context.userId)};
	if (!currentUser)
		throw RequestedDataNotFoundError {};

	if (!allowedUserTypes.contains(currentUser->getType()))
		throw UserNotAuthorizedError {};
}

static
Response::Node
clusterToResponseNode(const Cluster::pointer& cluster)
{
	Response::Node clusterNode;

	clusterNode.setValue(cluster->getName());
	clusterNode.setAttribute("songCount", cluster->getTracksCount());
	clusterNode.setAttribute("albumCount", cluster->getReleasesCount());

	return clusterNode;
}

static
Response
handlePingRequest(RequestContext& context)
{
	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleCreatePlaylistRequest(RequestContext& context)
{
	// Optional params
	const auto id {getParameterAs<TrackListId>(context.parameters, "playlistId")};
	auto name {getParameterAs<std::string>(context.parameters, "name")};

	std::vector<TrackId> trackIds {getMultiParametersAs<TrackId>(context.parameters, "songId")};

	if (!name && !id)
		throw RequiredParameterMissingError {"name or id"};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist;
	if (id)
	{
		tracklist = TrackList::find(context.dbSession, *id);
		if (!tracklist
			|| tracklist->getUser() != user
			|| tracklist->getType() != TrackListType::Playlist)
		{
			throw RequestedDataNotFoundError {};
		}

		if (name)
			tracklist.modify()->setName(*name);
	}
	else
	{
		tracklist = context.dbSession.create<TrackList>(*name, TrackListType::Playlist, false, user);
	}

	for (const TrackId trackId : trackIds)
	{
		Track::pointer track {Track::find(context.dbSession, trackId)};
		if (!track)
			continue;

		context.dbSession.create<TrackListEntry>(track, tracklist);
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleDeletePlaylistRequest(RequestContext& context)
{
	TrackListId id {getMandatoryParameterAs<TrackListId>(context.parameters, "id")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist {TrackList::find(context.dbSession, id)};
	if (!tracklist
		|| tracklist->getUser() != user
		|| tracklist->getType() != TrackListType::Playlist)
	{
		throw RequestedDataNotFoundError {};
	}

	tracklist.remove();

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleGetLicenseRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& licenseNode {response.createNode("license")};
	licenseNode.setAttribute("licenseExpires", "2025-09-03T14:46:43");
	licenseNode.setAttribute("email", "foo@bar.com");
	licenseNode.setAttribute("valid", true);

	return response;
}


static
Response
handleGetAlbumRequest(RequestContext& context)
{
	// Mandatory params
	ReleaseId id {getMandatoryParameterAs<ReleaseId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	Release::pointer release {Release::find(context.dbSession, id)};
	if (!release)
		throw RequestedDataNotFoundError {};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node albumNode {createAlbumNode(release, context.dbSession, user, true /* id3 */)};

	const auto tracks {Track::find(context.dbSession, Track::FindParameters {}.setRelease(id).setSortMethod(TrackSortMethod::Release))};
	for (const TrackId trackId : tracks.results)
	{
		const Track::pointer track {Track::find(context.dbSession, trackId)};
		albumNode.addArrayChild("song", createSongNode(track, context.dbSession, user));
	}

	response.addNode("album", std::move(albumNode));

	return response;
}

static
Response
handleGetSongRequest(RequestContext& context)
{
	// Mandatory params
	TrackId id {getMandatoryParameterAs<TrackId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	const Track::pointer track {Track::find(context.dbSession, id)};
	if (!track)
		throw RequestedDataNotFoundError {};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	response.addNode("song", createSongNode(track, context.dbSession, user));

	return response;
}

static
Response
handleGetArtistRequest(RequestContext& context)
{
	// Mandatory params
	ArtistId id {getMandatoryParameterAs<ArtistId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	const Artist::pointer artist {Artist::find(context.dbSession, id)};
	if (!artist)
		throw RequestedDataNotFoundError {};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node artistNode {createArtistNode(artist, context.dbSession, user, true /* id3 */)};

	const auto releases {Release::find(context.dbSession, Release::FindParameters {}.setArtist(artist->getId()))};
	for (const ReleaseId releaseId : releases.results)
	{
		const Release::pointer release {Release::find(context.dbSession, releaseId)};
		artistNode.addArrayChild("album", createAlbumNode(release, context.dbSession, user, true /* id3 */));
	}

	response.addNode("artist", std::move(artistNode));

	return response;
}

static
Response
handleGetArtistInfoRequestCommon(RequestContext& context, bool id3)
{
	// Mandatory params
	ArtistId id {getMandatoryParameterAs<ArtistId>(context.parameters, "id")};

	// Optional params
	std::size_t count {getParameterAs<std::size_t>(context.parameters, "count").value_or(20)};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& artistInfoNode {response.createNode(id3 ? "artistInfo2" : "artistInfo")};

	{
		auto transaction {context.dbSession.createSharedTransaction()};

		const Artist::pointer artist {Artist::find(context.dbSession, id)};
		if (!artist)
			throw RequestedDataNotFoundError {};

		std::optional<UUID> artistMBID {artist->getMBID()};
		if (artistMBID)
			artistInfoNode.createChild("musicBrainzId").setValue(artistMBID->getAsString());
	}

	auto similarArtistsId {Service<Recommendation::IRecommendationService>::get()->getSimilarArtists(id, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, count)};

	{
		auto transaction {context.dbSession.createSharedTransaction()};

		User::pointer user {User::find(context.dbSession, context.userId)};
		if (!user)
			throw UserNotAuthorizedError {};

		for (const ArtistId similarArtistId : similarArtistsId)
		{
			const Artist::pointer similarArtist {Artist::find(context.dbSession, similarArtistId)};
			if (similarArtist)
				artistInfoNode.addArrayChild("similarArtist", createArtistNode(similarArtist, context.dbSession, user, id3));
		}
	}

	return response;
}

static
Response
handleGetArtistInfoRequest(RequestContext& context)
{
	return handleGetArtistInfoRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetArtistInfo2Request(RequestContext& context)
{
	return handleGetArtistInfoRequestCommon(context, true /* id3 */);
}

static
Response
handleGetMusicDirectoryRequest(RequestContext& context)
{
	// Mandatory params
	const auto artistId {getParameterAs<ArtistId>(context.parameters, "id")};
	const auto releaseId {getParameterAs<ReleaseId>(context.parameters, "id")};
	const auto root {getParameterAs<RootId>(context.parameters, "id")};

	if (!root && !artistId && !releaseId)
		throw BadParameterGenericError {"id"};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& directoryNode {response.createNode("directory")};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	if (root)
	{
		directoryNode.setAttribute("id", idToString(RootId {}));
		directoryNode.setAttribute("name", "Music");

		auto rootArtistIds {Artist::find(context.dbSession, Artist::FindParameters {}.setSortMethod(ArtistSortMethod::BySortName))};
		for (const ArtistId rootArtistId : rootArtistIds.results)
		{
			const Artist::pointer artist {Artist::find(context.dbSession, rootArtistId)};
			directoryNode.addArrayChild("child", createArtistNode(artist, context.dbSession, user, false /* no id3 */));
		}
	}
	else if (artistId)
	{
		directoryNode.setAttribute("id", idToString(*artistId));

		auto artist {Artist::find(context.dbSession, *artistId)};
		if (!artist)
			throw RequestedDataNotFoundError {};

		directoryNode.setAttribute("name", makeNameFilesystemCompatible(artist->getName()));

		const auto artistReleases {Release::find(context.dbSession, Release::FindParameters {}.setArtist(*artistId))};
		for (const ReleaseId artistReleaseId : artistReleases.results)
		{
			const Release::pointer release {Release::find(context.dbSession, artistReleaseId)};
			directoryNode.addArrayChild("child", createAlbumNode(release, context.dbSession, user, false /* no id3 */));
		}
	}
	else if (releaseId)
	{
		directoryNode.setAttribute("id", idToString(*releaseId));

		auto release {Release::find(context.dbSession, *releaseId)};
		if (!release)
				throw RequestedDataNotFoundError {};

		directoryNode.setAttribute("name", makeNameFilesystemCompatible(release->getName()));

		const auto tracks {Track::find(context.dbSession, Track::FindParameters {}.setRelease(*releaseId).setSortMethod(TrackSortMethod::Release))};
		for (const TrackId trackId : tracks.results)
		{
			const Track::pointer track {Track::find(context.dbSession, trackId)};
			directoryNode.addArrayChild("child", createSongNode(track, context.dbSession, user));
		}
	}
	else
		throw BadParameterGenericError {"id"};

	return response;
}

static
Response
handleGetMusicFoldersRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& musicFoldersNode {response.createNode("musicFolders")};

	Response::Node& musicFolderNode {musicFoldersNode.createArrayChild("musicFolder")};
	musicFolderNode.setAttribute("id", "0");
	musicFolderNode.setAttribute("name", "Music");

	return response;
}

static
Response
handleGetArtistsRequestCommon(RequestContext& context, bool id3)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& artistsNode {response.createNode(id3 ? "artists" : "indexes")};
	artistsNode.setAttribute("ignoredArticles", "");
	artistsNode.setAttribute("lastModified", reportedDummyDateULong);

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Artist::FindParameters parameters;
	parameters.setSortMethod(ArtistSortMethod::BySortName);
	switch (user->getSubsonicArtistListMode())
	{
		case SubsonicArtistListMode::AllArtists:
			break;
		case SubsonicArtistListMode::ReleaseArtists:
			parameters.setLinkType(TrackArtistLinkType::ReleaseArtist);
			break;
		case SubsonicArtistListMode::TrackArtists:
			parameters.setLinkType(TrackArtistLinkType::Artist);
			break;
	}

	std::map<char, std::vector<Artist::pointer>> artistsSortedByFirstChar;
	const RangeResults<ArtistId> artists {Artist::find(context.dbSession, parameters)};
	for (const ArtistId artistId : artists.results)
	{
		const Artist::pointer artist {Artist::find(context.dbSession, artistId)};
		const std::string& sortName {artist->getSortName()};

		char sortChar;
		if (sortName.empty() || !std::isalpha(sortName[0]))
			sortChar = '?';
		else
			sortChar = std::toupper(sortName[0]);

		artistsSortedByFirstChar[sortChar].push_back(artist);
	}


	for (const auto& [sortChar, artists] : artistsSortedByFirstChar)
	{
		Response::Node& indexNode {artistsNode.createArrayChild("index")};
		indexNode.setAttribute("name", std::string {sortChar});

		for (const Artist::pointer& artist :artists)
			indexNode.addArrayChild("artist", createArtistNode(artist, context.dbSession, user, id3));
	}

	return response;
}

static
Response
handleGetIndexesRequest(RequestContext& context)
{
	return handleGetArtistsRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetGenresRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& genresNode {response.createNode("genres")};

	auto transaction {context.dbSession.createSharedTransaction()};

	const ClusterType::pointer clusterType {ClusterType::find(context.dbSession, genreClusterName)};
	if (clusterType)
	{
		const auto clusters {clusterType->getClusters()};

		for (const Cluster::pointer& cluster : clusters)
			genresNode.addArrayChild("genre", clusterToResponseNode(cluster));
	}

	return response;
}

static
Response
handleGetArtistsRequest(RequestContext& context)
{
	return handleGetArtistsRequestCommon(context, true /* id3 */);
}

static
std::vector<TrackId>
findSimilarSongs(RequestContext& context, ArtistId artistId, std::size_t count)
{
	// API says: "Returns a random collection of songs from the given artist and similar artists"
	const std::size_t similarArtistCount {count / 5};
	std::vector<ArtistId> artistIds {Service<Recommendation::IRecommendationService>::get()->getSimilarArtists(artistId, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, similarArtistCount)};
	artistIds.push_back(artistId);

	const std::size_t meanTrackCountPerArtist {(count / artistIds.size()) + 1};

	auto transaction {context.dbSession.createSharedTransaction()};

	std::vector<TrackId> tracks;
	tracks.reserve(count);

	for (const ArtistId id : artistIds)
	{
		Track::FindParameters params;
		params.setArtist(id);
		params.setRange({0, meanTrackCountPerArtist});
		params.setSortMethod(TrackSortMethod::Random);

		const auto artistTracks {Track::find(context.dbSession, params)};
		tracks.insert(std::end(tracks),
				std::begin(artistTracks.results),
				std::end(artistTracks.results));
	}

	return tracks;
}

static
std::vector<TrackId>
findSimilarSongs(RequestContext& context, ReleaseId releaseId, std::size_t count)
{
	// API says: "Returns a random collection of songs from the given artist and similar artists"
	// so let's extend this for release
	const std::size_t similarReleaseCount {count / 5};
	std::vector<ReleaseId> releaseIds {Service<Recommendation::IRecommendationService>::get()->getSimilarReleases(releaseId, similarReleaseCount)};
	releaseIds.push_back(releaseId);

	const std::size_t meanTrackCountPerRelease {(count / releaseIds.size()) + 1};

	auto transaction {context.dbSession.createSharedTransaction()};

	std::vector<TrackId> tracks;
	tracks.reserve(count);

	for (const ReleaseId id : releaseIds)
	{
		Track::FindParameters params;
		params.setRelease(id);
		params.setRange({0, meanTrackCountPerRelease});
		params.setSortMethod(TrackSortMethod::Random);

		const auto releaseTracks {Track::find(context.dbSession, params)};
		tracks.insert(std::end(tracks),
				std::begin(releaseTracks.results),
				std::end(releaseTracks.results));
	}

	return tracks;
}

static
std::vector<TrackId>
findSimilarSongs(RequestContext&, TrackId trackId, std::size_t count)
{
	return Service<Recommendation::IRecommendationService>::get()->findSimilarTracks({trackId}, count);
}

static
Response
handleGetSimilarSongsRequestCommon(RequestContext& context, bool id3)
{
	// Optional params
	std::size_t count {getParameterAs<std::size_t>(context.parameters, "count").value_or(50)};

	std::vector<TrackId> tracks;

	if (const auto artistId {getParameterAs<ArtistId>(context.parameters, "id")})
		tracks = findSimilarSongs(context, *artistId, count);
	else if (const auto releaseId {getParameterAs<ReleaseId>(context.parameters, "id")})
		tracks = findSimilarSongs(context, *releaseId, count);
	else if (const auto trackId {getParameterAs<TrackId>(context.parameters, "id")})
		tracks = findSimilarSongs(context, *trackId, count);
	else
		throw BadParameterGenericError {"id"};

	Random::shuffleContainer(tracks);

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& similarSongsNode {response.createNode(id3 ? "similarSongs2" : "similarSongs")};
	for (const TrackId trackId : tracks)
	{
		const Track::pointer track {Track::find(context.dbSession, trackId)};
		similarSongsNode.addArrayChild("song", createSongNode(track, context.dbSession, user));
	}

	return response;
}

static
Response
handleGetSimilarSongsRequest(RequestContext& context)
{
	return handleGetSimilarSongsRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetSimilarSongs2Request(RequestContext& context)
{
	return handleGetSimilarSongsRequestCommon(context, true /* id3 */);
}

static
Response::Node
tracklistToResponseNode(const TrackList::pointer& tracklist, Session&)
{
	Response::Node playlistNode;

	playlistNode.setAttribute("id", idToString(tracklist->getId()));
	playlistNode.setAttribute("name", tracklist->getName());
	playlistNode.setAttribute("songCount", tracklist->getCount());
	playlistNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(tracklist->getDuration()).count());
	playlistNode.setAttribute("public", tracklist->isPublic());
	playlistNode.setAttribute("created", reportedDummyDate);
	playlistNode.setAttribute("owner", tracklist->getUser()->getLoginName());

	return playlistNode;
}

static
Response
handleGetPlaylistRequest(RequestContext& context)
{
	// Mandatory params
	TrackListId trackListId {getMandatoryParameterAs<TrackListId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist {TrackList::find(context.dbSession, trackListId)};
	if (!tracklist)
		throw RequestedDataNotFoundError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node playlistNode {tracklistToResponseNode(tracklist, context.dbSession)};

	auto entries {tracklist->getEntries()};
	for (const TrackListEntry::pointer& entry : entries)
		playlistNode.addArrayChild("entry", createSongNode(entry->getTrack(), context.dbSession, user));

	response.addNode("playlist", playlistNode );

	return response;
}

static
Response
handleGetPlaylistsRequest(RequestContext& context)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& playlistsNode {response.createNode("playlists")};

	TrackList::FindParameters params;
	params.setUser(context.userId);
	params.setType(TrackListType::Playlist);

	auto tracklistIds {TrackList::find(context.dbSession, params)};
	for (const TrackListId trackListId : tracklistIds.results)
	{
		const TrackList::pointer trackList {TrackList::find(context.dbSession, trackListId)};
		playlistsNode.addArrayChild("playlist", tracklistToResponseNode(trackList, context.dbSession));
	}

	return response;
}

static
Response
handleUpdatePlaylistRequest(RequestContext& context)
{
	// Mandatory params
	TrackListId id {getMandatoryParameterAs<TrackListId>(context.parameters, "playlistId")};

	// Optional parameters
	auto name {getParameterAs<std::string>(context.parameters, "name")};
	auto isPublic {getParameterAs<bool>(context.parameters, "public")};

	std::vector<TrackId> trackIdsToAdd {getMultiParametersAs<TrackId>(context.parameters, "songIdToAdd")};
	std::vector<std::size_t> trackPositionsToRemove {getMultiParametersAs<std::size_t>(context.parameters, "songIndexToRemove")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::find(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist {TrackList::find(context.dbSession, id)};
	if (!tracklist
		|| tracklist->getUser() != user
		|| tracklist->getType() != TrackListType::Playlist)
	{
		throw RequestedDataNotFoundError {};
	}

	if (name)
		tracklist.modify()->setName(*name);

	if (isPublic)
		tracklist.modify()->setIsPublic(*isPublic);

	{
		// Remove from end to make indexes stable
		std::sort(std::begin(trackPositionsToRemove), std::end(trackPositionsToRemove), std::greater<std::size_t>());

		for (std::size_t trackPositionToRemove : trackPositionsToRemove)
		{
			auto entry {tracklist->getEntry(trackPositionToRemove)};
			if (entry)
				entry.remove();
		}
	}

	// Add tracks
	for (const TrackId trackIdToAdd : trackIdsToAdd)
	{
		Track::pointer track {Track::find(context.dbSession, trackIdToAdd)};
		if (!track)
			continue;

		context.dbSession.create<TrackListEntry>(track, tracklist);
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleNotImplemented(RequestContext&)
{
	throw NotImplementedGenericError {};
}

using RequestHandlerFunc = std::function<Response(RequestContext& context)>;
using CheckImplementedFunc = std::function<void()>;
struct RequestEntryPointInfo
{
	RequestHandlerFunc			func;
	EnumSet<UserType>			allowedUserTypes {UserType::DEMO, UserType::REGULAR, UserType::ADMIN};
	CheckImplementedFunc		checkFunc {};
};

static const std::unordered_map<std::string_view, RequestEntryPointInfo> requestEntryPoints
{
	// System
	{"/ping",		{handlePingRequest}},
	{"/getLicense",		{handleGetLicenseRequest}},

	// Browsing
	{"/getMusicFolders",	{handleGetMusicFoldersRequest}},
	{"/getIndexes",		{handleGetIndexesRequest}},
	{"/getMusicDirectory",	{handleGetMusicDirectoryRequest}},
	{"/getGenres",		{handleGetGenresRequest}},
	{"/getArtists",		{handleGetArtistsRequest}},
	{"/getArtist",		{handleGetArtistRequest}},
	{"/getAlbum",		{handleGetAlbumRequest}},
	{"/getSong",		{handleGetSongRequest}},
	{"/getVideos",		{handleNotImplemented}},
	{"/getArtistInfo",	{handleGetArtistInfoRequest}},
	{"/getArtistInfo2",	{handleGetArtistInfo2Request}},
	{"/getAlbumInfo",	{handleNotImplemented}},
	{"/getAlbumInfo2",	{handleNotImplemented}},
	{"/getSimilarSongs",	{handleGetSimilarSongsRequest}},
	{"/getSimilarSongs2",	{handleGetSimilarSongs2Request}},
	{"/getTopSongs",		{handleNotImplemented}},

	// Album/song lists
	{"/getAlbumList",	{handleGetAlbumListRequest}},
	{"/getAlbumList2",	{handleGetAlbumList2Request}},
	{"/getRandomSongs",	{handleGetRandomSongsRequest}},
	{"/getSongsByGenre",	{handleGetSongsByGenreRequest}},
	{"/getNowPlaying",	{handleNotImplemented}},
	{"/getStarred",		{handleGetStarredRequest}},
	{"/getStarred2",		{handleGetStarred2Request}},

	// Searching
	{"/search",		{handleNotImplemented}},
	{"/search2",		{handleSearch2Request}},
	{"/search3",		{handleSearch3Request}},

	// Playlists
	{"/getPlaylists",	{handleGetPlaylistsRequest}},
	{"/getPlaylist",		{handleGetPlaylistRequest}},
	{"/createPlaylist",	{handleCreatePlaylistRequest}},
	{"/updatePlaylist",	{handleUpdatePlaylistRequest}},
	{"/deletePlaylist",	{handleDeletePlaylistRequest}},

	// Media retrieval
	{"/hls",				{handleNotImplemented}},
	{"/getCaptions",		{handleNotImplemented}},
	{"/getLyrics",		{handleNotImplemented}},
	{"/getAvatar",		{handleNotImplemented}},

	// Media annotation
	{"/star",			{handleStarRequest}},
	{"/unstar",			{handleUnstarRequest}},
	{"/setRating",		{handleNotImplemented}},
	{"/scrobble",		{handleScrobble}},

	// Sharing
	{"/getShares",		{handleNotImplemented}},
	{"/createShares",	{handleNotImplemented}},
	{"/updateShare",		{handleNotImplemented}},
	{"/deleteShare",		{handleNotImplemented}},

	// Podcast
	{"/getPodcasts",			{handleNotImplemented}},
	{"/getNewestPodcasts",		{handleNotImplemented}},
	{"/refreshPodcasts",		{handleNotImplemented}},
	{"/createPodcastChannel",	{handleNotImplemented}},
	{"/deletePodcastChannel",	{handleNotImplemented}},
	{"/deletePodcastEpisode",	{handleNotImplemented}},
	{"/downloadPodcastEpisode",	{handleNotImplemented}},

	// Jukebox
	{"/jukeboxControl",	{handleNotImplemented}},

	// Internet radio
	{"/getInternetRadioStations",	{handleNotImplemented}},
	{"/createInternetRadioStation",	{handleNotImplemented}},
	{"/updateInternetRadioStation",	{handleNotImplemented}},
	{"/deleteInternetRadioStation",	{handleNotImplemented}},

	// Chat
	{"/getChatMessages",	{handleNotImplemented}},
	{"/addChatMessages",	{handleNotImplemented}},

	// User management
	{"/getUser",			{handleGetUserRequest}},
	{"/getUsers",		{handleGetUsersRequest,			{UserType::ADMIN}}},
	{"/createUser",		{handleCreateUserRequest,		{UserType::ADMIN}, &Utils::checkSetPasswordImplemented}},
	{"/updateUser",		{handleUpdateUserRequest,		{UserType::ADMIN}}},
	{"/deleteUser",		{handleDeleteUserRequest,		{UserType::ADMIN}}},
	{"/changePassword",	{handleChangePassword,			{UserType::REGULAR, UserType::ADMIN}, &Utils::checkSetPasswordImplemented}},

	// Bookmarks
	{"/getBookmarks",	{handleGetBookmarks}},
	{"/createBookmark",	{handleCreateBookmark}},
	{"/deleteBookmark",	{handleDeleteBookmark}},
	{"/getPlayQueue",	{handleNotImplemented}},
	{"/savePlayQueue",	{handleNotImplemented}},

	// Media library scanning
	{"/getScanStatus",	{Scan::handleGetScanStatus,		{UserType::ADMIN}}},
	{"/startScan",		{Scan::handleStartScan,			{UserType::ADMIN}}},
};

using MediaRetrievalHandlerFunc = std::function<void(RequestContext&, const Wt::Http::Request&, Wt::Http::Response&)>;
static std::unordered_map<std::string, MediaRetrievalHandlerFunc> mediaRetrievalHandlers
{
	// Media retrieval
	{"/download",		handleDownload},
	{"/stream",			handleStream},
	{"/getCoverArt",	handleGetCoverArt},
};

void
SubsonicResource::handleRequest(const Wt::Http::Request &request, Wt::Http::Response &response)
{
	static std::atomic<std::size_t> curRequestId {};

	const std::size_t requestId {curRequestId++};

	LMS_LOG(API_SUBSONIC, DEBUG) << "Handling request " << requestId << " '" << request.pathInfo() << "', continuation = " << (request.continuation() ? "true" : "false") << ", params = " << parameterMapToDebugString(request.getParameterMap());

	std::string requestPath {request.pathInfo()};
	if (StringUtils::stringEndsWith(requestPath, ".view"))
		requestPath.resize(requestPath.length() - 5);

	// Optional parameters
	const ResponseFormat format {getParameterAs<std::string>(request.getParameterMap(), "f").value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml};

	ProtocolVersion protocolVersion {defaultServerProtocolVersion};

	try
	{
		// We need to parse client a soon as possible to make sure to answer with the right protocol version
		protocolVersion = getServerProtocolVersion(getMandatoryParameterAs<std::string>(request.getParameterMap(), "c"));
		RequestContext requestContext {buildRequestContext(request)};

		auto itEntryPoint {requestEntryPoints.find(requestPath)};
		if (itEntryPoint != requestEntryPoints.end())
		{
			if (itEntryPoint->second.checkFunc)
				itEntryPoint->second.checkFunc();

			checkUserTypeIsAllowed(requestContext, itEntryPoint->second.allowedUserTypes);

			Response resp {(itEntryPoint->second.func)(requestContext)};

			resp.write(response.out(), format);
			response.setMimeType(ResponseFormatToMimeType(format));

			LMS_LOG(API_SUBSONIC, DEBUG) << "Request " << requestId << " '" << requestPath << "' handled!";
			return;
		}

		auto itStreamHandler {mediaRetrievalHandlers.find(requestPath)};
		if (itStreamHandler != mediaRetrievalHandlers.end())
		{
			itStreamHandler->second(requestContext, request, response);
			LMS_LOG(API_SUBSONIC, DEBUG) << "Request " << requestId  << " '" << requestPath << "' handled!";
			return;
		}

		LMS_LOG(API_SUBSONIC, ERROR) << "Unhandled command '" << requestPath << "'";
		throw UnknownEntryPointGenericError {};
	}
	catch (const Error& e)
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Error while processing request '" << requestPath << "'"
			<< ", params = [" << parameterMapToDebugString(request.getParameterMap()) << "]"
			<< ", code = " << static_cast<int>(e.getCode()) << ", msg = '" << e.getMessage() << "'";
		Response resp {Response::createFailedResponse(protocolVersion, e)};
		resp.write(response.out(), format);
		response.setMimeType(ResponseFormatToMimeType(format));
	}
}

ProtocolVersion
SubsonicResource::getServerProtocolVersion(const std::string& clientName) const
{
	auto it {_serverProtocolVersionsByClient.find(clientName)};
	if (it == std::cend(_serverProtocolVersionsByClient))
		return defaultServerProtocolVersion;

	return it->second;
}

void
SubsonicResource::checkProtocolVersion(ProtocolVersion client, ProtocolVersion server)
{
	if (client.major > server.major)
		throw ServerMustUpgradeError {};
	if (client.major < server.major)
		throw ClientMustUpgradeError {};
	if (client.minor > server.minor)
		throw ServerMustUpgradeError {};
	else if (client.minor == server.minor)
	{
		if (client.patch > server.patch)
			throw ServerMustUpgradeError {};
	}
}

ClientInfo
SubsonicResource::getClientInfo(const Wt::Http::ParameterMap& parameters)
{
	ClientInfo res;

	if (hasParameter(parameters, "t"))
		throw TokenAuthenticationNotSupportedForLDAPUsersError {};

	// Mandatory parameters
	res.name = getMandatoryParameterAs<std::string>(parameters, "c");
	res.version = getMandatoryParameterAs<ProtocolVersion>(parameters, "v");
	res.user = getMandatoryParameterAs<std::string>(parameters, "u");
	res.password = decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(parameters, "p"));

	return res;
}

RequestContext
SubsonicResource::buildRequestContext(const Wt::Http::Request& request)
{
	const Wt::Http::ParameterMap& parameters {request.getParameterMap()};
	const ClientInfo clientInfo {getClientInfo(parameters)};
	const Database::UserId userId {authenticateUser(request, clientInfo)};

	return {parameters, _db.getTLSSession(), userId, clientInfo, getServerProtocolVersion(clientInfo.name)};
}

Database::UserId
SubsonicResource::authenticateUser(const Wt::Http::Request& request, const ClientInfo& clientInfo)
{
	if (auto *authEnvService {Service<::Auth::IEnvService>::get()})
	{
		const auto checkResult {authEnvService->processRequest(request)};
		if (checkResult.state != ::Auth::IEnvService::CheckResult::State::Granted)
			throw UserNotAuthorizedError {};

		return *checkResult.userId;
	}
	else if (auto *authPasswordService {Service<::Auth::IPasswordService>::get()})
	{
		const auto checkResult {authPasswordService->checkUserPassword(boost::asio::ip::address::from_string(request.clientAddress()),
																			clientInfo.user, clientInfo.password)};

		switch (checkResult.state)
		{
			case Auth::IPasswordService::CheckResult::State::Granted:
				return *checkResult.userId;
				break;
			case Auth::IPasswordService::CheckResult::State::Denied:
				throw WrongUsernameOrPasswordError {};
			case Auth::IPasswordService::CheckResult::State::Throttled:
				throw LoginThrottledGenericError {};
		}
	}

	throw InternalErrorGenericError {"No service avalaible to authenticate user"};
}

} // namespace api::subsonic

