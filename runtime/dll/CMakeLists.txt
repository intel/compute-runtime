#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DISABLED_GTPIN_SUPPORT)
  set(GTPIN_INIT_FILE "${NEO_SOURCE_DIR}/runtime/gtpin/gtpin_init.cpp")
else()
  set(GTPIN_INIT_FILE "")
endif()

set(RUNTIME_SRCS_DLL_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_tbx_sockets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/options_dll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/get_devices.cpp

  ${NEO_SOURCE_DIR}/core/gmm_helper/resource_info.cpp
  ${NEO_SOURCE_DIR}/core/gmm_helper/page_table_mngr.cpp
  ${NEO_SOURCE_DIR}/core/helpers/abort.cpp
  ${NEO_SOURCE_DIR}/core/helpers/debug_helpers.cpp
  ${NEO_SOURCE_DIR}/core/helpers/allow_deferred_deleter.cpp
  ${NEO_SOURCE_DIR}/core/utilities/cpuintrinsics.cpp
  ${NEO_SOURCE_DIR}/core/utilities/debug_settings_reader_creator.cpp
  ${NEO_SOURCE_DIR}/runtime/api/api.cpp
  ${NEO_SOURCE_DIR}/runtime/compiler_interface/default_cache_config.cpp
  ${NEO_SOURCE_DIR}/runtime/helpers/built_ins_helper.cpp
  ${NEO_SOURCE_DIR}/runtime/program/evaluate_unhandled_token.cpp

  ${GTPIN_INIT_FILE}
  ${HW_SRC_LINK}
  ${EXPORTS_FILENAME}
)

append_sources_from_properties(RUNTIME_SRCS_DLL_BASE NEO_CORE_SRCS_LINK)

set(RUNTIME_SRCS_DLL_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/allocator_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/create_drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/devices${BRANCH_DIR_SUFFIX}/devices.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/devices/devices_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/options_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/os_interface.cpp
  ${NEO_SOURCE_DIR}/core/os_interface/linux/sys_calls_linux.cpp
)

set(RUNTIME_SRCS_DLL_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/create_wddm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/environment_variables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/options_windows.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/os_interface.cpp
  ${NEO_SOURCE_DIR}/core/os_interface/windows/sys_calls.cpp
  ${NEO_SOURCE_DIR}/runtime/gmm_helper/gmm_memory.cpp
  ${NEO_SOURCE_DIR}/core/os_interface/windows/wddm/wddm_calls.cpp
  ${NEO_SOURCE_DIR}/core/os_interface/windows/wddm/wddm_create.cpp
)

target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_BASE})
if(WIN32)
  if(DEFINED NEO_DLL_RC_FILE)
    list(APPEND RUNTIME_SRCS_DLL_WINDOWS ${NEO_DLL_RC_FILE})
  endif()
  target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_WINDOWS})
else()
  target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_LINUX})
endif()
