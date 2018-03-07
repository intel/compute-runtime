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

set(RUNTIME_SRCS_GENX_WINDOWS
  windows/command_stream_receiver.cpp
  windows/translationtable_callbacks.cpp
  windows/wddm_engine_mapper.cpp
  windows/wddm.cpp
)
set(RUNTIME_SRCS_GENX_LINUX
  linux/command_stream_receiver.cpp
  linux/drm_engine_mapper.cpp
)

set(RUNTIME_SRCS_GENX_BASE
  aub_command_stream_receiver.cpp
  aub_mapper.h
  aub_mem_dump.cpp
  command_queue.cpp
  device_enqueue.h
  device_queue.cpp
  command_stream_receiver_hw.cpp
  hw_cmds.h
  hw_cmds_generated.h
  hw_helper.cpp
  hw_info.cpp
  hw_info.h
  buffer.cpp
  image.cpp
  kernel_commands.cpp
  preamble.cpp
  preemption.cpp
  reg_configs.h
  sampler.cpp
  scheduler_definitions.h
  scheduler_igdrcl_built_in.inl
  state_base_address.cpp
  tbx_command_stream_receiver.cpp
)

foreach(GEN_TYPE ${ALL_GEN_TYPES})
  string(TOLOWER ${GEN_TYPE} GEN_TYPE_LOWER)
  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_TYPE} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    set(GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/${GEN_TYPE_LOWER})
    # Add default GEN files
    foreach(OS_IT "BASE" "WINDOWS" "LINUX")
      foreach(SRC_IT ${RUNTIME_SRCS_GENX_${OS_IT}})
        if(EXISTS ${GENX_PREFIX}/${SRC_IT})
          list(APPEND RUNTIME_SRCS_${GEN_TYPE}_${OS_IT} ${GENX_PREFIX}/${SRC_IT})
        endif()
      endforeach()
    endforeach()

    # Get all supported platforms for this GEN
    GET_PLATFORMS_FOR_GEN("SUPPORTED" ${GEN_TYPE} SUPPORTED_GENX_PLATFORMS)

    # Add platform-specific files
    foreach(PLATFORM_IT ${SUPPORTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      foreach(PLATFORM_FILE "hw_cmds_${PLATFORM_IT_LOWER}.h" "hw_info_${PLATFORM_IT_LOWER}.cpp")
        if(EXISTS ${GENX_PREFIX}/${PLATFORM_FILE})
          list(APPEND RUNTIME_SRCS_${GEN_TYPE}_BASE ${GENX_PREFIX}/${PLATFORM_FILE})
        endif()
      endforeach()

      # HwInfoConfig implementation
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_LINUX ${GENX_PREFIX}/linux/hw_info_config_${PLATFORM_IT_LOWER}.cpp)
      list(APPEND RUNTIME_SRCS_${GEN_TYPE}_WINDOWS ${GENX_PREFIX}/windows/hw_info_config_${PLATFORM_IT_LOWER}.cpp)

      # Enable platform
      list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_${PLATFORM_IT_LOWER}.cpp)
      list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_hw_info_config_${PLATFORM_IT_LOWER}.cpp)
    endforeach()

    list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/enable_family_full.cpp)

    if(GTPIN_HEADERS_DIR)
      list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${GENX_PREFIX}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
    endif(GTPIN_HEADERS_DIR)

    list(APPEND RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_${GEN_TYPE}_BASE})
    list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
    list(APPEND RUNTIME_SRCS_GENX_ALL_WINDOWS ${RUNTIME_SRCS_${GEN_TYPE}_WINDOWS})
    list(APPEND RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_${GEN_TYPE}_LINUX})
    if(UNIX)
      list(APPEND HW_SRC_LINK ${${GEN_TYPE}_SRC_LINK_LINUX})
    endif()
  endif()
endforeach()

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_BASE})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_WINDOWS})
else()
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GENX_ALL_LINUX})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_BASE ${RUNTIME_SRCS_GENX_ALL_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GENX_ALL_LINUX ${RUNTIME_SRCS_GENX_ALL_LINUX})
