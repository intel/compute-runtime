#!/usr/bin/env python3

#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

import sys

from datetime import datetime, timezone

def convert_ww(epoch):
    dt = datetime.fromtimestamp(epoch, timezone.utc)

    # get some info from epoch
    yr = int(dt.strftime("%y"))
    doy = int(dt.strftime("%j"))
    # and day of week for Jan 1st
    dow1 = int(datetime(dt.year, 1, 1).strftime("%w"))

    # number of days in a year
    _is_leap = yr % 400 == 0 or (yr % 4 == 0 and yr % 100 != 0)
    _y_days = 366 if _is_leap else 365

    _doy = doy - 1 + dow1           # shift day of year to simulate Jan 1st as Sunday
    _ww = int(_doy / 7) + 1         # get workweek
    _wd = int(_doy % 7)             # get days of week
    _y_days = _y_days + dow1        # adjusted number of days in year
    _w_days = _y_days - _doy + _wd  # number of week days to end of year

    if _w_days < 7:
        # last week has less than 7 days
        yr = yr + 1
        _ww = 1

    print("{:02d}.{:02d}".format(yr, _ww))
    return 0

if __name__ == '__main__':
    sys.exit(convert_ww(int(sys.argv[1])))
