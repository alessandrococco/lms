/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#include <boost/program_options.hpp>

#include "cover/ICoverArtGrabber.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/StreamLogger.hpp"

static
void
dumpTrackCovers(Database::Session& session, CoverArt::ImageSize width)
{
	std::vector<Database::TrackId> trackIds;
	{
		auto transaction {session.createSharedTransaction()};
		trackIds = Database::Track::getAllIds(session);
	}

	for (const Database::TrackId trackId : trackIds)
	{
		std::cout << "Getting cover for track id " << trackId.toString() << std::endl;
		Service<CoverArt::IGrabber>::get()->getFromTrack(session, trackId, width);
	}
}


int main(int argc, char *argv[])
{
	try
	{
		namespace po = boost::program_options;

		// log to stdout
		Service<Logger> logger {std::make_unique<StreamLogger>(std::cout)};


		po::options_description desc{"Allowed options"};
        desc.add_options()
        ("help,h", "print usage message")
		("conf,c", po::value<std::string>()->default_value("/etc/lms.conf"), "LMS config file")
        ("default-cover,d", po::value<std::string>(), "Default cover path")
        ("tracks,t", "dump covers for tracks")
		("size,s", po::value<unsigned>()->default_value(512), "Requested cover size")
		("quality,q", po::value<unsigned>()->default_value(75), "JPEG quality (1-100)")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
		{
			std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

		Service<IConfig> config {createConfig(vm["conf"].as<std::string>())};

		Service<CoverArt::IGrabber> coverArtService {CoverArt::createGrabber(argv[0],
				vm["default-cover"].as<std::string>(),
				config->getULong("cover-max-cache-size", 30) * 1000 * 1000,
				config->getULong("cover-max-file-size", 10) * 1000 * 1000,
				config->getULong("cover-jpeg-quality", vm["quality"].as<unsigned>())
				)};

		Database::Db db {config->getPath("working-dir") / "lms.db"};
		Database::Session session {db};

		if (vm.count("tracks"))
			dumpTrackCovers(session, vm["size"].as<unsigned>());
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

