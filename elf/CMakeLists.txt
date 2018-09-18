#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(elflib STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/writer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/writer.h
)

create_project_source_tree(elflib)

target_include_directories(elflib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(elflib PRIVATE ${IGDRCL_SOURCE_DIR})

set_target_properties(elflib PROPERTIES FOLDER "elflib")

set_target_properties(elflib PROPERTIES POSITION_INDEPENDENT_CODE ON)
