# Copyright (c) 2018, Intel Corporation
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

set(RUNTIME_SRCS_MEM_OBJ
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/buffer_factory_init.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/image.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/image.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/image_factory_init.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/map_operations_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/map_operations_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mem_obj_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/pipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pipe.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_MEM_OBJ})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_MEM_OBJ ${RUNTIME_SRCS_MEM_OBJ})
