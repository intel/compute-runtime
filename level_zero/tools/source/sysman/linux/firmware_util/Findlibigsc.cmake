#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

find_path(libigsc_INCLUDE_DIR NAMES igsc_lib.h PATHS /usr/local/include /usr/include)
find_library(libigsc_LIBRARIES NAMES igsc libigsc PATHS /usr/local/lib /usr/lib)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(libigsc DEFAULT_MSG libigsc_LIBRARIES libigsc_INCLUDE_DIR)
