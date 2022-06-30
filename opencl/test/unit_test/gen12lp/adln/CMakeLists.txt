#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  include(${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}excludes_ocl_adln.cmake)
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_adln_0 adln 0)
  add_dependencies(copy_test_files_per_product copy_test_files_adln_0)
endif()
