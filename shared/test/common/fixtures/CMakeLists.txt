#
# Copyright (C) 2019-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_libult_common PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_tests_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_center_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_center_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/preamble_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sip_external_lib_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sip_external_lib_fixture.h
)

add_subdirectories()
