#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_fabric_port_imp_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/os_fabric_port_imp_prelim.h
  )

  if(LIBGENL_FOUND)
    set(L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX_ACCESS
        ${CMAKE_CURRENT_SOURCE_DIR}/iaf_nl_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/iaf_nl_api.h
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_access.h
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_access_imp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_access_imp.h
    )
  else()
    set(L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX_ACCESS
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_access_stub.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_access_stub.h
    )
  endif()
else()
  list(APPEND L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_fabric_port_imp.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/os_fabric_port_imp.h
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX}
                 ${L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX_ACCESS}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX ${L0_SRCS_TOOLS_SYSMAN_FABRICPORT_LINUX})
