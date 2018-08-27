# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

if(UNIX)
  if(NOT DEFINED NEO_DRIVER_VERSION)
    find_program(GIT NAMES git)
    if(NOT "${GIT}" STREQUAL "GIT-NOTFOUND")
      if(IS_DIRECTORY ${IGDRCL_SOURCE_DIR}/.git)
        set(GIT_arg --git-dir=${IGDRCL_SOURCE_DIR}/.git show -s --format=%ct)
        execute_process(
          COMMAND ${GIT} ${GIT_arg}
          OUTPUT_VARIABLE GIT_output
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      endif()
    endif()

    if(NOT DEFINED NEO_VERSION_MAJOR)
      if(NOT DEFINED GIT_output)
        set(NEO_VERSION_MAJOR 1)
      else()
        SET(DATE_arg --date=@${GIT_output} +%y)
        execute_process(
          COMMAND date ${DATE_arg}
          OUTPUT_VARIABLE NEO_VERSION_MAJOR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "Computed version major is: ${NEO_VERSION_MAJOR}")
      endif()
    endif()

    if(NOT DEFINED NEO_VERSION_MINOR)
      if(NOT DEFINED GIT_output)
        set(NEO_VERSION_MINOR 0)
      else()
        SET(DATE_arg --date=@${GIT_output} +%V)
        execute_process(
          COMMAND date ${DATE_arg}
          OUTPUT_VARIABLE NEO_VERSION_MINOR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "Computed version minor is: ${NEO_VERSION_MINOR}")
      endif()
    endif()

    if(NOT DEFINED NEO_VERSION_BUILD)
      set(NEO_VERSION_BUILD 0)
    endif()
    set(NEO_DRIVER_VERSION "${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}")
  endif()
else()
  if(NOT DEFINED NEO_VERSION_MAJOR)
    set(NEO_VERSION_MAJOR 1)
  endif()

  if(NOT DEFINED NEO_VERSION_MINOR)
    set(NEO_VERSION_MINOR 0)
  endif()

  if(NOT DEFINED NEO_VERSION_BUILD)
    set(NEO_VERSION_BUILD 0)
  endif()
  set(NEO_DRIVER_VERSION "${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}")
endif(UNIX)
