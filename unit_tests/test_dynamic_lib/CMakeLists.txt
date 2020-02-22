#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(test_dynamic_lib)

if(WIN32)
  set(OS_SUFFIX win)
else()
  set(OS_SUFFIX lin)
endif()

add_library(test_dynamic_lib SHARED test_dynamic_lib_${OS_SUFFIX}.cpp)

create_project_source_tree(test_dynamic_lib)

set_target_properties(test_dynamic_lib PROPERTIES FOLDER "test mocks")
set_property(TARGET test_dynamic_lib APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})

