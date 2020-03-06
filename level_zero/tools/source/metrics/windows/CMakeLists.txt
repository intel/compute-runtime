#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(HAVE_INSTRUMENTATION)
    set(L0_SRCS_TOOLS_WINDOWS
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_query_imp_windows.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_enumeration_imp_windows.cpp)

    if(WIN32)
    target_sources(${TARGET_NAME_L0}
        PRIVATE
            ${L0_SRCS_TOOLS_WINDOWS}
    )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_WINDOWS ${L0_SRCS_TOOLS_WINDOWS})
endif()
