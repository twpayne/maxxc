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

%%{

machine igc_record_b;

action hour     { tm->tm_hour= 10 * (fpc[-1] - '0') + fc - '0'; }
action min      { tm->tm_min = 10 * (fpc[-1] - '0') + fc - '0'; }
action sec      { tm->tm_sec = 10 * (fpc[-1] - '0') + fc - '0'; }
# FIXME check return value of mktime
action time     { trkpt->time = mktime(tm); }
action lat_deg  { lat_deg    = 10 * (fpc[-1] - '0') + fc - '0'; }
action lat_mmin { lat_mmin   = 10000 * (fpc[-4] - '0') + 1000 * (fpc[-3] - '0') + 100 * (fpc[-2] - '0') + 10 * (fpc[-1] - '0') + fc - '0'; }
action N        { trkpt->lat = 60000 * lat_deg + lat_mmin; }
action S        { trkpt->lat = -60000 * lat_deg - lat_mmin; }
action lon_deg  { lon_deg    = 100 * (fpc[-2] - '0') + 10 * (fpc[-1] - '0') + fc - '0'; }
action lon_mmin { lon_mmin   = 10000 * (fpc[-4] - '0') + 1000 * (fpc[-3] - '0') + 100 * (fpc[-2] - '0') + 10 * (fpc[-1] - '0') + fc - '0'; }
action E        { trkpt->lon = 60000 * lon_deg + lon_mmin; }
action W        { trkpt->lon = -60000 * lon_deg - lon_mmin; }
action AV       { trkpt->fix_validity = fc; }
# FIXME handle negative altitudes and elevations
action alt      { trkpt->alt = 10000 * (fpc[-4] - '0') + 1000 * (fpc[-3] - '0') + 100 * (fpc[-2] - '0') + 10 * (fpc[-1] - '0') + fc - '0'; }
action gnss_alt { trkpt->ele = 10000 * (fpc[-4] - '0') + 1000 * (fpc[-3] - '0') + 100 * (fpc[-2] - '0') + 10 * (fpc[-1] - '0') + fc - '0'; }

brecord =
    "B"
    ( [0-1][0-9] | 2[0-3] ) @hour
    ( [0-5][0-9] )          @min
    ( [0-5][0-9] )          @sec @time
    ( [0-8][0-9] )          @lat_deg
    ( [0-5][0-9]{4} )       @lat_mmin
    ( "N" @N | "S" @S )
    ( [0-1][0-9][0-9] )     @lon_deg
    ( [0-5][0-9]{4} )       @lon_mmin
    ( "E" @E | "W" @W )
    ( [AV] @AV )
    ( [0-9]{5} )            @alt
    ( [0-9]{5} )            @gnss_alt
    /[^\r]*/
    "\r\n";

main := brecord 0 @{ fbreak; };

}%%

int igc_record_parse_b(const char *p, struct tm *tm, trkpt_t *trkpt)
{
    %% write data noerror;
    int cs = 0;
    int lat_deg = 0, lat_mmin = 0, lon_deg = 0, lon_mmin = 0;
    %% write init;
    %% write exec noend;
    return cs >= igc_record_b_first_final;
}

/* vim: set filetype=ragel: */
