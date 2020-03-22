#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(HAVE_INSTRUMENTATION)
    set(L0_SRCS_TOOLS_LINUX
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_query_imp_linux.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_enumeration_imp_linux.cpp)

    if(UNIX)
    target_sources(${L0_STATIC_LIB_NAME}
        PRIVATE
            ${L0_SRCS_TOOLS_LINUX}
    )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_LINUX ${L0_SRCS_TOOLS_LINUX})
endif()
