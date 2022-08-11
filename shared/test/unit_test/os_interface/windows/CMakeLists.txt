#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/adapter_info_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/adapter_info_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests_win.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_win_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_win.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/um_km_data_translator_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_address_space_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_command_stream_l0_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_mapper_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_preemption_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_shared_allocations_test.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_special_heap_test.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_tests.cpp
  )
endif()

add_subdirectories()