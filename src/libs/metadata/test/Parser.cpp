/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <vector>

#include <gtest/gtest.h>

#include <Wt/WTime.h>

#include "Parser.hpp"
#include "TestTagReader.hpp"

namespace lms::metadata
{
    TEST(Parser, generalTest)
    {
        Parser parser;
        TestTagReader testTags{
            {
                { TagType::AcoustID, { "e987a441-e134-4960-8019-274eddacc418" } },
                { TagType::Advisory, { "2" } },
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumSortOrder, { "MyAlbumSortName" } },
                { TagType::Artist, { "MyArtist1 & MyArtist2" } },
                { TagType::Artists, { "MyArtist1", "MyArtist2" } },
                { TagType::ArtistSortOrder, { "MyArtist1SortName", "MyArtist2SortName" } },
                { TagType::AlbumArtist, { "MyAlbumArtist1 & MyAlbumArtist2" } },
                { TagType::AlbumArtists, { "MyAlbumArtist1", "MyAlbumArtist2" } },
                { TagType::AlbumArtistsSortOrder, { "MyAlbumArtist1SortName", "MyAlbumArtist2SortName" } },
                { TagType::AlbumComment, { "MyAlbumComment" } },
                { TagType::Barcode, { "MyBarcode" } },
                { TagType::Comment, { "Comment1", "Comment2" } },
                { TagType::Compilation, { "1" } },
                { TagType::Composer, { "MyComposer1", "MyComposer2" } },
                { TagType::ComposerSortOrder, { "MyComposerSortOrder1", "MyComposerSortOrder2" } },
                { TagType::Conductor, { "MyConductor1", "MyConductor2" } },
                { TagType::Copyright, { "MyCopyright" } },
                { TagType::CopyrightURL, { "MyCopyrightURL" } },
                { TagType::Date, { "2020/03/04" } },
                { TagType::DiscNumber, { "2" } },
                { TagType::DiscSubtitle, { "MySubtitle" } },
                { TagType::Genre, { "Genre1", "Genre2" } },
                { TagType::Grouping, { "Grouping1", "Grouping2" } },
                { TagType::Media, { "CD" } },
                { TagType::Mixer, { "MyMixer1", "MyMixer2" } },
                { TagType::Mood, { "Mood1", "Mood2" } },
                { TagType::MusicBrainzArtistID, { "9d2e0c8c-8c5e-4372-a061-590955eaeaae", "5e2cf87f-c8d7-4504-8a86-954dc0840229" } },
                { TagType::MusicBrainzTrackID, { "0afb190a-6735-46df-a16d-199f48206e4a" } },
                { TagType::MusicBrainzReleaseArtistID, { "6fbf097c-1487-43e8-874b-50dd074398a7", "5ed3d6b3-2aed-4a03-828c-3c4d4f7406e1" } },
                { TagType::MusicBrainzReleaseID, { "3fa39992-b786-4585-a70e-85d5cc15ef69" } },
                { TagType::MusicBrainzReleaseGroupID, { "5b1a5a44-8420-4426-9b86-d25dc8d04838" } },
                { TagType::MusicBrainzRecordingID, { "bd3fc666-89de-4ac8-93f6-2dbf028ad8d5" } },
                { TagType::Producer, { "MyProducer1", "MyProducer2" } },
                { TagType::Remixer, { "MyRemixer1", "MyRemixer2" } },
                { TagType::RecordLabel, { "Label1", "Label2" } },
                { TagType::Language, { "Language1", "Language2" } },
                { TagType::Lyricist, { "MyLyricist1", "MyLyricist2" } },
                { TagType::OriginalReleaseDate, { "2019/02/03" } },
                { TagType::ReleaseType, { "Album", "Compilation" } },
                { TagType::ReplayGainTrackGain, { "-0.33" } },
                { TagType::ReplayGainAlbumGain, { "-0.5" } },
                { TagType::TrackTitle, { "MyTitle" } },
                { TagType::TrackNumber, { "7" } },
                { TagType::TotalTracks, { "12" } },
                { TagType::TotalDiscs, { "3" } },
            }
        };
        testTags.setExtraUserTags({ { "MY_AWESOME_TAG_A", { "MyTagValue1ForTagA", "MyTagValue2ForTagA" } },
            { "MY_AWESOME_TAG_B", { "MyTagValue1ForTagB", "MyTagValue2ForTagB" } } });
        testTags.setPerformersTags({ { "RoleA", { "MyPerformer1ForRoleA", "MyPerformer2ForRoleA" } },
            { "RoleB", { "MyPerformer1ForRoleB", "MyPerformer2ForRoleB" } } });
        testTags.setLyricsTags({ { "eng", "[00:00.00]First line\n[00:01.00]Second line" } });

        static_cast<IParser&>(parser).setUserExtraTags(std::vector<std::string>{ "MY_AWESOME_TAG_A", "MY_AWESOME_TAG_B", "MY_AWESOME_MISSING_TAG" });

        const std::unique_ptr<Track> track{ parser.parse(testTags) };

        // Audio properties
        {
            const AudioProperties& audioProperties{ testTags.getAudioProperties() };
            EXPECT_EQ(track->audioProperties.bitrate, audioProperties.bitrate);
            EXPECT_EQ(track->audioProperties.bitsPerSample, audioProperties.bitsPerSample);
            EXPECT_EQ(track->audioProperties.channelCount, audioProperties.channelCount);
            EXPECT_EQ(track->audioProperties.duration, audioProperties.duration);
            EXPECT_EQ(track->audioProperties.sampleRate, audioProperties.sampleRate);
        }

        EXPECT_EQ(track->acoustID, core::UUID::fromString("e987a441-e134-4960-8019-274eddacc418"));
        ASSERT_TRUE(track->advisory.has_value());
        EXPECT_EQ(track->advisory.value(), Track::Advisory::Clean);
        EXPECT_EQ(track->artistDisplayName, "MyArtist1 & MyArtist2");
        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "MyArtist1");
        EXPECT_EQ(track->artists[0].sortName, "MyArtist1SortName");
        EXPECT_EQ(track->artists[0].mbid, core::UUID::fromString("9d2e0c8c-8c5e-4372-a061-590955eaeaae"));
        EXPECT_EQ(track->artists[1].name, "MyArtist2");
        EXPECT_EQ(track->artists[1].sortName, "MyArtist2SortName");
        EXPECT_EQ(track->artists[1].mbid, core::UUID::fromString("5e2cf87f-c8d7-4504-8a86-954dc0840229"));
        ASSERT_EQ(track->comments.size(), 2);
        EXPECT_EQ(track->comments[0], "Comment1");
        EXPECT_EQ(track->comments[1], "Comment2");
        ASSERT_EQ(track->composerArtists.size(), 2);
        EXPECT_EQ(track->composerArtists[0].name, "MyComposer1");
        EXPECT_EQ(track->composerArtists[0].sortName, "MyComposerSortOrder1");
        EXPECT_EQ(track->composerArtists[1].name, "MyComposer2");
        EXPECT_EQ(track->composerArtists[1].sortName, "MyComposerSortOrder2");
        ASSERT_EQ(track->conductorArtists.size(), 2);
        EXPECT_EQ(track->conductorArtists[0].name, "MyConductor1");
        EXPECT_EQ(track->conductorArtists[1].name, "MyConductor2");
        EXPECT_EQ(track->copyright, "MyCopyright");
        EXPECT_EQ(track->copyrightURL, "MyCopyrightURL");
        ASSERT_TRUE(track->date.isValid());
        EXPECT_EQ(track->date.getYear(), 2020);
        EXPECT_EQ(track->date.getMonth(), 3);
        EXPECT_EQ(track->date.getDay(), 4);
        EXPECT_FALSE(track->hasCover);
        ASSERT_EQ(track->genres.size(), 2);
        EXPECT_EQ(track->genres[0], "Genre1");
        EXPECT_EQ(track->genres[1], "Genre2");
        ASSERT_EQ(track->groupings.size(), 2);
        EXPECT_EQ(track->groupings[0], "Grouping1");
        EXPECT_EQ(track->groupings[1], "Grouping2");
        ASSERT_EQ(track->languages.size(), 2);
        EXPECT_EQ(track->languages[0], "Language1");
        EXPECT_EQ(track->languages[1], "Language2");
        ASSERT_EQ(track->lyricistArtists.size(), 2);
        EXPECT_EQ(track->lyricistArtists[0].name, "MyLyricist1");
        EXPECT_EQ(track->lyricistArtists[1].name, "MyLyricist2");
        ASSERT_EQ(track->lyrics.size(), 1);
        EXPECT_EQ(track->lyrics.front().language, "eng");
        ASSERT_EQ(track->lyrics.front().synchronizedLines.size(), 2);
        ASSERT_TRUE(track->lyrics.front().synchronizedLines.contains(std::chrono::milliseconds{ 0 }));
        EXPECT_EQ(track->lyrics.front().synchronizedLines.find(std::chrono::milliseconds{ 0 })->second, "First line");
        ASSERT_TRUE(track->lyrics.front().synchronizedLines.contains(std::chrono::milliseconds{ 1000 }));
        EXPECT_EQ(track->lyrics.front().synchronizedLines.find(std::chrono::milliseconds{ 1000 })->second, "Second line");
        ASSERT_TRUE(track->mbid.has_value());
        EXPECT_EQ(track->mbid.value(), core::UUID::fromString("0afb190a-6735-46df-a16d-199f48206e4a"));
        ASSERT_EQ(track->mixerArtists.size(), 2);
        EXPECT_EQ(track->mixerArtists[0].name, "MyMixer1");
        EXPECT_EQ(track->mixerArtists[1].name, "MyMixer2");
        ASSERT_EQ(track->moods.size(), 2);
        EXPECT_EQ(track->moods[0], "Mood1");
        EXPECT_EQ(track->moods[1], "Mood2");
        ASSERT_TRUE(track->originalDate.isValid());
        EXPECT_EQ(track->originalDate.getYear(), 2019);
        EXPECT_EQ(track->originalDate.getMonth(), 2);
        EXPECT_EQ(track->originalDate.getDay(), 3);
        ASSERT_TRUE(track->originalYear.has_value());
        EXPECT_EQ(track->originalYear.value(), 2019);
        ASSERT_TRUE(track->performerArtists.contains("Rolea"));
        ASSERT_EQ(track->performerArtists["Rolea"].size(), 2);
        EXPECT_EQ(track->performerArtists["Rolea"][0].name, "MyPerformer1ForRoleA");
        EXPECT_EQ(track->performerArtists["Rolea"][1].name, "MyPerformer2ForRoleA");
        ASSERT_EQ(track->performerArtists["Roleb"].size(), 2);
        EXPECT_EQ(track->performerArtists["Roleb"][0].name, "MyPerformer1ForRoleB");
        EXPECT_EQ(track->performerArtists["Roleb"][1].name, "MyPerformer2ForRoleB");
        ASSERT_TRUE(track->position.has_value());
        EXPECT_EQ(track->position.value(), 7);
        ASSERT_EQ(track->producerArtists.size(), 2);
        EXPECT_EQ(track->producerArtists[0].name, "MyProducer1");
        EXPECT_EQ(track->producerArtists[1].name, "MyProducer2");
        ASSERT_TRUE(track->recordingMBID.has_value());
        EXPECT_EQ(track->recordingMBID.value(), core::UUID::fromString("bd3fc666-89de-4ac8-93f6-2dbf028ad8d5"));
        ASSERT_TRUE(track->replayGain.has_value());
        EXPECT_FLOAT_EQ(track->replayGain.value(), -0.33);
        ASSERT_EQ(track->remixerArtists.size(), 2);
        EXPECT_EQ(track->remixerArtists[0].name, "MyRemixer1");
        EXPECT_EQ(track->remixerArtists[1].name, "MyRemixer2");
        EXPECT_EQ(track->title, "MyTitle");
        ASSERT_EQ(track->userExtraTags["MY_AWESOME_TAG_A"].size(), 2);
        EXPECT_EQ(track->userExtraTags["MY_AWESOME_TAG_A"][0], "MyTagValue1ForTagA");
        EXPECT_EQ(track->userExtraTags["MY_AWESOME_TAG_A"][1], "MyTagValue2ForTagA");
        ASSERT_EQ(track->userExtraTags["MY_AWESOME_TAG_B"].size(), 2);
        EXPECT_EQ(track->userExtraTags["MY_AWESOME_TAG_B"][0], "MyTagValue1ForTagB");
        EXPECT_EQ(track->userExtraTags["MY_AWESOME_TAG_B"][1], "MyTagValue2ForTagB");

        // Medium
        ASSERT_TRUE(track->medium.has_value());
        EXPECT_EQ(track->medium->media, "CD");
        EXPECT_EQ(track->medium->name, "MySubtitle");
        ASSERT_TRUE(track->medium->position.has_value());
        EXPECT_EQ(track->medium->position.value(), 2);
        ASSERT_TRUE(track->medium->replayGain.has_value());
        EXPECT_FLOAT_EQ(track->medium->replayGain.value(), -0.5);
        ASSERT_TRUE(track->medium->trackCount.has_value());
        EXPECT_EQ(track->medium->trackCount.value(), 12);

        // Release
        ASSERT_TRUE(track->medium->release.has_value());
        const Release& release{ track->medium->release.value() };
        EXPECT_EQ(release.artistDisplayName, "MyAlbumArtist1 & MyAlbumArtist2");
        ASSERT_EQ(release.artists.size(), 2);
        EXPECT_EQ(release.artists[0].name, "MyAlbumArtist1");
        EXPECT_EQ(release.artists[0].sortName, "MyAlbumArtist1SortName");
        EXPECT_EQ(release.artists[0].mbid, core::UUID::fromString("6fbf097c-1487-43e8-874b-50dd074398a7"));
        EXPECT_EQ(release.artists[1].name, "MyAlbumArtist2");
        EXPECT_EQ(release.artists[1].sortName, "MyAlbumArtist2SortName");
        EXPECT_EQ(release.artists[1].mbid, core::UUID::fromString("5ed3d6b3-2aed-4a03-828c-3c4d4f7406e1"));
        EXPECT_TRUE(release.isCompilation);
        EXPECT_EQ(release.barcode, "MyBarcode");
        ASSERT_EQ(release.labels.size(), 2);
        EXPECT_EQ(release.labels[0], "Label1");
        EXPECT_EQ(release.labels[1], "Label2");
        ASSERT_TRUE(release.mbid.has_value());
        EXPECT_EQ(release.mbid.value(), core::UUID::fromString("3fa39992-b786-4585-a70e-85d5cc15ef69"));
        EXPECT_EQ(release.groupMBID.value(), core::UUID::fromString("5b1a5a44-8420-4426-9b86-d25dc8d04838"));
        EXPECT_EQ(release.mediumCount, 3);
        EXPECT_EQ(release.name, "MyAlbum");
        EXPECT_EQ(release.sortName, "MyAlbumSortName");
        EXPECT_EQ(release.comment, "MyAlbumComment");
        {
            std::vector<std::string> expectedReleaseTypes{ "Album", "Compilation" };
            EXPECT_EQ(release.releaseTypes, expectedReleaseTypes);
        }
    }

    TEST(Parser, trim)
    {
        const TestTagReader testTags{
            {
                { TagType::Genre, { "Genre1 ", " Genre2", " Genre3 " } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->genres.size(), 3);
        EXPECT_EQ(track->genres[0], "Genre1");
        EXPECT_EQ(track->genres[1], "Genre2");
        EXPECT_EQ(track->genres[2], "Genre3");
    }

    TEST(Parser, customDelimiters)
    {
        const TestTagReader testTags{
            {
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtist, { "AlbumArtist1 / AlbumArtist2" } },
                { TagType::Artist, { " Artist1 / Artist2 feat. Artist3  " } },
                { TagType::Genre, { "Genre1 ; Genre2" } },
                { TagType::Language, { " Lang1/Lang2 / Lang3" } },

            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setDefaultTagDelimiters(std::vector<std::string>{ " ; ", "/" });
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ " / ", " feat. " });
        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 3);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artists[2].name, "Artist3");
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2, Artist3"); // reconstruct artist display name since a custom delimiter is hit
        ASSERT_EQ(track->genres.size(), 2);
        EXPECT_EQ(track->genres[0], "Genre1");
        EXPECT_EQ(track->genres[1], "Genre2");
        ASSERT_EQ(track->languages.size(), 3);
        EXPECT_EQ(track->languages[0], "Lang1");
        EXPECT_EQ(track->languages[1], "Lang2");
        EXPECT_EQ(track->languages[2], "Lang3");

        // Medium
        ASSERT_TRUE(track->medium.has_value());

        // Release
        ASSERT_TRUE(track->medium->release.has_value());
        EXPECT_EQ(track->medium->release->name, "MyAlbum");
        EXPECT_EQ(track->medium->release->artists[0].name, "AlbumArtist1");
        EXPECT_EQ(track->medium->release->artists[1].name, "AlbumArtist2");
        EXPECT_EQ(track->medium->release->artistDisplayName, "AlbumArtist1, AlbumArtist2");
    }

    TEST(Parser, customDelimiters_foundInArtist)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1; Artist2" } },
                { TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ "; " });

        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2"); // reconstruct the display name since we hit a custom delimiter in Artist
    }

    TEST(Parser, customDelimiters_foundInArtists)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1 feat. Artist2" } },
                { TagType::Artists, { "Artist1; Artist2" } },
            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ "; " });

        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1 feat. Artist2");
    }

    TEST(Parser, customDelimiters_notUsed)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1 & Artist2" } },
                { TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ "; " });

        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1 & Artist2");
    }

    TEST(Parser, customDelimiters_onlyInArtist)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1 & Artist2" } },
            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ " & " });

        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2"); // reconstructed since a custom delimiter was hit for parsing
    }

    TEST(Parser, customDelimitersUsedForArtists)
    {
        const TestTagReader testTags{
            {
                { TagType::Artists, { "Artist1 & Artist2" } },
            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ " & " });

        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2"); // reconstructed since a custom delimiter was hit for parsing
    }

    TEST(Parser, noArtistInArtist)
    {
        const TestTagReader testTags{
            {
                // nothing in Artist!
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 0);
        EXPECT_EQ(track->artistDisplayName, "");
    }

    TEST(Parser, singleArtistInArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in Artist!
                { TagType::Artists, { "Artist1" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 1);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artistDisplayName, "Artist1");
    }

    TEST(Parser, multipleArtistsInArtist)
    {
        const TestTagReader testTags{
            {
                // nothing in Artists!
                { TagType::Artist, { "Artist1", "Artist2" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found
    }

    TEST(Parser, multipleArtistsInArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in Artist!
                { TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found and nothing is set in artist
    }

    TEST(Parser, multipleArtistsInArtistsWithEndDelimiter)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1 & (CV. Artist2)" } },
                { TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artistDisplayName, "Artist1 & (CV. Artist2)");
    }

    TEST(Parser, singleArtistInAlbumArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in AlbumArtist!
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtists, { "Artist1" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_TRUE(track->medium);
        ASSERT_TRUE(track->medium->release);
        ASSERT_EQ(track->medium->release->artists.size(), 1);
        EXPECT_EQ(track->medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track->medium->release->artistDisplayName, "Artist1");
    }

    TEST(Parser, multipleArtistsInAlbumArtist)
    {
        const TestTagReader testTags{
            {
                // nothing in AlbumArtists!
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtist, { "Artist1", "Artist2" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_TRUE(track->medium);
        ASSERT_TRUE(track->medium->release);
        ASSERT_EQ(track->medium->release->artists.size(), 2);
        EXPECT_EQ(track->medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track->medium->release->artists[1].name, "Artist2");
        EXPECT_EQ(track->medium->release->artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found
    }

    TEST(Parser, multipleArtistsInAlbumArtists_displayName)
    {
        const TestTagReader testTags{
            {
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtist, { "Artist1 & Artist2" } },
                { TagType::AlbumArtists, { "Artist1", "Artist2" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_TRUE(track->medium);
        ASSERT_TRUE(track->medium->release);
        ASSERT_EQ(track->medium->release->artists.size(), 2);
        EXPECT_EQ(track->medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track->medium->release->artists[1].name, "Artist2");
        EXPECT_EQ(track->medium->release->artistDisplayName, "Artist1 & Artist2");
    }

    TEST(Parser, multipleArtistsInAlbumArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in AlbumArtist!
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtists, { "Artist1", "Artist2" } },
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_TRUE(track->medium);
        ASSERT_TRUE(track->medium->release);
        ASSERT_EQ(track->medium->release->artists.size(), 2);
        EXPECT_EQ(track->medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track->medium->release->artists[1].name, "Artist2");
        EXPECT_EQ(track->medium->release->artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found and nothing is set in artist
    }

    TEST(Parser, multipleArtistsInArtistsButNotAllMBIDs)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1 & Artist2" } },
                { TagType::Artists, { "Artist1", "Artist2" } },
                { TagType::MusicBrainzArtistID, { "dd2180a2-a350-4012-b332-5d66102fa2c6" } }, // only one => no mbid will be added
            }
        };

        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[0].mbid, std::nullopt);
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artists[1].mbid, std::nullopt);
        EXPECT_EQ(track->artistDisplayName, "Artist1 & Artist2");
    }

    TEST(Parser, multipleArtistsInArtistsButNotAllMBIDs_customDelimiters)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1 / Artist2" } },
                { TagType::MusicBrainzArtistID, { "dd2180a2-a350-4012-b332-5d66102fa2c6" } }, // only one => no mbid will be added
            }
        };

        Parser parser;
        static_cast<IParser&>(parser).setArtistTagDelimiters(std::vector<std::string>{ " / " });
        std::unique_ptr<Track> track{ parser.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        EXPECT_EQ(track->artists[0].mbid, std::nullopt);
        EXPECT_EQ(track->artists[1].name, "Artist2");
        EXPECT_EQ(track->artists[1].mbid, std::nullopt);
        EXPECT_EQ(track->artistDisplayName, "Artist1, Artist2"); // reconstruct the artist display name
    }

    TEST(Parser, MBIDs_fallback)
    {
        TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1", "Artist2" } },
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtists, { "Artist3", "Artist4" } },
                { TagType::MusicBrainzArtistID, { "6643f584-5edc-45ce-927d-0a4ab25c2673", "481c5912-bf1a-47f7-b03c-d34e49711706" } },
                { TagType::MusicBrainzReleaseArtistID, { "ed42bcaf-e147-4f34-8f26-d74acc97670a", "6fc64a4b-26f5-441f-993c-fd511290233b" } },
                { TagType::Composer, { "Artist1", "Artist3" } },
                { TagType::Conductor, { "Artist1", "Artist3" } },
                { TagType::Lyricist, { "Artist1", "Artist3" } },
                { TagType::Mixer, { "Artist1", "Artist3" } },
                { TagType::Producer, { "Artist1", "Artist3" } },
                { TagType::Remixers, { "Artist1", "Artist3" } },
            }
        };

        testTags.setPerformersTags({ { "RoleA", { "Artist1", "Artist3" } },
            { "RoleB", { "Artist2", "Artist4" } } });
        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->artists.size(), 2);
        EXPECT_EQ(track->artists[0].name, "Artist1");
        ASSERT_TRUE(track->artists[0].mbid.has_value());
        EXPECT_EQ(track->artists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->artists[1].name, "Artist2");
        ASSERT_TRUE(track->artists[1].mbid.has_value());
        EXPECT_EQ(track->artists[1].mbid.value(), core::UUID::fromString("481c5912-bf1a-47f7-b03c-d34e49711706"));

        ASSERT_TRUE(track->medium.has_value());
        ASSERT_TRUE(track->medium->release.has_value());
        ASSERT_EQ(track->medium->release->artists.size(), 2);
        EXPECT_EQ(track->medium->release->artists[0].name, "Artist3");
        ASSERT_TRUE(track->medium->release->artists[0].mbid.has_value());
        EXPECT_EQ(track->medium->release->artists[0].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));
        EXPECT_EQ(track->medium->release->artists[1].name, "Artist4");
        ASSERT_TRUE(track->medium->release->artists[1].mbid.has_value());
        EXPECT_EQ(track->medium->release->artists[1].mbid.value(), core::UUID::fromString("6fc64a4b-26f5-441f-993c-fd511290233b"));

        ASSERT_EQ(track->composerArtists.size(), 2);
        EXPECT_EQ(track->composerArtists[0].name, "Artist1");
        ASSERT_TRUE(track->composerArtists[0].mbid.has_value());
        EXPECT_EQ(track->composerArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->composerArtists[1].name, "Artist3");
        ASSERT_TRUE(track->composerArtists[1].mbid.has_value());
        EXPECT_EQ(track->composerArtists[1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));

        ASSERT_EQ(track->conductorArtists.size(), 2);
        EXPECT_EQ(track->conductorArtists[0].name, "Artist1");
        ASSERT_TRUE(track->conductorArtists[0].mbid.has_value());
        EXPECT_EQ(track->conductorArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->conductorArtists[1].name, "Artist3");
        ASSERT_TRUE(track->conductorArtists[1].mbid.has_value());
        EXPECT_EQ(track->conductorArtists[1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));

        ASSERT_EQ(track->lyricistArtists.size(), 2);
        EXPECT_EQ(track->lyricistArtists[0].name, "Artist1");
        ASSERT_TRUE(track->lyricistArtists[0].mbid.has_value());
        EXPECT_EQ(track->lyricistArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->lyricistArtists[1].name, "Artist3");
        ASSERT_TRUE(track->lyricistArtists[1].mbid.has_value());
        EXPECT_EQ(track->lyricistArtists[1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));

        ASSERT_EQ(track->mixerArtists.size(), 2);
        EXPECT_EQ(track->mixerArtists[0].name, "Artist1");
        ASSERT_TRUE(track->mixerArtists[0].mbid.has_value());
        EXPECT_EQ(track->mixerArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->mixerArtists[1].name, "Artist3");
        ASSERT_TRUE(track->mixerArtists[1].mbid.has_value());
        EXPECT_EQ(track->mixerArtists[1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));

        ASSERT_EQ(track->producerArtists.size(), 2);
        EXPECT_EQ(track->producerArtists[0].name, "Artist1");
        ASSERT_TRUE(track->producerArtists[0].mbid.has_value());
        EXPECT_EQ(track->producerArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->producerArtists[1].name, "Artist3");
        ASSERT_TRUE(track->producerArtists[1].mbid.has_value());
        EXPECT_EQ(track->producerArtists[1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));

        ASSERT_EQ(track->remixerArtists.size(), 2);
        EXPECT_EQ(track->remixerArtists[0].name, "Artist1");
        ASSERT_TRUE(track->remixerArtists[0].mbid.has_value());
        EXPECT_EQ(track->remixerArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->remixerArtists[1].name, "Artist3");
        ASSERT_TRUE(track->remixerArtists[1].mbid.has_value());
        EXPECT_EQ(track->remixerArtists[1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));

        ASSERT_TRUE(track->performerArtists.contains("Rolea"));
        ASSERT_EQ(track->performerArtists["Rolea"].size(), 2);
        EXPECT_EQ(track->performerArtists["Rolea"][0].name, "Artist1");
        ASSERT_TRUE(track->performerArtists["Rolea"][0].mbid.has_value());
        EXPECT_EQ(track->performerArtists["Rolea"][0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
        EXPECT_EQ(track->performerArtists["Rolea"][1].name, "Artist3");
        ASSERT_TRUE(track->performerArtists["Rolea"][1].mbid.has_value());
        EXPECT_EQ(track->performerArtists["Rolea"][1].mbid.value(), core::UUID::fromString("ed42bcaf-e147-4f34-8f26-d74acc97670a"));
        ASSERT_EQ(track->performerArtists["Roleb"].size(), 2);
        EXPECT_EQ(track->performerArtists["Roleb"][0].name, "Artist2");
        ASSERT_TRUE(track->performerArtists["Roleb"][0].mbid.has_value());
        EXPECT_EQ(track->performerArtists["Roleb"][0].mbid.value(), core::UUID::fromString("481c5912-bf1a-47f7-b03c-d34e49711706"));
        EXPECT_EQ(track->performerArtists["Roleb"][1].name, "Artist4");
        ASSERT_TRUE(track->performerArtists["Roleb"][1].mbid.has_value());
        EXPECT_EQ(track->performerArtists["Roleb"][1].mbid.value(), core::UUID::fromString("6fc64a4b-26f5-441f-993c-fd511290233b"));
    }

    TEST(Parser, MBIDs_fallback_priority)
    {
        const TestTagReader testTags{
            {
                { TagType::Artist, { "Artist1" } },
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumArtists, { "Artist1" } },
                { TagType::MusicBrainzArtistID, { "6643f584-5edc-45ce-927d-0a4ab25c2673" } },
                { TagType::MusicBrainzReleaseArtistID, { "ed42bcaf-e147-4f34-8f26-d74acc97670a" } },
                { TagType::Composer, { "Artist1" } },
            }
        };
        std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

        ASSERT_EQ(track->composerArtists.size(), 1);
        EXPECT_EQ(track->composerArtists[0].name, "Artist1");
        ASSERT_TRUE(track->composerArtists[0].mbid.has_value());
        EXPECT_EQ(track->composerArtists[0].mbid.value(), core::UUID::fromString("6643f584-5edc-45ce-927d-0a4ab25c2673"));
    }

    TEST(Parser, advisory)
    {
        auto doTest = [](std::string_view value, std::optional<Track::Advisory> expectedValue) {
            const TestTagReader testTags{
                {
                    { TagType::Advisory, { value } },
                }
            };

            Parser parser;
            std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

            ASSERT_EQ(track->advisory.has_value(), expectedValue.has_value()) << "Value = '" << value << "'";
            if (track->advisory.has_value())
            {
                EXPECT_EQ(track->advisory.value(), expectedValue);
            }
        };

        doTest("0", Track::Advisory::Unknown);
        doTest("1", Track::Advisory::Explicit);
        doTest("4", Track::Advisory::Explicit);
        doTest("2", Track::Advisory::Clean);
        doTest("", std::nullopt);
        doTest("3", std::nullopt);
    }

    TEST(Parser, encodingTime)
    {
        auto doTest = [](std::string_view value, core::PartialDateTime expectedValue) {
            const TestTagReader testTags{
                {
                    { TagType::EncodingTime, { value } },
                }
            };

            Parser parser;
            std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

            ASSERT_EQ(track->encodingTime, expectedValue) << "Value = '" << value << "'";
        };

        doTest("", core::PartialDateTime{});
        doTest("foo", core::PartialDateTime{});
        doTest("2020-01-03T09:08:11.075", core::PartialDateTime{ 2020, 01, 03, 9, 8, 11 });
        doTest("2020-01-03", core::PartialDateTime{ 2020, 01, 03 });
        doTest("2020/01/03", core::PartialDateTime{ 2020, 01, 03 });
    }

    TEST(Parser, date)
    {
        auto doTest = [](std::string_view value, core::PartialDateTime expectedValue) {
            const TestTagReader testTags{
                {
                    { TagType::Date, { value } },
                }
            };

            Parser parser;
            std::unique_ptr<Track> track{ Parser{}.parse(testTags) };

            ASSERT_EQ(track->date, expectedValue) << "Value = '" << value << "'";
        };

        doTest("", core::PartialDateTime{});
        doTest("foo", core::PartialDateTime{});
        doTest("2020-01-03", core::PartialDateTime{ 2020, 01, 03 });
        doTest("2020-01", core::PartialDateTime{ 2020, 1 });
        doTest("2020", core::PartialDateTime{ 2020 });
        doTest("2020/01/03", core::PartialDateTime{ 2020, 01, 03 });
        doTest("2020/01", core::PartialDateTime{ 2020, 1 });
        doTest("2020", core::PartialDateTime{ 2020 });
    }
} // namespace lms::metadata
