#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_GENX_CPP_BASE
    buffer
    cl_hw_helper
    command_queue
    experimental_command_buffer
    gpgpu_walker
    hardware_commands_helper
    image
    sampler
)

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

    foreach(SRC_IT ${RUNTIME_SRCS_GENX_CPP_BASE})
      if(EXISTS ${GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
        list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
      endif()
    endforeach()

    foreach(BRANCH ${BRANCH_DIR_LIST})
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
