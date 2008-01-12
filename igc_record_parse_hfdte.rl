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

machine igc_record_hfdte;

action mday { tm->tm_mday = 10 * (fpc[-1] - '0') + fc - '0'; }
action mon  { tm->tm_mon  = 10 * (fpc[-1] - '0') + fc - '0' - 1; }
action year { tm->tm_year = 10 * (fpc[-1] - '0') + fc - '0' + 2000 - 1900; }

hfdte_record =
    "HFDTE"
    ( [0-9][0-9] ) @mday
    ( [0-9][0-9] ) @mon
    ( [0-9][0-9] ) @year
    "\r\n";

main := hfdte_record 0 @{ fbreak; };

}%%

int igc_record_parse_hfdte(const char *p, struct tm *tm)
{
    %% write data noerror;
    int cs = 0;
    %% write init;
    %% write exec noend;
    return cs >= igc_record_hfdte_first_final;
}

/* vim: set filetype=ragel: */
