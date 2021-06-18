#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

get_filename_component(IGSC_DIR_tmp "${NEO_SOURCE_DIR}/../igsc" ABSOLUTE)

if(IS_DIRECTORY "${IGSC_DIR_tmp}")
  set(NEO__IGSC_INCLUDE_DIR "${IGSC_DIR_tmp}/include")
  set(NEO__IGSC_LIB_DIR "${IGSC_DIR_tmp}/lib")
endif()

find_path(libigsc_INCLUDE_DIR NAMES igsc_lib.h PATHS ${NEO__IGSC_INCLUDE_DIR} /usr/local/include /usr/include)
find_library(libigsc_LIBRARIES NAMES igsc libigsc PATHS ${NEO__IGSC_LIB_DIR} /usr/local/lib /usr/lib)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(libigsc DEFAULT_MSG libigsc_LIBRARIES libigsc_INCLUDE_DIR)
