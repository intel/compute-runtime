#
# Copyright (C) 2019-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_uuid_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_context_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/os_library_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}hw_info_override_tests.cpp
)

if(WIN32 OR(UNIX AND NOT DISABLE_WDDM_LINUX))
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface_logging_tests.cpp)
endif()

add_subdirectories()