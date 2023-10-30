#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_ENGINE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_SYSMAN_ENGINE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_engine_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_engine_prelim.h
  )

else()
  list(APPEND L0_TESTS_SYSMAN_ENGINE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_engine.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_engine.h
  )
endif()

if(NEO_ENABLE_XE_DRM_DETECTION)
  list(APPEND L0_TESTS_SYSMAN_ENGINE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_engine_xe.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_engine_xe.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_ENGINE_LINUX}
  )
endif()
