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

#include "frcfd.h"

    void
frcfd_optimize(track_t *track, result_t *result)
{
    track_compute_circuit_tables(track, 3.0 / R);
    result->n = 1;
    route_t *route = result->routes;
    route->name = "distance libre";
    int indexes[2];
    route->distance = track_open_distance(track, 0.0, indexes);
    route->multiplier = 1.0;
    route->declared = 0;
    route->circuit = 0;
    route->n = 2;
    for (int i = 0; i < 2; ++i)
	trkpt_to_waypoint(track->trkpts + indexes[i], route->waypoints + i);
    route->waypoints[0].name = "BD";
    route->waypoints[1].name = "BA";

}
