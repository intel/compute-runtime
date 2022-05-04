#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(NEO_CORE_EXECUTION_ENVIRONMENT_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_linux.cpp
  )

  set(NEO_CORE_EXECUTION_ENVIRONMENT_WDDM
      ${CMAKE_CURRENT_SOURCE_DIR}/../windows/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/../windows/execution_environment_windows.cpp
  )

  set_property(GLOBAL PROPERTY NEO_CORE_EXECUTION_ENVIRONMENT_WDDM ${NEO_CORE_EXECUTION_ENVIRONMENT_WDDM})
  set_property(GLOBAL PROPERTY NEO_CORE_EXECUTION_ENVIRONMENT_LINUX ${NEO_CORE_EXECUTION_ENVIRONMENT_LINUX})
endif()
