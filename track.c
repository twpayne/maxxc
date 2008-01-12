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

#define R 6371.0

    void
trkpt_to_waypoint(const trkpt_t *trkpt, waypoint_t *waypoint)
{
    waypoint->lat = trkpt->lat;
    waypoint->lon = trkpt->lon;
    waypoint->time = trkpt->time;
    waypoint->ele = trkpt->ele;
    waypoint->name = 0;
    waypoint->fix = trkpt->fix_validity == 'A' ? fix_3d : fix_2d;
}

    static inline double
track_delta(const track_t *track, int i, int j)
{
    const coord_t *coord_i = track->coords + i;
    const coord_t *coord_j = track->coords + j;
    double x = coord_i->sin_lat * coord_j->sin_lat + coord_i->cos_lat * coord_j->cos_lat * cos(coord_i->lon - coord_j->lon);
    return x < 1.0 ? R * acos(x) : 0.0;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_forward(const track_t *track, int i, double d)
{
    int step = (int) (d / track->max_delta);
    return step > 0 ? i + step : ++i;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_fast_forward(const track_t *track, int i, double d)
{
    double target = track->sigma_delta[i] + d;
    i = track_forward(track, i, d);
    if (i >= track->n)
	return i;
    while (1) {
	double error = target - track->sigma_delta[i];
	if (error <= 0.0)
	    return i;
	i = track_forward(track, i, error);
	if (i >= track->n)
	    return i;
    }
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_backward(const track_t *track, int i, double d)
{
    int step = (int) (d / track->max_delta);
    return step > 0 ? i - step : --i;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_fast_backward(const track_t *track, int i, double d)
{
    double target = track->sigma_delta[i] - d;
    i = track_backward(track, i, d);
    if (i < 0)
	return i;
    while (1) {
	double error = track->sigma_delta[i] - target;
	if (error <= 0.0)
	    return i;
	i = track_backward(track, i, error);
	if (i < 0)
	    return i;
    }
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_furthest_from(const track_t *track, int i, int begin, int end, double bound, double *out)
{
    int result = -1;
    for (int j = begin; j < end; ) {
	double d = track_delta(track, i, j);
	if (d > bound) {
	    bound = *out = d;
	    result = j;
	    ++j;
	} else {
	    j = track_fast_forward(track, j, bound - d);
	}
    }
    return result;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_nearest_to(const track_t *track, int i, int begin, int end, double bound, double *out)
{
    int result = -1;
    for (int j = begin; j < end; ) {
	double d = track_delta(track, i, j);
	if (d < bound) {
	    result = j;
	    bound = *out = d;
	    ++j;
	} else {
	    j = track_fast_forward(track, j, d - bound);
	}
    }
    return result;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_furthest_from2(const track_t *track, int i, int j, int begin, int end, double bound, double *out)
{
    int result = -1;
    for (int k = begin; k < end; ) {
	double d = track_delta(track, i, k) + track_delta(track, k, j);
	if (d > bound) {
	    result = k;
	    bound = *out = d;
	    ++k;
	} else {
	    k = track_fast_forward(track, k, (bound - d) / 2.0);
	}
    }
    return result;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_first_at_least(const track_t *track, int i, int begin, int end, double bound)
{
    for (int j = begin; j < end; ) {
	double d = track_delta(track, i, j);
	if (d > bound)
	    return j;
	j = track_fast_forward(track, j, bound - d);
    }
    return -1;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static inline int
track_last_at_least(const track_t *track, int i, int begin, int end, double bound)
{
    for (int j = end - 1; j >= begin; ) {
	double d = track_delta(track, i, j);
	if (d > bound)
	    return j;
	j = track_fast_backward(track, j, bound - d);
    }
    return -1;
} __attribute__ ((nonnull(1))) __attribute__ ((pure))

    static void
track_initialize(track_t *track)
{
    track->coords = alloc(track->n * sizeof(coord_t));
    for (int i = 0; i < track->n; ++i) {
	double lat = M_PI * track->trkpts[i].lat / (180 * 60000);
	double lon = M_PI * track->trkpts[i].lon / (180 * 60000);
	track->coords[i].sin_lat = sin(lat);
	track->coords[i].cos_lat = cos(lat);
	track->coords[i].lon = lon;
    }
    track->max_delta = 0.0;
    track->sigma_delta = alloc(track->n * sizeof(double));
    double sigma_delta = 0.0;
    for (int i = 1; i < track->n; ++i) {
	track->sigma_delta[i] = sigma_delta;
	sigma_delta += track_delta(track, i - 1, i);
    }
    track->before = alloc(track->n * sizeof(limit_t));
    track->before[0].index = 0;
    track->before[0].distance = 0.0;
    for (int i = 1; i < track->n; ++i)
	track->before[i].index = track_furthest_from(track, i, 0, i, track->before[i - 1].distance - track->max_delta, &track->before[i].distance);
    track->after = alloc(track->n * sizeof(limit_t));
    track->after[0].index = track_furthest_from(track, 0, 1, track->n, 0.0, &track->after[0].distance);
    for (int i = 1; i < track->n - 1; ++i)
	track->after[i].index = track_furthest_from(track, i, i + 1, track->n, track->after[i - 1].distance - track->max_delta, &track->after[i].distance);
    track->after[track->n - 1].index = track->n - 1;
    track->after[track->n - 1].distance = 0.0;
}

    static void
track_push_trkpt(track_t *track, const trkpt_t *trkpt)
{
    if (track->n == track->capacity) {
	track->capacity = track->capacity ? 2 * track->capacity : 16384;
	track->trkpts = realloc(track->trkpts, track->capacity * sizeof(trkpt_t));
	if (!track->trkpts)
	    DIE("realloc", errno);
    }
    track->trkpts[track->n] = *trkpt;
    ++track->n;
}

    track_t *
track_new_from_igc(FILE *file)
{
    track_t *track = alloc(sizeof(track_t));

    struct tm tm;
    memset(&tm, 0, sizeof tm);
    trkpt_t trkpt;
    memset(&trkpt, 0, sizeof trkpt);
    char record[1024];
    while (fgets(record, sizeof record, file)) {
	if (igc_record_parse_hfdte(record, &tm)) {
	    ;
	} else if (igc_record_parse_b(record, &tm, &trkpt)) {
	    track_push_trkpt(track, &trkpt);
	}
    }
    track_initialize(track);
    return track;
}

    void
track_delete(track_t *track)
{
    if (track) {
	free(track->trkpts);
	free(track->coords);
	free(track->sigma_delta);
	free(track->before);
	free(track->after);
	free(track);
    }
}

    double
track_open_distance(const track_t *track, double bound, int *indexes)
{
    indexes[0] = indexes[1] = -1;
    for (int start = 0; start < track->n - 1; ++start) {
	int finish = track_furthest_from(track, start, start + 1, track->n, bound, &bound);
	if (finish != -1) {
	    indexes[0] = start;
	    indexes[1] = finish;
	}
    }
    return bound;
}

    double
track_open_distance_one_point(const track_t *track, double bound, int *indexes)
{
    indexes[0] = indexes[1] = indexes[2] = -1;
    for (int tp1 = 1; tp1 < track->n - 1; ) {
        double total = track->before[tp1].distance + track->after[tp1].distance;
        if (total > bound) {
            indexes[0] = track->before[tp1].index;
            indexes[1] = tp1;
            indexes[2] = track->after[tp1].index;
            bound = total;
            ++tp1;
        } else {
            tp1 = track_fast_forward(track, tp1, 0.5 * (bound - total));
        }
    }
    return bound;
}


    double
track_open_distance_two_points(const track_t *track, double bound, int *indexes)
{
    indexes[0] = indexes[1] = indexes[2] = indexes[3] = -1;
    for (int tp1 = 1; tp1 < track->n - 2; ++tp1) {
	double leg1 = track->before[tp1].distance;
	double bound23 = bound - leg1;
	for (int tp2 = tp1 + 1; tp2 < track->n - 1; ) {
	    double leg23 = track_delta(track, tp1, tp2) + track->after[tp2].distance;
	    if (leg23 > bound23) {
		indexes[0] = track->before[tp1].index;
		indexes[1] = tp1;
		indexes[2] = tp2;
		indexes[3] = track->after[tp2].index;
		bound23 = leg23;
		++tp2;
	    } else {
		tp2 = track_fast_forward(track, tp2, 0.5 * (bound23 - leg23));
	    }
	}
	bound = leg1 + bound23;
    }
    return bound;
}

    double
track_open_distance_three_points(const track_t *track, double bound, int *indexes)
{
    indexes[0] = indexes[1] = indexes[2] = indexes[3] = indexes[4] = -1;
    for (int tp1 = 1; tp1 < track->n - 3; ++tp1) {
	double leg1 = track->before[tp1].distance;
	double bound234 = bound - leg1;
	for (int tp2 = tp1 + 1; tp2 < track->n - 2; ++tp2) {
	    double leg2 = track_delta(track, tp1, tp2);
	    double bound34 = bound234 - leg2;
	    for (int tp3 = tp2 + 1; tp3 < track->n - 1; ) {
		double legs34 = track_delta(track, tp2, tp3) + track->after[tp3].distance;
		if (legs34 > bound34) {
		    indexes[0] = track->before[tp1].index;
		    indexes[1] = tp1;
		    indexes[2] = tp2;
		    indexes[3] = tp3;
		    indexes[4] = track->after[tp3].index;
		    bound34 = legs34;
		    ++tp3;
		} else {
		    tp3 = track_fast_forward(track, tp3, 0.5 * (bound34 - legs34));
		}
	    }
	    bound234 = leg2 + bound34;
	}
	bound = leg1 + bound234;
    }
    return bound;
}

    double
track_out_and_return(const track_t *track, double bound, int *indexes)
{
    indexes[0] = indexes[1] = indexes[2] = indexes[3] = -1;
    for (int tp1 = 0; tp1 < track->n - 2; ++tp1) {
	int start = track->best_start[tp1];
	int finish = track->last_finish[start];
	if (finish < 0)
	    continue;
	double leg = 0.0;
	int tp2 = track_furthest_from(track, tp1, tp1 + 1, finish + 1, bound, &leg);
	if (tp2 >= 0) {
	    indexes[0] = start;
	    indexes[1] = tp1;
	    indexes[2] = tp2;
	    indexes[3] = finish;
	    bound = leg;
	}
    }
    return bound;
}

    double
track_triangle(const track_t *track, double bound, int *indexes)
{
    indexes[0] = indexes[1] = indexes[2] = indexes[3] = indexes[4] = -1;
    for (int tp1 = 0; tp1 < track->n - 1; ++tp1) {
	if (track->sigma_delta[track->n - 1] - track->sigma_delta[tp1] < bound)
	    break;
	int start = track->best_start[tp1];
	int finish = track->last_finish[start];
	if (finish < 0 || track->sigma_delta[finish] - track->sigma_delta[tp1] < bound)
	    continue;
	for (int tp3 = finish; tp3 > tp1 + 1; --tp3) {
	    double leg31 = track_delta(track, tp3, tp1);
	    double bound123 = bound - leg31;
	    double legs123 = 0.0;
	    int tp2 = track_furthest_from2(track, tp1, tp3, tp1 + 1, tp3, bound123, &legs123);
	    if (tp2 > 0) {
		bound = leg31 + legs123;
		indexes[0] = start;
		indexes[1] = tp1;
		indexes[2] = tp2;
		indexes[3] = tp3;
		indexes[4] = finish;
	    }
	}
    }
    return bound;
}
