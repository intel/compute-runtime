#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_GENX_CPP_WINDOWS
    windows/command_stream_receiver
    windows/gmm_callbacks
)

set(RUNTIME_SRCS_GENX_CPP_LINUX
    linux/command_stream_receiver
)

if(NOT DISABLE_WDDM_LINUX)
  list(APPEND RUNTIME_SRCS_GENX_CPP_LINUX
       ${RUNTIME_SRCS_GENX_CPP_WINDOWS}
  )
endif()

set(RUNTIME_SRCS_GENX_CPP_BASE
    aub_command_stream_receiver
    aub_mem_dump
    buffer
    cl_hw_helper
    command_queue
    command_stream_receiver_simulated_common_hw
    create_device_command_stream_receiver
    experimental_command_buffer
    gpgpu_walker
    hardware_commands_helper
    hw_info
    image
    sampler
    tbx_command_stream_receiver
)

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    set(PLATFORM_FILE "hw_info_config_${PLATFORM_IT_LOWER}.inl")
    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}/${PLATFORM_FILE})
    if(EXISTS ${SRC_FILE})
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
    endif()

    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(PLATFORM_FILE "hw_info_${PLATFORM_IT_LOWER}.inl")
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}${PLATFORM_FILE})
      if(EXISTS ${SRC_FILE})
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
      endif()
      string(REGEX REPLACE "/$" "" _BRANCH_FILENAME_SUFFIX "${BRANCH_DIR}")
      string(REGEX REPLACE "^/" "_" _BRANCH_FILENAME_SUFFIX "${_BRANCH_FILENAME_SUFFIX}")
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}linux/hw_info_config_${PLATFORM_IT_LOWER}${_BRANCH_FILENAME_SUFFIX}.inl)
      if(EXISTS ${SRC_FILE})
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX ${SRC_FILE})
      endif()
    endforeach()
  endforeach()
endmacro()

macro(macro_for_each_gen)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    set(GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER})
    # Add default GEN files

    foreach(SRC_IT "state_compute_mode_helper_${GEN_TYPE_LOWER}.cpp")
      if(EXISTS ${GENX_PREFIX}/${SRC_IT})
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${GENX_PREFIX}/${SRC_IT})
      endif()
    endforeach()

    if(EXISTS "${GENX_PREFIX}/additional_files_${GEN_TYPE_LOWER}.cmake")
      include("${GENX_PREFIX}/additional_files_${GEN_TYPE_LOWER}.cmake")
    endif()

    if(${SUPPORT_DEVICE_ENQUEUE_${GEN_TYPE}})
      if(EXISTS ${GENX_PREFIX}/device_enqueue.h)
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/device_enqueue.h)
      endif()
      if(EXISTS ${GENX_PREFIX}/scheduler_definitions.h)
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/scheduler_definitions.h)
      endif()
      if(EXISTS ${GENX_PREFIX}/scheduler_builtin_kernel.inl)
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/scheduler_builtin_kernel.inl)
      endif()
      if(EXISTS ${GENX_PREFIX}/device_queue_${GEN_TYPE_LOWER}.cpp)
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${GENX_PREFIX}/device_queue_${GEN_TYPE_LOWER}.cpp)
      endif()
    endif()

    foreach(OS_IT "BASE" "WINDOWS" "LINUX")
      foreach(SRC_IT ${RUNTIME_SRCS_GENX_CPP_${OS_IT}})
        if(EXISTS ${GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
          list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_${OS_IT} ${GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
        endif()
      endforeach()
    endforeach()

    apply_macro_for_each_platform()

    foreach(BRANCH ${BRANCH_DIR_LIST})
      string(REGEX REPLACE "/$" "" _BRANCH_FILENAME_SUFFIX "${BRANCH}")
      string(REGEX REPLACE "^/" "_" _BRANCH_FILENAME_SUFFIX "${_BRANCH_FILENAME_SUFFIX}")
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH}${GEN_TYPE_LOWER}/image_core_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
      endif()
    endforeach()
    if(EXISTS ${GENX_PREFIX}/enable_family_full_ocl_${GEN_TYPE_LOWER}.cpp)
      list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_family_full_ocl_${GEN_TYPE_LOWER}.cpp)
    endif()
    if(NOT DISABLED_GTPIN_SUPPORT)
      if(EXISTS ${GENX_PREFIX}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
      endif()
    endif()

    list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_H_BASE})
    list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE})
    list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
    list(APPEND RUNTIME_SRCS_GENX_ALL_WINDOWS ${RUNTIME_SRCS_${GEN_TYPE}_CPP_WINDOWS})
    list(APPEND RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX})
    if(UNIX)
      list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_LINUX})
    endif()
  endforeach()
endmacro()

get_property(RUNTIME_SRCS_GENX_ALL_BASE GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_BASE)
get_property(RUNTIME_SRCS_GENX_ALL_LINUX GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_LINUX)
get_property(RUNTIME_SRCS_GENX_ALL_WINDOWS GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_WINDOWS)

apply_macro_for_each_gen("SUPPORTED")

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_BASE})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_WINDOWS})
else()
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_LINUX})
endif()

set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_GENX_ALL_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_GENX_ALL_LINUX})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_WINDOWS ${RUNTIME_SRCS_GENX_ALL_WINDOWS})
