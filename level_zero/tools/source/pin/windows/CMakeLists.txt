#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/os_pin.h
)

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_WINDOWS}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  target_include_directories(${L0_STATIC_LIB_NAME}
                             PRIVATE
                             ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_WINDOWS ${L0_SRCS_TOOLS_WINDOWS})

