#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}stream_properties_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_1_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_2_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_3_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_file_stream_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/get_devices_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties_tests_common.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties_tests_common.h
               ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_tests.cpp
)

if(TESTS_XEHP_AND_LATER)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_tests_xehp_and_later.cpp
  )
endif()

if(TESTS_PVC_AND_LATER)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_pvc_and_later.cpp)
endif()

add_subdirectories()
