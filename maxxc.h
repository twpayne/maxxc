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

#ifndef MAXXC_H
#define MAXXC_H

#include <stdio.h>
#include <time.h>

#define R 6371.0

#define DIE(syscall, _errno) die(__FILE__, __LINE__, __FUNCTION__, (syscall), (_errno))
#define ABORT() die(__FILE__, __LINE__, __FUNCTION__, 0, -1)

void error(const char *, ...) __attribute__ ((noreturn, format(printf, 1, 2)));
void die(const char *, int, const char *, const char *, int) __attribute__ ((noreturn));
void *alloc(int) __attribute__ ((malloc));

typedef struct {
    int lat;
    int lon;
    time_t time;
    int val;
    int ele;
    char *name;
} wpt_t;

void wpt_delete(wpt_t *);

typedef struct {
    time_t time;
    int lat;
    int lon;
    int val;
    int alt;
    int ele;
} trkpt_t;

void trkpt_to_wpt(const trkpt_t *, wpt_t *);

typedef struct {
    const char *name;
    double distance;
    double multiplier;
    int circuit;
    int declared;
    int nwpts;
    int wpts_capacity;
    wpt_t *wpts;
} route_t;

void route_delete(route_t *);
void route_push_wpt(route_t *, const wpt_t *);
void route_push_trkpts(route_t *, const trkpt_t *, int, int *, const char **);

typedef struct {
    const char *league;
    int nroutes;
    int routes_capacity;
    route_t *routes;
} result_t;

result_t *result_new(const char *);
void result_delete(result_t *);
route_t *result_push_new_route(result_t *, const char *, double, double, int, int);
void result_write_gpx(const result_t *, FILE *);

typedef struct {
    double cos_lat;
    double sin_lat;
    double lon;
} coord_t;

typedef struct {
    int index;
    double distance;
} limit_t;

typedef struct {
    int ntrkpts;
    int trkpts_capacity;
    trkpt_t *trkpts;
    int ntask_wpts;
    int task_wpts_capacity;
    wpt_t *task_wpts;
    coord_t *coords;
    double max_delta;
    double *sigma_delta;
    limit_t *before;
    limit_t *after;
    int *last_finish;
    int *best_start;
} track_t;

track_t *track_new_from_igc(FILE *) __attribute__ ((malloc));
void track_compute_circuit_tables(track_t *, double);
void track_delete(track_t *);
result_t *track_optimize_frcfd(track_t *);

#endif
