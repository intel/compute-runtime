#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_TBX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_proto.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_sockets.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_sockets_imp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_sockets_imp.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_TBX})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_TBX ${RUNTIME_SRCS_TBX})
