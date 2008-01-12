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

    int
main(int argc, char *argv[])
{
    program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];

    setenv("TZ", "UTC", 1);
    tzset();

    track_t *track = track_new_from_igc(stdin);
    result_t result;
    frcfd_optimize(track, &result);
    result_write_gpx(&result, stdout);
    track_delete(track);

    return EXIT_SUCCESS;
}
