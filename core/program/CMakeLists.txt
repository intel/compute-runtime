#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PROGRAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/print_formatter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/print_formatter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/program_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/program_info_from_patchtokens.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_info_from_patchtokens.h
  ${CMAKE_CURRENT_SOURCE_DIR}/program_initialization.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_initialization.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sync_buffer_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sync_buffer_handler.h
)

set_property(GLOBAL PROPERTY NEO_CORE_PROGRAM ${NEO_CORE_PROGRAM})
