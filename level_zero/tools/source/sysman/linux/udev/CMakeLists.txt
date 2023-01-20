#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(LIBUDEV_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_UDEV_LIB
      ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib_imp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib_imp.h
      ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib.h
  )
  add_subdirectories()
else()
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_UDEV_LIB
      ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib_imp_stub.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/udev_lib.h
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX_UDEV_LIB}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_LINUX_UDEV_LIB ${L0_SRCS_TOOLS_SYSMAN_LINUX_UDEV_LIB})
