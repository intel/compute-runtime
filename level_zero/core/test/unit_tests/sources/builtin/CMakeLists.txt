#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/create_ult_builtin_functions_lib.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/built_in_tests_l0.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/builtin_functions_tests.cpp
)
if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/built_in_spv_tests_l0.cpp
  )
endif()
