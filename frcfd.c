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

#include "frcfd.h"

    result_t *
track_optimize_frcfd(track_t *track)
{
    static char *names[] = { "BD", "B1", "B2", "B3", "B4" };
    static const char *last_name = "BA";

    result_t *result = result_new();

    track_compute_circuit_tables(track, 3.0 / R);
    int indexes[6];

    double distance;
    route_t *route;

    distance = track_open_distance(track, 0.0, indexes);
    route = result_push_new_route(result, "distance libre", distance, 1.0, 0, 0);
    route_push_trkpts(route, track->trkpts, 2, indexes, names, last_name);

    distance = track_open_distance_one_point(track, distance, indexes);
    if (indexes[0] != -1) {
	route = result_push_new_route(result, "distance libre avec un point de contournement", distance, 1.0, 0, 0);
	route_push_trkpts(route, track->trkpts, 3, indexes, names, last_name);
    }

    distance = track_open_distance_two_points(track, distance, indexes);
    if (indexes[0] != -1) {
	route = result_push_new_route(result, "distance libre avec deux points de contournement", distance, 1.0, 0, 0);
	route_push_trkpts(route, track->trkpts, 4, indexes, names, last_name);
    }

#if 0
    distance = track_frcfd_aller_retour(track, 15.0 / R, indexes);
    if (indexes[0] != -1) {
	route = result_push_new_route(result, "parcours en aller-retour", distance, 1.2, 1, 0);
	route_push_trkpts(route, track->trkpts, 4, indexes, names, last_name);
    }

    distance = track_frcfd_triangle_plat(track, distance, indexes);
    if (indexes[0] != -1) {
	route = result_push_new_route(result, "triangle plat", distance, 1.2, 1, 0);
	route_push_trkpts(route, track->trkpts, 5, indexes, names, last_name);
    }
#endif

    return result;
}
