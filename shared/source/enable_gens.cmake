#
# Copyright (C) 2020 Intel Corporation
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
    command_encoder
    command_stream_receiver_hw
    hw_helper
    preamble
    preemption
)

set(CORE_RUNTIME_SRCS_GENX_CPP_WINDOWS
    windows/direct_submission
)

set(CORE_RUNTIME_SRCS_GENX_CPP_LINUX
    linux/direct_submission
)

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

  foreach(PLATFORM_FILE "hw_cmds_${PLATFORM_IT_LOWER}.h" "hw_info_${PLATFORM_IT_LOWER}.h")
    if(EXISTS ${CORE_GENX_PREFIX}/${PLATFORM_FILE})
      list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${CORE_GENX_PREFIX}/${PLATFORM_FILE})
    endif()
  endforeach()

endmacro()

macro(macro_for_each_gen)
  set(CORE_GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/${GEN_TYPE_LOWER})
  set(GENERATED_GENX_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/generated/${GEN_TYPE_LOWER})

  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    string(REGEX REPLACE "/$" "" _BRANCH_FILENAME_SUFFIX "${BRANCH_DIR}")
    string(REGEX REPLACE "^/" "_" _BRANCH_FILENAME_SUFFIX "${_BRANCH_FILENAME_SUFFIX}")

    foreach(SRC_IT ${CORE_SRCS_GENX_H_BASE} "hw_info_${GEN_TYPE_LOWER}.h")
      set(SRC_FILE ${CORE_GENX_PREFIX}${BRANCH_DIR}${SRC_IT})
      if(EXISTS ${SRC_FILE})
        list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${SRC_FILE})
      endif()
    endforeach()

    foreach(OS_IT "BASE" "WINDOWS" "LINUX")
      foreach(SRC_IT ${CORE_RUNTIME_SRCS_GENX_CPP_${OS_IT}})
        set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_${OS_IT} ${SRC_FILE})
        endif()
      endforeach()
    endforeach()

    set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/enable_family_full_core_${GEN_TYPE_LOWER}.cpp)
    if(EXISTS ${SRC_FILE})
      list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
    endif()

    foreach(SRC_IT "enable_hw_info_config_" "enable_")
      set(SRC_FILE ${CORE_GENX_PREFIX}${BRANCH_DIR}${SRC_IT}${GEN_TYPE_LOWER}${_BRANCH_FILENAME_SUFFIX}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
      set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${GEN_TYPE_LOWER}/${SRC_IT}${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${SRC_FILE})
      endif()
    endforeach()
  endforeach()

  list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE "${GENERATED_GENX_PREFIX}/hw_cmds_generated_${GEN_TYPE_LOWER}.inl")

  apply_macro_for_each_platform()

  list(APPEND CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_${GEN_TYPE}_H_BASE})
  list(APPEND CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_${GEN_TYPE}_CPP_BASE})

  list(APPEND CORE_SRCS_GENX_ALL_WINDOWS ${CORE_SRCS_${GEN_TYPE}_CPP_WINDOWS})
  list(APPEND CORE_SRCS_GENX_ALL_LINUX ${CORE_SRCS_${GEN_TYPE}_CPP_LINUX})

  list(APPEND CORE_SRCS_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
endmacro()

apply_macro_for_each_gen("SUPPORTED")

set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_GENX_ALL_BASE})
set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_LINUX ${CORE_SRCS_GENX_ALL_LINUX})
set_property(GLOBAL PROPERTY CORE_SRCS_GENX_ALL_WINDOWS ${CORE_SRCS_GENX_ALL_WINDOWS})
