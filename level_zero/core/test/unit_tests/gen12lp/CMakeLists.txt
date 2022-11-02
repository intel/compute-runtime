#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cache_flush.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_events_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_hw_helper_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_gen12lp.cpp
  )

  target_include_directories(${TARGET_NAME} PRIVATE ${COMPUTE_RUNTIME_DIR}/level_zero/core/source/gen12lp/definitions/)
endif()
