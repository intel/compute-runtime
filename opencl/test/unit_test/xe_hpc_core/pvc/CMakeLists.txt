#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(IGDRCL_SRCS_tests_xe_hpc_core_pvc
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/get_device_info_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_pvc.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpc_core_pvc})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_pvc_0 pvc 0)
  neo_copy_test_files_with_revision(copy_test_files_pvc_3 pvc 3)
  add_dependencies(copy_test_files_per_product copy_test_files_pvc_0)
  add_dependencies(copy_test_files_per_product copy_test_files_pvc_3)
endif()
