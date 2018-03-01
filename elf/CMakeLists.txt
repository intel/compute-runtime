# Copyright (c) 2017 - 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

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
