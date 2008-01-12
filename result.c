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

    static const char *
fix_to_s(fix_t fix)
{
    switch (fix) {
	case fix_none: return "none";
	case fix_2d:   return "2d";
	case fix_3d:   return "3d";
	case fix_dgps: return "dgps";
	case fix_pps:  return "pps";
	default:       ABORT();
    }
}

    static void
waypoint_write_gpx(const waypoint_t *waypoint, FILE *file, const char *type)
{
    fprintf(file, "\t\t<%s lat=\"%.8f\" lon=\"%.8f\">\n", type, waypoint->lat / 60000.0, waypoint->lon / 60000.0);
    if (waypoint->fix != fix_none && waypoint->fix != fix_2d)
	fprintf(file, "\t\t\t<ele>%d</ele>\n", waypoint->ele);
    if (waypoint->time != (time_t) -1) {
	struct tm tm;
	if (!gmtime_r(&waypoint->time, &tm))
	    DIE("gmtime_r", errno);
	char time[32];
	if (!strftime(time, sizeof time, "%Y-%m-%dT%H:%M:%SZ", &tm))
	    DIE("strftime", errno);
	fprintf(file, "\t\t\t<time>%s</time>\n", time);
    }
    if (waypoint->name)
	fprintf(file, "\t\t\t<name>%s</name>\n", waypoint->name);
    fprintf(file, "\t\t\t<fix>%s</fix>\n", fix_to_s(waypoint->fix));
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
	waypoint_write_gpx(route->waypoints + i, file, "rtept");
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
