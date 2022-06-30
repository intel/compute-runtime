#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SRCS_LIB_ULT_CS
    ${NEO_SOURCE_DIR}/shared/test/common/libult/create_command_stream.cpp
    ${NEO_SOURCE_DIR}/shared/test/common/libult/create_command_stream.h
)
add_library(neo_libult_cs OBJECT EXCLUDE_FROM_ALL
            ${NEO_SRCS_LIB_ULT_CS}
)

set_target_properties(neo_libult_cs PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(neo_libult_cs PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
set_property(TARGET neo_libult_cs APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(neo_libult_cs PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(neo_libult_cs PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
create_project_source_tree(neo_libult_cs)

add_subdirectories()

set_property(GLOBAL PROPERTY NEO_SRCS_LIB_ULT_CS ${NEO_SRCS_LIB_ULT_CS})
