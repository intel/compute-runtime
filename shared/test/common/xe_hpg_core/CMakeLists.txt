#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  set(IGDRCL_SRCS_tests_xe_hpg_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpg_core_excludes})

  target_sources(${TARGET_NAME} PRIVATE
                 ${IGDRCL_SRCS_tests_xe_hpg_core_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_xe_hpg_core.h
  )

  add_subdirectories()
endif()
