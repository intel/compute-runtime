#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DISABLED_GTPIN_SUPPORT)
  set(GTPIN_INIT_FILE "${NEO_SOURCE_DIR}/opencl/source/gtpin/gtpin_init.cpp")
else()
  set(GTPIN_INIT_FILE "")
endif()

set(RUNTIME_SRCS_DLL_BASE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_dll.cpp
    ${NEO_SHARED_DIRECTORY}/built_ins/sip_init.cpp
    ${NEO_SHARED_DIRECTORY}/gmm_helper/resource_info.cpp
    ${NEO_SHARED_DIRECTORY}/gmm_helper/page_table_mngr.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/allow_deferred_deleter.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/debug_helpers.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/file_io.cpp
    ${NEO_SHARED_DIRECTORY}/utilities/cpuintrinsics.cpp
    ${NEO_SHARED_DIRECTORY}/utilities/debug_settings_reader_creator.cpp
    ${NEO_SHARED_DIRECTORY}/utilities/io_functions.cpp
    ${NEO_SOURCE_DIR}/opencl/source/api/api.cpp
    ${NEO_SOURCE_DIR}/opencl/source/dll/debug_manager.cpp
    ${NEO_SOURCE_DIR}/opencl/source/helpers/api_specific_config_ocl.cpp

    ${GTPIN_INIT_FILE}
    ${HW_SRC_LINK}
    ${EXPORTS_FILENAME}
    $<TARGET_OBJECTS:neo_shared_dlls_precompiled_objects>
)

append_sources_from_properties(RUNTIME_SRCS_DLL_BASE
                               NEO_CORE_SRCS_LINK
                               NEO_SRCS_ENABLE_CORE
)

set(RUNTIME_SRCS_DLL_LINUX
    ${NEO_SHARED_DIRECTORY}/dll/linux/debugger_l0_dll_linux.cpp
    ${NEO_SHARED_DIRECTORY}/dll/linux/drm_neo_create.cpp
    ${NEO_SHARED_DIRECTORY}/dll/linux${BRANCH_DIR_SUFFIX}/options_linux.cpp
    ${NEO_SHARED_DIRECTORY}/dll/linux/options_linux.inl
    ${NEO_SHARED_DIRECTORY}/dll/linux/os_interface.cpp
    ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}devices.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices/devices_base.inl
    ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
    ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux/platform_teardown_linux.cpp
)
if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
  if(NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM)
    list(APPEND RUNTIME_SRCS_DLL_LINUX
         ${NEO_SOURCE_DIR}/shared/source/os_interface/linux/xe/eudebug/enable_eudebug_interface_upstream.cpp
    )
  endif()
  if(NEO_ENABLE_XE_PRELIM_DETECTION)
    list(APPEND RUNTIME_SRCS_DLL_LINUX
         ${NEO_SOURCE_DIR}/shared/source/os_interface/linux/xe/eudebug/enable_eudebug_interface_prelim.cpp
    )
  endif()
endif()

set(RUNTIME_SRCS_DLL_WINDOWS
    ${NEO_SHARED_DIRECTORY}/dll/windows/debugger_l0_windows.cpp
    ${NEO_SHARED_DIRECTORY}/dll/windows${BRANCH_DIR_SUFFIX}/options_windows.cpp
    ${NEO_SHARED_DIRECTORY}/dll/windows/options_windows.inl
    ${NEO_SHARED_DIRECTORY}/dll/windows/os_interface.cpp
    ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_memory_virtual_alloc.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/windows/sys_calls.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/windows/sys_calls_winmm.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/windows/wddm/wddm_calls.cpp
    ${NEO_SHARED_DIRECTORY}/dll/wddm/wddm_create.cpp
    ${NEO_SOURCE_DIR}/opencl/source/os_interface/windows/platform_teardown_win.cpp
    ${NEO_SHARED_DIRECTORY}/dll/wddm/create_gdi.cpp
)

if(NOT DISABLE_WDDM_LINUX)
  list(APPEND RUNTIME_SRCS_DLL_LINUX
       ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
       ${NEO_SHARED_DIRECTORY}/dll/wddm/wddm_create.cpp
       ${NEO_SHARED_DIRECTORY}/dll/wddm/create_gdi.cpp
  )
endif()

target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_BASE})
if(WIN32)
  if(DEFINED NEO_DLL_RC_FILE)
    list(APPEND RUNTIME_SRCS_DLL_WINDOWS ${NEO_DLL_RC_FILE})
  endif()
  target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_WINDOWS})
else()
  target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DLL_LINUX})
endif()
