#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(_is_leap_year year result)
  math(EXPR _mod400 "${year} % 400")
  math(EXPR _mod100 "${year} % 100")
  math(EXPR _mod4 "${year} % 4")
  if(_mod400 EQUAL 0 OR(_mod4 EQUAL 0 AND NOT _mod100 EQUAL 0))
    set(${result} TRUE)
  else()
    set(${result} FALSE)
  endif()
endmacro()

function(neo_ww_calculator EPOCH OUT_VAR)
  set(SECONDS_PER_DAY 86400)

  math(EXPR total_days "${EPOCH} / ${SECONDS_PER_DAY}")

  math(EXPR year "1970 + ${total_days} / 365")

  set(days_to_year 0)
  set(y 1970)
  while(y LESS year)
  _is_leap_year(${y} leap)
  if(leap)
    math(EXPR days_to_year "${days_to_year} + 366")
  else()
    math(EXPR days_to_year "${days_to_year} + 365")
  endif()
  math(EXPR y "${y} + 1")
  endwhile()

  while(days_to_year GREATER total_days)
  math(EXPR year "${year} - 1")
  _is_leap_year(${year} leap)
  if(leap)
    math(EXPR days_to_year "${days_to_year} - 366")
  else()
    math(EXPR days_to_year "${days_to_year} - 365")
  endif()
  endwhile()

  math(EXPR doy "${total_days} - ${days_to_year} + 1")

  math(EXPR yr "${year} % 100")

  math(EXPR y "${year} - 1")
  math(EXPR dow1 "(1 + 5*(${y} % 4) + 4*(${y} % 100) + 6*(${y} % 400)) % 7")

  _is_leap_year(${year} leap)
  if(leap)
    set(y_days 366)
  else()
    set(y_days 365)
  endif()

  math(EXPR _doy "${doy} - 1 + ${dow1}") # shift day of year to simulate Jan 1st as Sunday
  math(EXPR _ww "${_doy} / 7 + 1") # get workweek
  math(EXPR _wd "${_doy} % 7") # get days of week
  math(EXPR _y_days "${y_days} + ${dow1}") # adjusted number of days in year
  math(EXPR _w_days "${_y_days} - ${_doy} + ${_wd}") # number of week days to end of year

  if(_w_days LESS 7)
    # last week has less than 7 days
    math(EXPR yr "(${yr} + 1) % 100")
    set(_ww 1)
  endif()

  string(LENGTH "${yr}" yr_len)
  string(LENGTH "${_ww}" ww_len)
  if(yr_len LESS 2)
    set(yr "0${yr}")
  endif()
  if(ww_len LESS 2)
    set(_ww "0${_ww}")
  endif()

  set(${OUT_VAR} "${yr}.${_ww}" PARENT_SCOPE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND CMAKE_ARGC GREATER 3)
  neo_ww_calculator(${CMAKE_ARGV3} _result)
  execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${_result}")
endif()
