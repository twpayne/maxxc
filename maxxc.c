/*

   maxxc - maximise cross country flights
   Copyright (C) 2007  Tom Payne

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

#include <getopt.h>
#include "maxxc.h"
#include "frcfd.h"

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
    printf("%s - optimise cross country flights\n"
	    "Usage: %s [options] [filename]\n"
	    "Options:\n"
	    "\t-h, --help\t\tprint usage and exit\n"
	    "\t-o, --output=FILENAME\tset output filename (default is stdout)\n",
	    program_name, program_name);
}

    int
main(int argc, char *argv[])
{
    program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];

    setenv("TZ", "UTC", 1);
    tzset();

    const char *output_filename = 0;

    opterr = 0;
    while (1) {
	static struct option options[] = {
	    { "help",    no_argument,       0, 'h' },
	    { "output",  required_argument, 0, 'o' },
	    { 0,         0,                 0, 0 },
	};
	int c = getopt_long(argc, argv, ":ho:", options, 0);
	if (c == -1)
	    break;
	switch (c) {
	    case 'h':
		usage();
		return EXIT_SUCCESS;
	    case 'o':
		output_filename = optarg;
		break;
	    case ':':
		error("option '%c' requires and argument", optopt);
	    case '?':
		error("invalid option '%c'", optopt);
		break;
	}
    }

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
    track_t *track = track_new_from_igc(input);
    if (input != stdin)
	fclose(input);

    result_t result;
    frcfd_optimize(track, &result);

    FILE *output;
    if (!output_filename || !strcmp(output_filename, "-")) {
	output = stdout;
    } else {
	output = fopen(output_filename, "w");
	if (!output)
	    error("fopen: %s: %s", output_filename, strerror(errno));
    }
    result_write_gpx(&result, output);
    if (output != stdout)
	fclose(stdout);

    track_delete(track);

    return EXIT_SUCCESS;
}
