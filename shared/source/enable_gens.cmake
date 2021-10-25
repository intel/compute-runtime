#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CORE_SRCS_GENX_H_BASE
    hw_cmds.h
    hw_info.h
    hw_cmds_base.h
    aub_mapper.h
)

set(CORE_RUNTIME_SRCS_GENX_CPP_BASE
    aub_command_stream_receiver
    aub_mem_dump
    command_encoder
    command_stream_receiver_hw
    command_stream_receiver_simulated_common_hw
    create_device_command_stream_receiver
    direct_submission
    experimental_command_buffer
    hw_helper
    hw_info
    preamble
    preemption
    state_base_address
    tbx_command_stream_receiver
)

set(CORE_RUNTIME_SRCS_GENX_CPP_WDDM
    windows/command_stream_receiver
    windows/direct_submission
    windows/gmm_callbacks
)

set(CORE_RUNTIME_SRCS_GENX_CPP_LINUX
    linux/command_stream_receiver
    linux/direct_submission
)

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    set(PLATFORM_FILE "hw_info_setup_${PLATFORM_IT_LOWER}.inl")
    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}${PLATFORM_FILE})
    if(EXISTS ${SRC_FILE})
      list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
    endif()
    foreach(BRANCH ${BRANCH_DIR_LIST})

      set(PLATFORM_FILE "hw_cmds_${PLATFORM_IT_LOWER}.h")
      set(PATH_TO_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}${PLATFORM_FILE})
      if(EXISTS ${PATH_TO_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${PATH_TO_FILE})
      endif()

      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}linux/hw_info_config_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_LINUX ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}windows/hw_info_config_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_WINDOWS ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}enable_hw_info_config_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/os_interface/linux/local${BRANCH_DIR_SUFFIX}${PLATFORM_IT_LOWER}/enable_local_memory_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_LINUX ${SRC_FILE})
      endif()

      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ail${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}${PLATFORM_IT_LOWER}/ail_configuration_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE "${CORE_GENX_PREFIX}/os_agnostic_hw_info_config_${PLATFORM_IT_LOWER}.inl")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(PLATFORM_FILE "hw_info_${PLATFORM_IT_LOWER}.cpp")
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}${PLATFORM_FILE})
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
      endif()
    endforeach()
  endforeach()

endmacro()

macro(macro_for_each_gen)
  foreach(BRANCH ${BRANCH_DIR_LIST})
    set(CORE_GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH}${GEN_TYPE_LOWER})
    set(GENERATED_GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/generated${BRANCH}${GEN_TYPE_LOWER})

    foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
      foreach(SRC_IT ${CORE_SRCS_GENX_H_BASE} "hw_info_${GEN_TYPE_LOWER}.h")
        set(SRC_FILE ${CORE_GENX_PREFIX}${BRANCH_DIR}${SRC_IT})
        if(EXISTS ${SRC_FILE})
          list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${SRC_FILE})
        endif()
      endforeach()

      foreach(OS_IT "BASE" "WDDM" "LINUX")
        foreach(SRC_IT ${CORE_RUNTIME_SRCS_GENX_CPP_${OS_IT}})
          set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
          if(EXISTS ${SRC_FILE})
            list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_${OS_IT} ${SRC_FILE})
          endif()
        endforeach()
      endforeach()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH}${GEN_TYPE_LOWER}/image_core_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/enable_family_full_core_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${CORE_GENX_PREFIX}${BRANCH_DIR}enable_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/enable_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE "${CORE_GENX_PREFIX}/os_agnostic_hw_info_config_${GEN_TYPE_LOWER}.inl")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE "${NEO_SOURCE_DIR}/third_party${BRANCH_DIR}sip_kernels/${GEN_TYPE_LOWER}/sip_kernel_${GEN_TYPE_LOWER}.cpp")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE "${NEO_SOURCE_DIR}/third_party${BRANCH_DIR}sip_kernels/${GEN_TYPE_LOWER}/sip_kernel_${GEN_TYPE_LOWER}.h")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${SRC_FILE})
      endif()

    endforeach()
    set(SRC_FILE "${CORE_GENX_PREFIX}/state_compute_mode_helper_${GEN_TYPE_LOWER}.cpp")
    if(EXISTS ${SRC_FILE})
      list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_BASE ${SRC_FILE})
    endif()
    if(EXISTS ${GENERATED_GENX_PREFIX}/hw_cmds_generated_${GEN_TYPE_LOWER}.inl)
      list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE "${GENERATED_GENX_PREFIX}/hw_cmds_generated_${GEN_TYPE_LOWER}.inl")
    endif()
  endforeach()
  apply_macro_for_each_platform()

  list(APPEND CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_${GEN_TYPE}_H_BASE})
  list(APPEND CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_${GEN_TYPE}_CPP_BASE})

  list(APPEND CORE_SRCS_GENX_ALL_WINDOWS ${CORE_SRCS_${GEN_TYPE}_CPP_WINDOWS})
  list(APPEND CORE_SRCS_GENX_ALL_WDDM ${CORE_SRCS_${GEN_TYPE}_CPP_WDDM})
  list(APPEND CORE_SRCS_GENX_ALL_LINUX ${CORE_SRCS_${GEN_TYPE}_CPP_LINUX})

  list(APPEND CORE_SRCS_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
  list(APPEND CORE_SRCS_LINK_LINUX ${${GEN_TYPE}_SRC_LINK_LINUX})
endmacro()

apply_macro_for_each_gen("SUPPORTED")

set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_GENX_ALL_BASE})
set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_LINUX ${CORE_SRCS_GENX_ALL_LINUX})
set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_WDDM ${CORE_SRCS_GENX_ALL_WDDM})
set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_WINDOWS ${CORE_SRCS_GENX_ALL_WINDOWS})
