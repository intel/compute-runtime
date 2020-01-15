#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CORE_SRCS_GENX_H_BASE
  hw_cmds.h
  hw_info.h
  hw_cmds_base.h
)

set(CORE_RUNTIME_SRCS_GENX_CPP_BASE
  command_encoder
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

  foreach(SRC_IT ${CORE_SRCS_GENX_H_BASE})
    list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE ${CORE_GENX_PREFIX}/${SRC_IT})
  endforeach()

  list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE "${GENERATED_GENX_PREFIX}/hw_cmds_generated_${GEN_TYPE_LOWER}.inl")
  list(APPEND CORE_SRCS_${GEN_TYPE}_H_BASE "${CORE_GENX_PREFIX}/hw_info_${GEN_TYPE_LOWER}.h")

  foreach(SRC_IT ${CORE_RUNTIME_SRCS_GENX_CPP_BASE})
    list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_BASE ${CORE_GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
  endforeach()

  foreach(OS_IT "BASE" "WINDOWS" "LINUX")
    foreach(SRC_IT ${CORE_RUNTIME_SRCS_GENX_CPP_${OS_IT}})
      list(APPEND CORE_SRCS_${GEN_TYPE}_CPP_${OS_IT} ${CORE_GENX_PREFIX}/${SRC_IT}_${GEN_TYPE_LOWER}.cpp)
    endforeach()
  endforeach()
  apply_macro_for_each_platform()

  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${CORE_GENX_PREFIX}/enable_family_full_core_${GEN_TYPE_LOWER}.cpp)
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${CORE_GENX_PREFIX}/enable_hw_info_config_${GEN_TYPE_LOWER}.cpp)
  list(APPEND ${GEN_TYPE}_SRC_LINK_BASE ${CORE_GENX_PREFIX}/enable_${GEN_TYPE_LOWER}.cpp)

  list(APPEND CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_${GEN_TYPE}_H_BASE})
  list(APPEND CORE_SRCS_GENX_ALL_BASE ${CORE_SRCS_${GEN_TYPE}_CPP_BASE})

  list(APPEND CORE_SRCS_GENX_ALL_WINDOWS ${CORE_SRCS_${GEN_TYPE}_CPP_WINDOWS})
  list(APPEND CORE_SRCS_GENX_ALL_LINUX ${CORE_SRCS_${GEN_TYPE}_CPP_LINUX})

  list(APPEND CORE_SRCS_LINK ${${GEN_TYPE}_SRC_LINK_BASE})
endmacro()

apply_macro_for_each_gen("SUPPORTED")
