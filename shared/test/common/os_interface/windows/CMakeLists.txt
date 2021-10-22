#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/gdi_dll_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sys_calls.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_memory_manager.h
               ${CMAKE_CURRENT_SOURCE_DIR}/wddm_fixture.h
)

set_property(GLOBAL PROPERTY NEO_CORE_SHARED_TESTS_OS_INTERFACE_WINDOWS ${NEO_CORE_SHARED_TESTS_OS_INTERFACE_WINDOWS})
