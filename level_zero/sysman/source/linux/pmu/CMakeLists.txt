#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_LINUX_PMU
    ${CMAKE_CURRENT_SOURCE_DIR}/pmu_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pmu_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pmu.h
)
if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_SYSMAN_LINUX_PMU}
  )
endif()
# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_PMU_LINUX ${L0_SRCS_SYSMAN_PMU_LINUX})
