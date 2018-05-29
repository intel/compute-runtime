# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

set(RUNTIME_SRCS_GENX_CPP_WINDOWS
  windows/command_stream_receiver
  windows/translationtable_callbacks
  windows/wddm_engine_mapper
  windows/wddm
)

set(RUNTIME_SRCS_GENX_CPP_LINUX
  linux/command_stream_receiver
  linux/drm_engine_mapper
)

set(RUNTIME_SRCS_GENX_H_BASE
  aub_mapper.h
  device_enqueue.h
  hw_cmds.h
  hw_cmds_generated.h
  hw_info.h
  reg_configs.h
  scheduler_definitions.h
  scheduler_igdrcl_built_in.inl
)

set(RUNTIME_SRCS_GENX_CPP_BASE
  aub_command_stream_receiver
  aub_mem_dump
  command_queue
  device_queue
  command_stream_receiver_hw
  flat_batch_buffer_helper_hw
  gpgpu_walker
  hw_helper
  hw_info
  buffer
  image
  kernel_commands
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

  list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX ${GENX_PREFIX}/linux/hw_info_config_${PLATFORM_IT_LOWER}.cpp)
  list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_WINDOWS ${GENX_PREFIX}/windows/hw_info_config_${PLATFORM_IT_LOWER}.cpp)

  # Enable platform
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_${PLATFORM_IT_LOWER}.cpp)
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_hw_info_config_${PLATFORM_IT_LOWER}.cpp)
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

  foreach(OS_IT "BASE" "WINDOWS" "LINUX")
    foreach(SRC_IT ${RUNTIME_SRCS_GENX_CPP_${OS_IT}})
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_CPP_${OS_IT} ${GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
    endforeach()
  endforeach()

  apply_macro_for_each_platform()

  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_family_full_${GEN_TYPE_LOWER}.cpp)

  list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_H_BASE})
  list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_CPP_BASE})
  list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
  list(APPEND RUNTIME_SRCS_GENX_ALL_WINDOWS ${RUNTIME_SRCS_${GEN_TYPE}_CPP_WINDOWS})
  list(APPEND RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_${GEN_TYPE}_CPP_LINUX})

  if(UNIX)
    list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_LINUX})
  endif()
  if(GTPIN_HEADERS_DIR)
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
