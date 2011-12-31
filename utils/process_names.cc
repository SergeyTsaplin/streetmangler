/*
 * Copyright (C) 2011 Dmitry Marakasov
 *
 * This file is part of streetmangler.
 *
 * streetmangler is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * streetmangler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with streetmangler.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>

#include <stdlib.h>

#include <streetmangler/locale.hh>
#include <streetmangler/database.hh>
#include <streetmangler/stringlistparser.hh>

#include "osm_name_extractor.hh"

#include "name_aggregator.hh"

#ifndef DEFAULT_DATAFILE
#	define DEFAULT_DATAFILE "streets.txt"
#endif

#ifndef DEFAULT_LOCALE
#	define DEFAULT_LOCALE "ru_RU"
#endif

static const char* default_addr_tags[] = {
	"addr:street",
	"addr:street1",
	"addr:street2",
	"addr:street3",
	"addr2:street",
	"addr3:street",
};

static const char* default_name_tags[] = {
	"name",
};

class OsmNameProcessor : public OsmNameExtractor {
private:
	NameAggregator& aggregator_;

public:
	OsmNameProcessor(NameAggregator& aggregator) : aggregator_(aggregator) {
	}

	virtual void ProcessName(const std::string& name) {
		aggregator_.ProcessName(name);
	}
};

class TextfileNameProcessor : public StreetMangler::StringListParser {
private:
	NameAggregator& aggregator_;

public:
	TextfileNameProcessor(NameAggregator& aggregator) : aggregator_(aggregator) {
	}

protected:
	virtual void ProcessString(const std::string& string) {
		aggregator_.ProcessName(string);
	}
};

int usage(const char* progname, int code) {
	fprintf(stderr, "Usage: %s [-h] [-cdsAN] [-l locale] [-p depth] [[-a tag] ...] [[-n tag] ...] [[-f database] ...] file.osm|file.txt|- ...\n", progname);
	fprintf(stderr, "  -s  display per-street statistics (takes extra time)\n");
	fprintf(stderr, "  -d  dump street lists into dump.*\n");
	fprintf(stderr, "  -c  include dumps with street name counts\n\n");

	fprintf(stderr, "  -l  set locale (default \""DEFAULT_LOCALE"\")\n");
	fprintf(stderr, "  -p  spelling check distance (default 1)\n\n");

	fprintf(stderr, "  -f  specify pats to street names database (default "DEFAULT_DATAFILE")\n");
	fprintf(stderr, "      (may be specified more than once)\n\n");

	fprintf(stderr, "  -a  specify addr tag(s) instead of default set (\"addrN:streetN\" variants)\n");
	fprintf(stderr, "  -n  specify name tag(s) instead of default set (\"name\")\n");
	fprintf(stderr, "  -A  don't use default addr tags set\n");
	fprintf(stderr, "  -N  don't use default name tags set\n\n");

	fprintf(stderr, "  -h  display this help\n");
	return code;
}

int main(int argc, char** argv) {
	const char* progname = argv[0];
	const char* localename = DEFAULT_LOCALE;
	bool dumpflag = false;
	int flags = 0;
	int spelldistance = 1;
	bool use_default_addr_tags = true;
	bool use_default_name_tags = true;

	std::vector<const char*> datafiles;
	std::vector<const char*> name_tags;
	std::vector<const char*> addr_tags;

	/* process options */
	int c;
    while ((c = getopt(argc, argv, "sdhf:l:p:n:a:cNA")) != -1) {
		switch (c) {
			case 's': flags |= NameAggregator::PERSTREET_STATS; break;
			case 'd': dumpflag = true; break;
			case 'f': datafiles.push_back(optarg); break;
			case 'n': name_tags.push_back(optarg); break;
			case 'l': localename = optarg; break;
			case 'p': spelldistance = (int)strtoul(optarg, 0, 10); break;
			case 'a': addr_tags.push_back(optarg); break;
			case 'c': flags |= NameAggregator::COUNT_NAMES; break;
			case 'A': use_default_addr_tags = false; break;
			case 'N': use_default_name_tags = false; break;
			case 'h': return usage(progname, 0);
			default:
				return usage(progname, 1);
		}
	}

	/* if no databases were specified, use the default one */
	if (datafiles.empty())
		datafiles.push_back(DEFAULT_DATAFILE);

	if (addr_tags.empty() && use_default_addr_tags)
		for (const char** tag = default_addr_tags; tag < default_addr_tags + sizeof(default_addr_tags)/sizeof(default_addr_tags[0]); ++tag)
			addr_tags.push_back(*tag);

	if (name_tags.empty() && use_default_name_tags)
		for (const char** tag = default_name_tags; tag < default_name_tags + sizeof(default_name_tags)/sizeof(default_name_tags[0]); ++tag)
			name_tags.push_back(*tag);

	argc -= optind;
	argv += optind;

	/* there should be some input */
	if (argc < 1)
		return usage(progname, 1);

	/* setup and load the database */
	StreetMangler::Locale locale(localename);
	StreetMangler::Database database(locale);

	for (std::vector<const char*>::const_iterator i = datafiles.begin(); i != datafiles.end(); ++i) {
		fprintf(stderr, "Loading database \"%s\"...\n", *i);
		database.Load(*i);
	}

	/* create tag aggregator */
	NameAggregator aggregator(database, flags, spelldistance);

	OsmNameProcessor osm_processor(aggregator);

	for (std::vector<const char*>::const_iterator i = addr_tags.begin(); i != addr_tags.end(); ++i)
		osm_processor.AddAddrTag(*i);
	for (std::vector<const char*>::const_iterator i = name_tags.begin(); i != name_tags.end(); ++i)
		osm_processor.AddNameTag(*i);

	TextfileNameProcessor text_processor(aggregator);

	/* process all input files */
	for (int i = 0; i < argc; ++i) {
		std::string file(argv[i]);
		if (file == "-") {
			fprintf(stderr, "Processing stdin as OSM data...\n");
			osm_processor.ParseStdin();
		} else if (file.rfind(".osm") == file.length() - 4 || file == "-") {
			fprintf(stderr, "Processing file \"%s\" as OSM data...\n", file.c_str());
			osm_processor.ParseFile(file.c_str());
		} else if (file.rfind(".txt") == file.length() - 4) {
			fprintf(stderr, "Processing file \"%s\" as strings list...\n", file.c_str());
			text_processor.ParseFile(file.c_str());
		} else {
			fprintf(stderr, "%s: unknown format (we only support .osm and .txt)\n", file.c_str());
			return 1;
		}
	}

	/* produce aggregated dump and statistics */
	if (dumpflag) {
		fprintf(stderr, "Dumping data...\n");
		aggregator.DumpData();
	}

	aggregator.DumpStats();

	return 0;
}
