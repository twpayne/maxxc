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

#include "maxxc.h"

    static void
wpt_write_gpx(const wpt_t *wpt, FILE *file, const char *type)
{
    fprintf(file, "\t\t<%s lat=\"%.8f\" lon=\"%.8f\">\n", type, wpt->lat / 60000.0, wpt->lon / 60000.0);
    if (wpt->val == 'A')
	fprintf(file, "\t\t\t<ele>%d</ele>\n", wpt->ele);
    if (wpt->time != (time_t) -1) {
	struct tm tm;
	if (!gmtime_r(&wpt->time, &tm))
	    DIE("gmtime_r", errno);
	char time[32];
	if (!strftime(time, sizeof time, "%Y-%m-%dT%H:%M:%SZ", &tm))
	    DIE("strftime", errno);
	fprintf(file, "\t\t\t<time>%s</time>\n", time);
    }
    if (wpt->name)
	fprintf(file, "\t\t\t<name>%s</name>\n", wpt->name);
    fprintf(file, "\t\t\t<fix>%s</fix>\n", wpt->val == 'A' ? "3d" : "2d");
    fprintf(file, "\t\t</%s>\n", type);
}

    static void
route_write_gpx(const route_t *route, FILE *file)
{
    fprintf(file, "\t<rte>\n");
    if (route->name)
	fprintf(file, "\t\t<name>%s</name>\n", route->name);
    fprintf(file, "\t\t<extensions>\n");
    fprintf(file, "\t\t\t<distance>%.3f</distance>\n", route->distance);
    fprintf(file, "\t\t\t<multiplier>%.1f</multiplier>\n", route->multiplier);
    fprintf(file, "\t\t\t<score>%.2f</score>\n", route->distance * route->multiplier);
    if (route->circuit)
	fprintf(file, "\t\t\t<circuit/>\n");
    if (route->declared)
	fprintf(file, "\t\t\t<declared/>\n");
    fprintf(file, "\t\t</extensions>\n");
    for (int i = 0; i < route->n; ++i)
	wpt_write_gpx(route->wpts + i, file, "rtept");
    fprintf(file, "\t</rte>\n");
}

    void
result_write_gpx(const result_t *result, FILE *file)
{
    fprintf(file, "<?xml version=\"1.0\"?>\n");
    fprintf(file, "<gpx version=\"1.1\" creator=\"http://maximumxc.com/\">\n");
#if 0
    fprintf(file, "\t<metadata>\n");
    fprintf(file, "\t\t<extensions>\n");
    fprintf(file, "\t\t\t<league></league>\n"); /* FIXME */
    fprintf(file, "\t\t</extensions>\n");
    fprintf(file, "\t</metadata>\n");
#endif
    for (int i = 0; i < result->n; ++i)
	route_write_gpx(result->routes + i, file);
    fprintf(file, "</gpx>\n");
}
