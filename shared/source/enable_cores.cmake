#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CORE_SRCS_COREX_H_BASE
    hw_cmds.h
    hw_info.h
    hw_cmds_base.h
    aub_mapper.h
)

set(CORE_RUNTIME_SRCS_COREX_CPP_BASE
    aub_command_stream_receiver
    aub_mem_dump
    command_encoder
    command_stream_receiver_hw
    command_stream_receiver_simulated_common_hw
    create_device_command_stream_receiver
    debugger
    direct_submission
    experimental_command_buffer
    host_function
    implicit_scaling
    gfx_core_helper
    gmm_callbacks
    hw_info
    preamble
    preemption
    state_base_address
    tbx_command_stream_receiver
)

set(CORE_RUNTIME_SRCS_COREX_CPP_WDDM
    windows/command_stream_receiver
    windows/direct_submission
)

set(CORE_RUNTIME_SRCS_COREX_CPP_LINUX
    linux/command_stream_receiver
    linux/direct_submission
)

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    set(PLATFORM_FILE "hw_info_setup_${PLATFORM_IT_LOWER}.inl")
    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}${PLATFORM_FILE})
    if(EXISTS ${SRC_FILE})
      list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
    endif()

    set(PLATFORM_FILE "hw_cmds_${PLATFORM_IT_LOWER}.inl")
    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}${PLATFORM_FILE})
    if(EXISTS ${SRC_FILE})
      list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
    endif()

    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/additional_files_${CORE_TYPE_LOWER}.cmake)
    if(EXISTS ${SRC_FILE})
      include(${SRC_FILE})
    endif()

    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(PATH_TO_CORE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH})

      set(SRC_FILE ${PATH_TO_CORE}hw_cmds_${PLATFORM_IT_LOWER}.h)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}hw_cmds_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}linux/hw_info_extra_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_LINUX ${SRC_FILE})
      endif()
      set(SRC_FILE ${PATH_TO_CORE}windows/hw_info_extra_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_WINDOWS ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}linux/product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_LINUX ${SRC_FILE})
      endif()
      set(SRC_FILE ${PATH_TO_CORE}windows/product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_WINDOWS ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}enable_product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/compiler_product_helper_${PLATFORM_IT_LOWER}.inl)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/compiler_product_helper_${PLATFORM_IT_LOWER}_base.inl)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${PATH_TO_CORE}enable_compiler_product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/os_interface/linux/local${BRANCH_DIR_SUFFIX}${PLATFORM_IT_LOWER}/enable_ioctl_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_LINUX ${SRC_FILE})
      endif()

      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ail${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH}${PLATFORM_IT_LOWER}/ail_configuration_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}${PLATFORM_IT_LOWER}/os_agnostic_product_helper_${PLATFORM_IT_LOWER}.inl)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX}os_agnostic_product_helper_${PLATFORM_IT_LOWER}_extra.inl)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}${PLATFORM_IT_LOWER}/device_ids_configs_${PLATFORM_IT_LOWER}.h)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}${PLATFORM_IT_LOWER}/definitions/device_ids_configs_${PLATFORM_IT_LOWER}_base.h)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX}device_ids_configs_${PLATFORM_IT_LOWER}.h)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX}device_ids_configs_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}hw_info_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${PATH_TO_CORE}hw_info_${PLATFORM_IT_LOWER}.h)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()
    endforeach()
  endforeach()

endmacro()

macro(macro_for_each_core_type)
  foreach(BRANCH ${BRANCH_DIR_LIST})
    set(CORE_COREX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH}${CORE_TYPE_LOWER})
    set(GENERATED_COREX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/generated${BRANCH}${CORE_TYPE_LOWER})

    foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
      foreach(SRC_IT ${CORE_SRCS_COREX_H_BASE} "hw_info_${CORE_TYPE_LOWER}.h" "hw_cmds_${CORE_TYPE_LOWER}_base.h")
        set(SRC_FILE ${CORE_COREX_PREFIX}${BRANCH_DIR}${SRC_IT})
        if(EXISTS ${SRC_FILE})
          list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
        endif()
      endforeach()

      foreach(OS_IT "BASE" "WDDM" "LINUX")
        foreach(SRC_IT ${CORE_RUNTIME_SRCS_COREX_CPP_${OS_IT}})
          set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/${SRC_IT}_${CORE_TYPE_LOWER}.cpp)
          if(EXISTS ${SRC_FILE})
            list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_${OS_IT} ${SRC_FILE})
          endif()
        endforeach()
      endforeach()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH}${CORE_TYPE_LOWER}/image_core_${CORE_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/enable_family_full_core_${CORE_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${CORE_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/enable_${CORE_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND NEO_SRCS_ENABLE_CORE ${SRC_FILE})
      endif()

      set(SRC_FILE "${CORE_COREX_PREFIX}/os_agnostic_product_helper_${CORE_TYPE_LOWER}.inl")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE "${NEO_SOURCE_DIR}/third_party${BRANCH_DIR}sip_kernels/${CORE_TYPE_LOWER}/sip_kernel_${CORE_TYPE_LOWER}.cpp")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_CPP_BASE ${SRC_FILE})
      endif()

      set(SRC_FILE "${NEO_SOURCE_DIR}/third_party${BRANCH_DIR}sip_kernels/${CORE_TYPE_LOWER}/sip_kernel_${CORE_TYPE_LOWER}.h")
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE ${SRC_FILE})
      endif()

    endforeach()
    if(EXISTS ${GENERATED_COREX_PREFIX}/hw_cmds_generated_${CORE_TYPE_LOWER}.inl)
      list(APPEND CORE_SRCS_${CORE_TYPE}_H_BASE "${GENERATED_COREX_PREFIX}/hw_cmds_generated_${CORE_TYPE_LOWER}.inl")
    endif()
  endforeach()
  apply_macro_for_each_platform("SUPPORTED")

  list(APPEND CORE_SRCS_COREX_ALL_BASE ${CORE_SRCS_${CORE_TYPE}_H_BASE})
  list(APPEND CORE_SRCS_COREX_ALL_BASE ${CORE_SRCS_${CORE_TYPE}_CPP_BASE})

  list(APPEND CORE_SRCS_COREX_ALL_WINDOWS ${CORE_SRCS_${CORE_TYPE}_CPP_WINDOWS})
  list(APPEND CORE_SRCS_COREX_ALL_WDDM ${CORE_SRCS_${CORE_TYPE}_CPP_WDDM})
  list(APPEND CORE_SRCS_COREX_ALL_LINUX ${CORE_SRCS_${CORE_TYPE}_CPP_LINUX})

  list(APPEND CORE_SRCS_LINK ${${CORE_TYPE}_SRC_LINK_BASE})
  list(APPEND CORE_SRCS_LINK_LINUX ${${CORE_TYPE}_SRC_LINK_LINUX})
endmacro()

apply_macro_for_each_core_type("SUPPORTED")

set_property(GLOBAL PROPERTY CORE_SRCS_COREX_ALL_BASE ${CORE_SRCS_COREX_ALL_BASE})
set_property(GLOBAL PROPERTY CORE_SRCS_COREX_ALL_LINUX ${CORE_SRCS_COREX_ALL_LINUX})
set_property(GLOBAL PROPERTY CORE_SRCS_COREX_ALL_WDDM ${CORE_SRCS_COREX_ALL_WDDM})
set_property(GLOBAL PROPERTY CORE_SRCS_COREX_ALL_WINDOWS ${CORE_SRCS_COREX_ALL_WINDOWS})
set_property(GLOBAL APPEND PROPERTY NEO_SRCS_ENABLE_CORE ${NEO_SRCS_ENABLE_CORE})
