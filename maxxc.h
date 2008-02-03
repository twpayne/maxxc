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

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define R 6371.0

#define DIE(syscall, _errno) die(__FILE__, __LINE__, __FUNCTION__, (syscall), (_errno))
#define ABORT() die(__FILE__, __LINE__, __FUNCTION__, 0, -1)

void error(const char *, ...) __attribute__ ((noreturn, format(printf, 1, 2)));
void die(const char *, int, const char *, const char *, int) __attribute__ ((noreturn));
void *alloc(int) __attribute__ ((malloc));

typedef enum { fix_none, fix_2d, fix_3d, fix_dgps, fix_pps } fix_t;

typedef struct {
    int lat;
    int lon;
    time_t time;
    int ele;
    char *name;
    fix_t fix;
} waypoint_t;

typedef struct {
    char *name;
    double distance;
    double multiplier;
    int circuit;
    int declared;
    int n;
    waypoint_t waypoints[8];
} route_t;

typedef struct {
    int n;
    route_t routes[8];
} result_t;

void result_write_gpx(const result_t *, FILE *);

typedef struct {
    time_t time;
    int lat;
    int lon;
    int val;
    int alt;
    int ele;
    char *name;
} trkpt_t;

void trkpt_to_waypoint(const trkpt_t *, waypoint_t *);

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
    int n;
    int capacity;
    trkpt_t *trkpts;
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
double track_open_distance(const track_t *, double, int *);
double track_open_distance_one_point(const track_t *, double, int *);
double track_open_distance_two_points(const track_t *, double, int *);
double track_open_distance_three_points(const track_t *, double, int *);
double frcfd_open_distance_out_and_return(const track_t *, double, int *);
double frcfd_open_distance_triangle(const track_t *, double, int *);

track_t *track_igc_new(FILE *) __attribute__ ((malloc));

#endif
