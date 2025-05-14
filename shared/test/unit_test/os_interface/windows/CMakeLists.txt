#
# Copyright (C) 2019-2025 Intel Corporation
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
                 ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_adapter_luid_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests_win.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_win_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader_tests.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_win.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/um_km_data_translator_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}wddm_additional_apater_info_options_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_address_space_windows_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_command_stream_l0_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_kmdaf_listener_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_mapper_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_controller_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_special_heap_test.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_reservation_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_windows_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm32_tests_extra.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}/wddm_perf_tests.cpp
  )
endif()

if(WIN32 OR(UNIX AND NOT DISABLE_WDDM_LINUX))
  target_sources(neo_shared_tests PRIVATE
                 ${IGDRCL_SRC_tests_wddm_interface}
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm20_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm23_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm32_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_address_space_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_mapper_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_preemption_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_shared_allocations_test.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_operations_handler_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_operations_handler_with_aub_dump_tests.cpp
  )
endif()

if(NOT WIN32 AND NOT DISABLE_WDDM_LINUX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_perf_tests.cpp
  )
endif()

add_subdirectories()