#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BDW)
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_bdw_0 bdw 0)
  add_dependencies(copy_test_files_per_product copy_test_files_bdw_0)
endif()
