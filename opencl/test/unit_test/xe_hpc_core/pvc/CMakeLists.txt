#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(IGDRCL_SRCS_tests_xe_hpc_core_pvc
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/get_device_info_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_pvc.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpc_core_pvc})
  add_subdirectories()
endif()
