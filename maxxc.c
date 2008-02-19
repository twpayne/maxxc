/*

   maxxc - maximise cross country flights
   Copyright (C) 2008  Tom Payne

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "maxxc.h"

const char *program_name = 0;

    void
error(const char *message, ...)
{
    fprintf(stderr, "%s: ", program_name);
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    abort();
}

    void
die(const char *file, int line, const char *function, const char *message, int _errno)
{
    if (_errno)
	error("%s:%d: %s: %s: %s", file, line, function, message, strerror(_errno));
    else if (message)
	error("%s:%d: %s: %s", file, line, function, message);
    else
	error("%s:%d: %s", file, line, function);
}

    void *
alloc(int size)
{
    void *p = malloc(size);
    if (!p)
	DIE("malloc", errno);
    memset(p, 0, size);
    return p;
}

    static void
usage(void)
{
    printf("%s - maximise cross country flights\n"
	    "Usage: %s [options] [filename]\n"
	    "Options:\n"
	    "\t-h, --help\t\tprint usage and exit\n"
	    "\t-l, --league=LEAGUE\tset league\n"
	    "\t-c, --complexity=N\tset maximum flight complexity\n"
	    "\t-o, --output=FILENAME\tset output filename (default is stdout)\n"
	    "\t-i, --embed-igc\t\tembed IGC in output\n"
	    "\t-t, --embed-trk\t\tembed tracklog in output\n"
	    "Leagues:\n"
	    "\tfrcfd\tCoupe F\303\251d\303\251rale de Distance (France)\n"
	    "\tukxcl\tCross Country League (UK)\n"
	    "Complexities:\n"
	    "\t0\tOpen distance\n"
	    "\t1\tOpen distance via a turnpoint\n"
	    "\t2\tOpen distance via two turnpoints, Out-and-return\n"
	    "\t3\tOpen distance via three turnpoints, Flat triangle, FAI triangle\n"
	    "\t4\tQuadrilateral\n",
	    program_name, program_name);
}

    int
main(int argc, char *argv[])
{
    program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];

    setenv("TZ", "UTC", 1);
    tzset();

    const char *league = 0;
    int complexity = -1;
    const char *filename = 0;
    const char *output_filename = 0;
    int embed_trk = 0;
    int embed_igc = 0;

    opterr = 0;
    while (1) {
	static struct option options[] = {
	    { "help",       no_argument,       0, 'h' },
	    { "league",     required_argument, 0, 'l' },
	    { "complexity", required_argument, 0, 'c' },
	    { "output",     required_argument, 0, 'o' },
	    { "embed-igc",  no_argument,       0, 'i' },
	    { "embed-trk",  no_argument,       0, 't' },
	    { 0,            0,                 0, 0 },
	};
	int c = getopt_long(argc, argv, ":hl:c:o:it", options, 0);
	if (c == -1)
	    break;
	char *endptr = 0;
	switch (c) {
	    case 'c':
		errno = 0;
		complexity = strtol(optarg, &endptr, 10);
		if (errno || *endptr)
		    error("invalid integer value '%s'", optarg);
		break;
	    case 'h':
		usage();
		return EXIT_SUCCESS;
	    case 'i':
		embed_igc = 1;
		break;
	    case 'l':
		league = optarg;
		break;
	    case 'o':
		output_filename = optarg;
		break;
	    case 't':
		embed_trk = 1;
		break;
	    case ':':
		error("option '%c' requires and argument", optopt);
	    case '?':
		error("invalid option '%c'", optopt);
		break;
	}
    }

    result_t * (*track_optimize)(track_t *, int) = 0;
    if (!league)
	error("no league specified");
    else if (!strcmp(league, "frcfd"))
	track_optimize = track_optimize_frcfd;
    else if (!strcmp(league, "ukxcl"))
	track_optimize = track_optimize_ukxcl;
    else
	error("invalid league '%s'", optarg);

    const char *input_filename = 0;
    if (optind == argc)
	;
    else if (optind + 1 == argc)
	input_filename = argv[optind];
    else
	error("excess arguments on command line");

    FILE *input;
    if (!input_filename) {
	input = stdin;
    } else {
	input = fopen(input_filename, "r");
	if (!input)
	    error("fopen: %s: %s", input_filename, strerror(errno));
    }
    if (input_filename) {
	filename = strrchr(input_filename, '/');
	filename = filename ? filename + 1 : filename;
    } else {
	filename = "";
    }
    track_t *track = track_new_from_igc(filename, input);
    if (input != stdin)
	fclose(input);

    result_t *result = track_optimize(track, complexity);

    FILE *output;
    if (!output_filename || !strcmp(output_filename, "-")) {
	output = stdout;
    } else {
	output = fopen(output_filename, "w");
	if (!output)
	    error("fopen: %s: %s", output_filename, strerror(errno));
    }
    result_write_gpx(result, track, embed_igc, embed_trk, output);
    if (output != stdout)
	fclose(output);

    result_delete(result);
    track_delete(track);

    return EXIT_SUCCESS;
}
