#include <ruby.h>
#include <math.h>
#include <stdio.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#define p(rb_value) rb_funcall(rb_mKernel, rb_intern("p"), 1, (rb_value))

#define R 6371.0
#define CIRCUIT_WEIGHT 256.0

static VALUE id_alt;
static VALUE id_lat;
static VALUE id_lon;
static VALUE id_new;
static VALUE id_time;
static VALUE id_to_i;

typedef struct {
    double cos_lat;
    double sin_lat;
    double lon;
} fix_t;

typedef struct {
    int index;
    double distance;
} limit_t;

typedef struct {
    VALUE rb_league;
    VALUE rb_fixes;
    int n;
    fix_t *fixes;
    time_t *times;
    double *sigma_delta;
    limit_t *before;
    limit_t *after;
    int *last_finish;
    int *best_start;
    double max_delta;
} track_t;

typedef struct {
    double min;
    double max;
} bound_t;

static inline double track_delta(const track_t *track, int i, int j) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static inline int track_forward(const track_t *track, int i, double d) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static inline int track_fast_forward(const track_t *track, int i, double d) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static inline int track_backward(const track_t *track, int i, double d) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static inline int track_fast_backward(const track_t *track, int i, double d) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static inline int track_first_at_least(const track_t *track, int i, int begin, int end, double bound) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static inline int track_last_at_least(const track_t *track, int i, int begin, int end, double bound) __attribute__ ((nonnull(1))) __attribute__ ((pure));
static track_t *track_new(VALUE rb_league, VALUE rb_fixes) __attribute__ ((malloc));
static track_t *track_downsample(track_t *track, double threshold) __attribute__ ((malloc));
void Init_cxc(void);

#if 0
static void benchmark(const char *label, struct tms *buf)
{
    struct tms now;
    static int clk_tck = 0;
    if (!clk_tck)
        clk_tck = sysconf(_SC_CLK_TCK);
    times(&now);
    if (label) {
        double utime = now.tms_utime - buf->tms_utime;
        if (utime < 0)
            utime += (unsigned) -1;
        fprintf(stderr, "%s: cpu=%.3fs\n", label,  utime / clk_tck);
    }
    *buf = now;
}
#endif

static inline VALUE
rb_ary_push_unless_nil(VALUE rb_self, VALUE rb_value)
{
    if (rb_value != Qnil)
        rb_ary_push(rb_self, rb_value);
    return rb_self;
}

static inline double
track_delta(const track_t *track, int i, int j)
{
    const fix_t *fix_i = track->fixes + i;
    const fix_t *fix_j = track->fixes + j;
    double x = fix_i->sin_lat * fix_j->sin_lat + fix_i->cos_lat * fix_j->cos_lat * cos(fix_i->lon - fix_j->lon);
    return x < 1.0 ? acos(x) : 0.0;
}

static inline int
track_forward(const track_t *track, int i, double d)
{
    int step = (int) (d / track->max_delta);
    return step > 0 ? i + step : ++i;
}

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
}

static inline int
track_backward(const track_t *track, int i, double d)
{
    int step = (int) (d / track->max_delta);
    return step > 0 ? i - step : --i;
}

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
}

static inline int
track_furthest_from(const track_t *track, int i, int begin, int end, double bound, double *out)
{
    int result = -1, j;
    for (j = begin; j < end; ) {
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
}

static inline int
track_nearest_to(const track_t *track, int i, int begin, int end, double bound, double *out)
{
    int result = -1, j;
    for (j = begin; j < end; ) {
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
}

static inline int
track_furthest_from2(const track_t *track, int i, int j, int begin, int end, double bound, double *out)
{
    int result = -1, k;
    for (k = begin; k < end; ) {
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
}

static inline int
track_first_at_least(const track_t *track, int i, int begin, int end, double bound)
{
    int j;
    for (j = begin; j < end; ) {
        double d = track_delta(track, i, j);
        if (d > bound)
            return j;
        j = track_fast_forward(track, j, bound - d);
    }
    return -1;
}

static inline int
track_last_at_least(const track_t *track, int i, int begin, int end, double bound)
{
    int j;
    for (j = end - 1; j >= begin; ) {
        double d = track_delta(track, i, j);
        if (d > bound)
            return j;
        j = track_fast_backward(track, j, bound - d);
    }
    return -1;
}

static track_t *
track_new_common(track_t *track)
{
    /* Compute before lookup table */
    track->before = ALLOC_N(limit_t, track->n);
    track->before[0].index = 0;
    track->before[0].distance = 0.0;
    int i;
    for (i = 1; i < track->n; ++i)
        track->before[i].index = track_furthest_from(track, i, 0, i, track->before[i - 1].distance - track->max_delta, &track->before[i].distance);

    /* Compute after lookup table */
    track->after = ALLOC_N(limit_t, track->n);
    for (i = 0; i < track->n - 1; ++i)
        track->after[i].index = track_furthest_from(track, i, i + 1, track->n, track->after[i - 1].distance - track->max_delta, &track->after[i].distance);
    track->after[track->n - 1].index = track->n - 1;
    track->after[track->n - 1].distance = 0.0;

    return track;
}

static track_t *
track_new(VALUE rb_league, VALUE rb_fixes)
{
    Check_Type(rb_fixes, T_ARRAY);

    track_t *track = ALLOC(track_t);
    memset(track, 0, sizeof(track_t));
    track->rb_league = rb_league;
    track->rb_fixes = rb_fixes;
    track->n = RARRAY(rb_fixes)->len;

    /* Compute cos_lat, sin_lat and lon lookup tables */
    track->fixes = ALLOC_N(fix_t, track->n);
    track->times = ALLOC_N(time_t, track->n);
    int i;
    for (i = 0; i < track->n; ++i) {
        VALUE rb_fix = RARRAY(rb_fixes)->ptr[i];
        double lat = NUM2DBL(rb_funcall(rb_fix, id_lat, 0));
        track->fixes[i].cos_lat = cos(lat);
        track->fixes[i].sin_lat = sin(lat);
        track->fixes[i].lon = NUM2DBL(rb_funcall(rb_fix, id_lon, 0));
        track->times[i] = NUM2INT(rb_funcall(rb_funcall(rb_fix, id_time, 0), id_to_i, 0));
    }

    /* Compute max_delta and sigma_delta lookup table */
    track->max_delta = 0.0;
    track->sigma_delta = ALLOC_N(double, track->n);
    track->sigma_delta[0] = 0.0;
    for (i = 1; i < track->n; ++i) {
        double delta = track_delta(track, i - 1, i);
        track->sigma_delta[i] = track->sigma_delta[i - 1] + delta;
        if (delta > track->max_delta)
            track->max_delta = delta;
    }

    return track_new_common(track);
}

static track_t *
track_downsample(track_t *track, double threshold)
{
    track_t *result = ALLOC(track_t);
    memset(result, 0, sizeof(track_t));
    result->rb_league = track->rb_league;
    result->rb_fixes = Qnil;
    result->fixes = ALLOC_N(fix_t, track->n);
    result->times = ALLOC_N(time_t, track->n);
    result->max_delta = 0.0;
    result->sigma_delta = ALLOC_N(double, track->n);
    result->fixes[0] = track->fixes[0];
    result->times[0] = track->times[0];
    result->sigma_delta[0] = 0.0;
    result->n = 1;
    int i = 0, j;
    for (j = 1; j < track->n; ++j) {
        double delta = track_delta(track, i, j);
        if (delta > threshold) {
            result->fixes[result->n] = track->fixes[j];
            result->times[result->n] = track->times[j];
            result->sigma_delta[result->n] = result->sigma_delta[result->n - 1] + delta;
            if (delta > result->max_delta)
                result->max_delta = delta;
            ++result->n;
            i = j;
        }
    }
    return track_new_common(result);
}

static void
track_compute_circuit_tables(track_t *track, double circuit_bound)
{
    track->last_finish = ALLOC_N(int, track->n);
    track->best_start = ALLOC_N(int, track->n);
    int current_best_start = 0, i, j;
    for (i = 0; i < track->n; ++i) {
        for (j = track->n - 1; j >= i; ) {
            double error = track_delta(track, i, j);
            if (error < circuit_bound) {
                track->last_finish[i] = j;
                break;
            } else {
                j = track_fast_backward(track, j, error - circuit_bound);
            }
        }
        if (track->last_finish[i] > track->last_finish[current_best_start])
            current_best_start = i;
        if (track->last_finish[current_best_start] < i) {
            current_best_start = 0;
            for (j = 1; j <= i; ++j)
                if (track->last_finish[j] > track->last_finish[current_best_start])
                    current_best_start = j;
        }
        track->best_start[i] = current_best_start;
    }
}

static void
track_delete(track_t *track)
{
    if (track) {
        xfree(track->fixes);
        xfree(track->times);
        xfree(track->sigma_delta);
        xfree(track->before);
        xfree(track->after);
        xfree(track->last_finish);
        xfree(track->best_start);
        xfree(track);
    }
}

static void
track_indexes_to_times(const track_t *track, int n, const int *indexes, time_t *times)
{
    int i;
    for (i = 0; i < n; ++i)
        times[i] = indexes[i] == -1 ? -1 : track->times[indexes[i]];
}

static double
track_open_distance(const track_t *track, double bound, time_t *times)
{
    int indexes[2] = { -1, -1 };
    int start;
    for (start = 0; start < track->n - 1; ++start) {
        int finish = track_furthest_from(track, start, start + 1, track->n, bound, &bound);
        if (finish != -1) {
            indexes[0] = start;
            indexes[1] = finish;
        }
    }
    track_indexes_to_times(track, 2, indexes, times);
    return bound;
}

static double
track_open_distance_one_point(const track_t *track, double bound, time_t *times)
{
    int indexes[3] = { -1, -1, -1 };
    int tp1;
    for (tp1 = 1; tp1 < track->n - 1; ) {
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
    track_indexes_to_times(track, 3, indexes, times);
    return bound;
}

static double
track_open_distance_two_points(const track_t *track, double bound, time_t *times)
{
    int indexes[4] = { -1, -1, -1, -1 };
    int tp1, tp2;
    for (tp1 = 1; tp1 < track->n - 2; ++tp1) {
        double leg1 = track->before[tp1].distance;
        double bound23 = bound - leg1;
        for (tp2 = tp1 + 1; tp2 < track->n - 1; ) {
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
    track_indexes_to_times(track, 4, indexes, times);
    return bound;
}

static double
track_open_distance_three_points(const track_t *track, double bound, time_t *times)
{
    int indexes[5] = { -1, -1, -1, -1, -1 };
    int tp1, tp2, tp3;
    for (tp1 = 1; tp1 < track->n - 3; ++tp1) {
        double leg1 = track->before[tp1].distance;
        double bound234 = bound - leg1;
        for (tp2 = tp1 + 1; tp2 < track->n - 2; ++tp2) {
            double leg2 = track_delta(track, tp1, tp2);
            double bound34 = bound234 - leg2;
            for (tp3 = tp2 + 1; tp3 < track->n - 1; ) {
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
    track_indexes_to_times(track, 5, indexes, times);
    return bound;
}

static void
track_circuit_close(const track_t *track, int n, int *indexes, double circuit_bound)
{
    if (indexes[0] == -1)
        return;
    int start, finish;
    double bound = track_delta(track, indexes[1], indexes[0]) + CIRCUIT_WEIGHT * track_delta(track, indexes[0], indexes[n - 1]) + track_delta(track, indexes[n - 1], indexes[n - 2]);
    for (start = indexes[0]; start <= indexes[1]; ++start) {
        double leg1 = track_delta(track, indexes[1], start);
        for (finish = indexes[n - 1]; finish >= indexes[n - 2]; --finish) {
            double leg2 = track_delta(track, start, finish);
            if (leg2 < circuit_bound) {
                double leg3 = track_delta(track, finish, indexes[n - 2]);
                double score = leg1 + CIRCUIT_WEIGHT * leg2 + leg3;
                if (score < bound) {
                    indexes[0] = start;
                    indexes[n - 1] = finish;
                    bound = score;
                }
            }
        }
    }
}

static double
track_out_and_return(const track_t *track, double bound, time_t *times)
{
    int indexes[4] = { -1, -1, -1, -1 };
    int tp1;
    for (tp1 = 0; tp1 < track->n - 2; ++tp1) {
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
    track_circuit_close(track, 4, indexes, 3.0 / R);
    track_indexes_to_times(track, 4, indexes, times);
    return bound;
}

static double
track_triangle(const track_t *track, double bound, time_t *times)
{
    int indexes[5] = { -1, -1, -1, -1, -1 };
    int tp1;
    for (tp1 = 0; tp1 < track->n - 1; ++tp1) {
        if (track->sigma_delta[track->n - 1] - track->sigma_delta[tp1] < bound)
            break;
        int start = track->best_start[tp1];
        int finish = track->last_finish[start];
        if (finish < 0 || track->sigma_delta[finish] - track->sigma_delta[tp1] < bound)
            continue;
        int tp3;
        for (tp3 = finish; tp3 > tp1 + 1; --tp3) {
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
    track_circuit_close(track, 5, indexes, 3.0 / R);
    track_indexes_to_times(track, 5, indexes, times);
    return bound;
}

static double
track_triangle_fai(const track_t *track, double bound, time_t *times)
{
    int indexes[5] = { -1, -1, -1, -1, -1 };
    double legbound = 0.28 * bound;
    int tp1;
    for (tp1 = 0; tp1 < track->n - 2; ++tp1) {
        int start = track->best_start[tp1];
        int finish = track->last_finish[start];
        if (finish < 0)
            continue;
        int tp3first = track_first_at_least(track, tp1, tp1 + 2, finish + 1, legbound);
        if (tp3first < 0)
            continue;
        int tp3last = track_last_at_least(track, tp1, tp3first, finish + 1, legbound);
        if (tp3last < 0)
            continue;
        int tp3;
        for (tp3 = tp3last; tp3 >= tp3first; ) {
            double leg3 = track_delta(track, tp3, tp1);
            if (leg3 < legbound) {
                tp3 = track_fast_backward(track, tp3, legbound - leg3);
                continue;
            }
            double shortestlegbound = 0.28 * leg3 / 0.44;
            int tp2first = track_first_at_least(track, tp1, tp1 + 1, tp3 - 1, shortestlegbound);
            if (tp2first < 0) {
                --tp3;
                continue;
            }
            int tp2last = track_last_at_least(track, tp3, tp2first, tp3, shortestlegbound);
            if (tp2last < 0) {
                --tp3;
                continue;
            }
            double longestlegbound = 0.44 * leg3 / 0.28;
            int tp2;
            for (tp2 = tp2first; tp2 <= tp2last; ) {
                double d = 0.0;
                double leg1 = track_delta(track, tp1, tp2);
                if (leg1 < shortestlegbound)
                    d = shortestlegbound - leg1;
                if (leg1 > longestlegbound && leg1 - longestlegbound > d)
                    d = leg1 - longestlegbound;
                double leg2 = track_delta(track, tp2, tp3);
                if (leg2 < shortestlegbound && shortestlegbound - leg2 > d)
                    d = shortestlegbound - leg2;
                if (leg2 > longestlegbound && leg2 - longestlegbound > d)
                    d = leg2 - longestlegbound;
                if (d > 0.0) {
                    tp2 = track_fast_forward(track, tp2, d);
                    continue;
                }
                double total = leg1 + leg2 + leg3;
                double thislegbound = 0.28 * total;
                if (leg1 < thislegbound)
                    d = thislegbound - leg1;
                if (leg2 < thislegbound && thislegbound - leg2 > d)
                    d = thislegbound - leg2;
                if (leg3 < thislegbound && thislegbound - leg3 > d)
                    d = thislegbound - leg3;
                if (d > 0.0) {
                    tp2 = track_fast_forward(track, tp2, 0.5 * d);
                    continue;
                }
                if (total < bound) {
                    tp2 = track_fast_forward(track, tp2, 0.5 * (bound - total));
                    continue;
                }
                bound = total;
                legbound = thislegbound;
                indexes[0] = start;
                indexes[1] = tp1;
                indexes[2] = tp2;
                indexes[3] = tp3;
                indexes[4] = finish;
                ++tp2;
            }
            --tp3;
        }
    }
    track_circuit_close(track, 5, indexes, 3.0 / R);
    track_indexes_to_times(track, 5, indexes, times);
    return bound;
}

static double
track_quadrilateral(const track_t *track, double bound, time_t *times)
{
    int n = 0;
    int indexes[6] = { -1, -1, -1, -1, -1, -1 };
    int tp1;
    double legbound = 0.15 * bound;
    for (tp1 = 0; tp1 < track->n - 3; ++tp1) {
        int start = track->best_start[tp1];
        int finish = track->last_finish[start];
        if (finish < 0)
            continue;
        int tp4first = track_first_at_least(track, tp1, tp1 + 2, finish + 1, legbound);
        if (tp4first < 0)
            continue;
        int tp4last = track_last_at_least(track, tp1, tp4first, finish + 1, legbound);
        if (tp4last < 0)
            continue;
        int tp4;
        for (tp4 = tp4last; tp4 >= tp4first; ) {
            double leg4 = track_delta(track, tp4, tp1);
            if (leg4 < legbound) {
                tp4 = track_fast_backward(track, tp4, legbound - leg4);
                continue;
            }
            double shortestlegbound = 0.15 * leg4 / (1.0 - 3 * 0.15);
            int tp2first = track_first_at_least(track, tp1, tp1 + 1, tp4 - 1, shortestlegbound);
            if (tp2first < 0) {
                --tp4;
                continue;
            }
            int tp3last = track_last_at_least(track, tp4, tp2first + 1, tp4, shortestlegbound);
            if (tp3last < 0) {
                --tp4;
                continue;
            }
            int tp2last = track_last_at_least(track, tp4, tp2first + 1, tp3last - 1, shortestlegbound);
            if (tp2last < 0) {
                --tp4;
                continue;
            }
            double longestlegbound = (1.0 - 3 * 0.15) * leg4 / 0.15;
            int tp2;
            for (tp2 = tp2first; tp2 <= tp2last; ) {
                double leg1 = track_delta(track, tp1, tp2);
                double shortestlegbound2 = 0.15 * (leg1 + leg4) / (1.0 - 2 * 0.15);
                if (shortestlegbound2 > shortestlegbound)
                    shortestlegbound2 = shortestlegbound;
                double longestlegbound2 = (1.0 - 3 * 0.15) * (leg1 + leg4) / (2 * 0.15);
                if (longestlegbound2 < longestlegbound)
                    longestlegbound2 = longestlegbound;
                int tp3first = track_first_at_least(track, tp2, tp2 + 1, tp3last + 1, shortestlegbound2);
                if (tp3first < 0) {
                    ++tp2;
                    continue;
                }
                int tp3;
                for (tp3 = tp3last; tp3 >= tp3first; ) {
                    if (--n < 0) {
                        fprintf(stderr, "tp1=%4d tp2=%4d tp3=%4d tp4=%4d\r", tp1, tp2, tp3, tp4);
                        n = 50000;
                    }
                    double d = 0.0;
                    double leg2 = track_delta(track, tp2, tp3);
                    if (leg2 < shortestlegbound2)
                        d = shortestlegbound2 - leg2;
                    if (leg2 > longestlegbound2 && leg2 - longestlegbound2 > d)
                        d = leg2 - longestlegbound2;
                    double leg3 = track_delta(track, tp3, tp4);
                    if (leg3 < shortestlegbound2 && shortestlegbound2 - leg3 > d)
                        d = shortestlegbound2 - leg3;
                    if (leg3 > longestlegbound2 && leg3 - longestlegbound2 > d)
                        d = leg3 - longestlegbound2;
                    if (d > 0.0) {
                        tp3 = track_fast_backward(track, tp3, d);
                        continue;
                    }
                    double total = leg1 + leg2 + leg3 + leg4;
                    double thislegbound = 0.15 * total;
                    if (leg1 < thislegbound)
                        d = thislegbound - leg1;
                    if (leg2 < thislegbound && thislegbound - leg2 > d)
                        d = thislegbound - leg2;
                    if (leg3 < thislegbound && thislegbound - leg3 > d)
                        d = thislegbound - leg3;
                    if (leg4 < thislegbound && thislegbound - leg4 > d)
                        d = thislegbound - leg4;
                    if (d > 0.0) {
                        tp3 = track_fast_backward(track, tp3, 0.5 * d);
                        continue;
                    }
                    if (total < bound) {
                        tp3 = track_fast_backward(track, tp3, 0.5 * (bound - total));
                        continue;
                    }
                    bound = total;
                    legbound = thislegbound;
                    indexes[0] = start;
                    indexes[1] = tp1;
                    indexes[2] = tp2;
                    indexes[3] = tp3;
                    indexes[4] = tp4;
                    indexes[5] = finish;
                    --tp3;
                }
                ++tp2;
            }
            --tp4;
        }
    }
    track_circuit_close(track, 6, indexes, 3.0 / R);
    track_indexes_to_times(track, 6, indexes, times);
    return bound;
}

static int
track_time_to_index(const track_t *track, time_t time, int left, int right)
{
    while (left <= right) {
        int middle = (left + right) / 2;
        if (track->times[middle] > time)
            right = middle - 1;
        else if (track->times[middle] == time)
            return middle;
        else
            left = middle + 1;
    }
    return -1;
}

static VALUE
track_rb_new_xc(const track_t *track, const char *flight, int n, time_t *times)
{
    if (times[0] == -1)
        return Qnil;
    VALUE rb_fixes = rb_ary_new2(n);
    int left = 0;
    int i;
    for (i = 0; i < n; ++i) {
        int index = track_time_to_index(track, times[i], left, track->n);
        rb_ary_push(rb_fixes, RARRAY(track->rb_fixes)->ptr[index]);
        left = index;
    }
    return rb_funcall(rb_const_get(track->rb_league, rb_intern(flight)), id_new, 1, rb_fixes);
}

static VALUE
rb_XC_Open_optimize(VALUE rb_self, VALUE rb_fixes)
{
    VALUE rb_result = rb_ary_new2(1);
    track_t *track = track_new(rb_self, rb_fixes);
    time_t times[2];
    track_open_distance(track, 0.0, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open0", 2, times));
    track_delete(track);
    return rb_result;
}

static VALUE
rb_XC_FRCFD_optimize(VALUE rb_self, VALUE rb_fixes)
{
    VALUE rb_result = rb_ary_new2(7);
    track_t *track = track_new(rb_self, rb_fixes);
    time_t times[6];
    double bound = 0.0;
    bound = track_open_distance(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open0", 2, times));
    if (bound < 15.0 / R)
        bound = 15.0 / R;
    bound = track_open_distance_one_point(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open1", 3, times));
    bound = track_open_distance_two_points(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open2", 4, times));
    track_compute_circuit_tables(track, 3.0 / R);
    track_out_and_return(track, 15.0 / R, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Circuit2", 4, times));
    time_t times_fai[5] = { -1 };
    time_t downsampled_times_fai[5] = { -1 };
    time_t downsampled_times[5] = { -1 };
    track_t *downsampled_track = track_downsample(track, 0.5 / R);
    track_compute_circuit_tables(downsampled_track, 3.0 / R);
    bound = track_triangle_fai(downsampled_track, 15.0 / R, downsampled_times_fai);
    bound = track_triangle_fai(track, bound, times_fai);
    if (times_fai[0] == -1)
        memcpy(times_fai, downsampled_times_fai, sizeof times_fai);
    VALUE rb_circuit3fai = track_rb_new_xc(track, "Circuit3FAI", 5, times_fai);
    bound = track_triangle(downsampled_track, bound, downsampled_times);
    bound = track_triangle(track, bound, times);
    if (times[0] == -1)
        memcpy(times, downsampled_times[0] == -1 ? times_fai : downsampled_times, sizeof times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Circuit3", 5, times));
    rb_ary_push_unless_nil(rb_result, rb_circuit3fai);
#if 0
    bound = track_quadrilateral(downsampled_track, 15.0 / R, downsampled_times);
#if 0
    bound = track_quadrilateral(track, bound, times);
#else
    times[0] = -1;
#endif
    if (times[0] == -1)
        memcpy(times, downsampled_times, sizeof times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Circuit4", 6, times));
#endif
    track_delete(downsampled_track);
    track_delete(track);
    return rb_result;
}

static VALUE
rb_XC_UKXCL_optimize(VALUE rb_self, VALUE rb_fixes)
{
    VALUE rb_result = rb_ary_new2(4);
    track_t *track = track_new(rb_self, rb_fixes);
    time_t times[5];
    double bound = 0.0;
    bound = track_open_distance(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open0", 2, times));
    if (bound < 15.0 / R)
        bound = 15.0 / R;
    bound = track_open_distance_one_point(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open1", 3, times));
    bound = track_open_distance_two_points(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open2", 4, times));
    bound = track_open_distance_three_points(track, bound, times);
    rb_ary_push_unless_nil(rb_result, track_rb_new_xc(track, "Open3", 5, times));
    track_delete(track);
    return rb_result;
}

void
Init_cxc(void)
{
    id_alt = rb_intern("alt");
    id_lat = rb_intern("lat");
    id_lon = rb_intern("lon");
    id_new = rb_intern("new");
    id_time = rb_intern("time");
    id_to_i = rb_intern("to_i");
    VALUE rb_XC = rb_define_module("XC");
    VALUE rb_XC_League = rb_define_class_under(rb_XC, "League", rb_cObject);
    VALUE rb_XC_Open = rb_define_class_under(rb_XC, "Open", rb_XC_League);
    rb_define_module_function(rb_XC_Open, "optimize", rb_XC_Open_optimize, 1);
    VALUE rb_XC_FRCFD = rb_define_class_under(rb_XC, "FRCFD", rb_XC_League);
    rb_define_module_function(rb_XC_FRCFD, "optimize", rb_XC_FRCFD_optimize, 1);
    VALUE rb_XC_UKXCL = rb_define_class_under(rb_XC, "UKXCL", rb_XC_League);
    rb_define_module_function(rb_XC_UKXCL, "optimize", rb_XC_UKXCL_optimize, 1);
}
