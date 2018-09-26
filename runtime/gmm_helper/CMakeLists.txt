#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_GMM_HELPER_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context/gmm_client_context_base.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context/gmm_client_context_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context${BRANCH_DIR_SUFFIX}/gmm_client_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context${BRANCH_DIR_SUFFIX}/gmm_client_context.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_lib.h
  ${CMAKE_CURRENT_SOURCE_DIR}/resource_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/resource_info_impl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/gmm_utils.cpp
)

set(RUNTIME_SRCS_GMM_HELPER_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_mngr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_mngr_impl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_memory${BRANCH_DIR_SUFFIX}/gmm_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_memory_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_memory_base.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GMM_HELPER_BASE})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GMM_HELPER_WINDOWS})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GMM_HELPER_BASE ${RUNTIME_SRCS_GMM_HELPER_BASE})
