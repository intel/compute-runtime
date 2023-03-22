#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(test_l0_loader_lib)

if(WIN32)
  set(OS_SUFFIX win)
else()
  set(OS_SUFFIX lin)
endif()

add_library(test_l0_loader_lib SHARED test_l0_loader_lib_${OS_SUFFIX}.cpp)

create_project_source_tree(test_l0_loader_lib)

set_target_properties(test_l0_loader_lib PROPERTIES FOLDER "test mocks")
set_property(TARGET test_l0_loader_lib APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})

