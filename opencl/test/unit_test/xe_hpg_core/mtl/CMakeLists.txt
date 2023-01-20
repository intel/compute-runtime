#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_MTL)
  set(IGDRCL_SRCS_tests_xe_hpg_core_mtl_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_mtl.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpg_core_mtl_excludes})

  set(IGDRCL_SRCS_tests_xe_hpg_core_mtl
      ${IGDRCL_SRCS_tests_xe_hpg_core_mtl_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/get_device_info_mtl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_mtl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_mtl.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpg_core_mtl})
  add_subdirectories()
endif()
