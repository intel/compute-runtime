#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DISABLED_GTPIN_SUPPORT)
  set(GTPIN_INIT_FILE "gtpin/gtpin_init.cpp")
else()
  set(GTPIN_INIT_FILE "")
endif()

set(RUNTIME_SRCS_DLL_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_tbx_sockets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger.cpp

  ${IGDRCL_SOURCE_DIR}/runtime/api/api.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/resource_info.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/helpers/abort.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/helpers/built_ins_helper.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/helpers/debug_helpers.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/program/evaluate_unhandled_token.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/utilities/debug_settings_reader_creator.cpp

  ${GTPIN_INIT_FILE}
  ${HW_SRC_LINK}
  ${EXPORTS_FILENAME}
)

set(RUNTIME_SRCS_DLL_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/allocator_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/os_interface.cpp
)

set(RUNTIME_SRCS_DLL_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/os_interface.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/gmm_memory.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/page_table_mngr.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/os_interface/windows/sys_calls.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/os_interface/windows/wddm/wddm_calls.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/os_interface/windows/wddm/wddm_create.cpp
)

target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_BASE})
if(WIN32)
  target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_WINDOWS})
else()
  target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_LINUX})
endif()
