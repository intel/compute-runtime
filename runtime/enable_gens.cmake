#
# Copyright (C) 2018-2019 Intel Corporation
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

set(RUNTIME_SRCS_GENX_H_BASE
  aub_mapper.h
  hw_cmds.h
  hw_cmds_generated.h
  hw_info.h
  reg_configs.h
)

set(RUNTIME_SRCS_GENX_CPP_BASE
  aub_command_stream_receiver
  aub_mem_dump
  buffer
  command_queue
  command_stream_receiver_hw
  command_stream_receiver_simulated_common_hw
  device_queue
  experimental_command_buffer
  gpgpu_walker
  hardware_commands_helper
  hw_helper
  hw_info
  image
  preamble
  preemption
  sampler
  state_base_address
  tbx_command_stream_receiver
)

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
  foreach(PLATFORM_FILE "hw_cmds_${PLATFORM_IT_LOWER}.h")
    if(EXISTS ${GENX_PREFIX}/${PLATFORM_FILE})
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/${PLATFORM_FILE})
    endif()
  endforeach()

  foreach(PLATFORM_FILE "hw_info_${PLATFORM_IT_LOWER}.inl")
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE ${GENX_PREFIX}/${PLATFORM_FILE})
  endforeach()
  
  foreach(PLATFORM_FILE "hw_info_${PLATFORM_IT_LOWER}.h")
    if(EXISTS ${GENX_PREFIX}/${PLATFORM_FILE})
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/${PLATFORM_FILE})
    endif()
  endforeach()
  
  list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX ${GENX_PREFIX}/linux/hw_info_config_${PLATFORM_IT_LOWER}.inl)
endmacro()

macro(macro_for_each_gen)
  set(GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/${GEN_TYPE_LOWER})
  # Add default GEN files
  foreach(SRC_IT ${RUNTIME_SRCS_GENX_H_BASE})
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/${SRC_IT})
  endforeach()
  if(EXISTS "${GENX_PREFIX}/hw_cmds_generated_patched.h")
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE "${GENX_PREFIX}/hw_cmds_generated_patched.h")
  endif()
  if(EXISTS "${GENX_PREFIX}/hw_cmds_base.h")
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE "${GENX_PREFIX}/hw_cmds_base.h")
  endif()
  if(EXISTS "${GENX_PREFIX}/hw_info_${GEN_TYPE_LOWER}.h")
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE "${GENX_PREFIX}/hw_info_${GEN_TYPE_LOWER}.h")
  endif()

  if(${SUPPORT_DEVICE_ENQUEUE_${GEN_TYPE}})
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/device_enqueue.h)
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/scheduler_definitions.h)
    list(APPEND RUNTIME_SRCS_${GEN_TYPE}_H_BASE ${GENX_PREFIX}/scheduler_igdrcl_built_in.inl)
  endif()

  foreach(OS_IT "BASE" "WINDOWS" "LINUX")
    foreach(SRC_IT ${RUNTIME_SRCS_GENX_CPP_${OS_IT}})
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_${OS_IT} ${GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
    endforeach()
  endforeach()

  apply_macro_for_each_platform()
  
  list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_WINDOWS ${GENX_PREFIX}/windows/hw_info_config_${GEN_TYPE_LOWER}.cpp)
  list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX ${GENX_PREFIX}/linux/hw_info_config_${GEN_TYPE_LOWER}.cpp)
  
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_family_full_${GEN_TYPE_LOWER}.cpp)
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_hw_info_config_${GEN_TYPE_LOWER}.cpp)
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_${GEN_TYPE_LOWER}.cpp)

  list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_H_BASE})
  list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE})
  list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
  list(APPEND RUNTIME_SRCS_GENX_ALL_WINDOWS ${RUNTIME_SRCS_${GEN_TYPE}_CPP_WINDOWS})
  list(APPEND RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX})

  if(UNIX)
    list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_LINUX})
  endif()
  if(NOT DISABLED_GTPIN_SUPPORT)
    list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
  endif()

endmacro()

apply_macro_for_each_gen("SUPPORTED")

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_BASE})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_WINDOWS})
else()
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_LINUX})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_GENX_ALL_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_GENX_ALL_LINUX})
