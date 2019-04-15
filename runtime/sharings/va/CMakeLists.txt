#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_SHARINGS_VA
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_va_api.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing.h
  ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing_defines.h
  ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/va_surface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/va_surface.h
)
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SHARINGS_VA ${RUNTIME_SRCS_SHARINGS_VA})

if(IGDRCL__LIBVA_FOUND)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_VA})
endif(IGDRCL__LIBVA_FOUND)
