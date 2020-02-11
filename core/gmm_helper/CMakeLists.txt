#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GMM_HELPER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context/gmm_client_context_base.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context/gmm_client_context_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context${BRANCH_DIR_SUFFIX}/gmm_client_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_context${BRANCH_DIR_SUFFIX}/gmm_client_context.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_lib.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_types_converter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_types_converter.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/gmm_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_mngr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_mngr_impl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/resource_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/resource_info_impl.cpp
)

set(NEO_CORE_GMM_HELPER_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_memory${BRANCH_DIR_SUFFIX}/gmm_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_memory_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/gmm_memory_base.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_GMM_HELPER ${NEO_CORE_GMM_HELPER})
set_property(GLOBAL PROPERTY NEO_CORE_GMM_HELPER_WINDOWS ${NEO_CORE_GMM_HELPER_WINDOWS})
add_subdirectories()
