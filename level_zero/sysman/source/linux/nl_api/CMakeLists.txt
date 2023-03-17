#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/nl_api.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/nl_api.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/iaf_nl_api.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/iaf_nl_api.cpp
    )
  endif()
endif()
