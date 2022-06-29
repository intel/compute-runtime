#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  include(${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}excludes_ocl_adln.cmake)
  add_subdirectories()
endif()
