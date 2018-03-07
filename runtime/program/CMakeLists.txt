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

set(RUNTIME_SRCS_PROGRAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/block_kernel_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/block_kernel_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/build.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compile.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/get_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/link.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patch_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print_formatter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/print_formatter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/process_elf_binary.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/process_gen_binary.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/process_spir_binary.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_PROGRAM})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_PROGRAM ${RUNTIME_SRCS_PROGRAM})
