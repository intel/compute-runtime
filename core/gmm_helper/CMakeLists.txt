#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GMM_HELPER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context/gmm_client_context_base.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context/gmm_client_context_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context${BRANCH_DIR_SUFFIX}/gmm_client_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context${BRANCH_DIR_SUFFIX}/gmm_client_context.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_lib.h
)

set_property(GLOBAL PROPERTY NEO_CORE_GMM_HELPER ${NEO_CORE_GMM_HELPER})
add_subdirectories()
