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

set(RUNTIME_SRCS_BUILT_INS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aux_translation_builtin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/builtins_dispatch_builder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_storage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/sip.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sip.h
  ${CMAKE_CURRENT_SOURCE_DIR}/vme_dispatch_builder.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_INS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_BUILT_INS ${RUNTIME_SRCS_BUILT_INS})

set(RUNTIME_SRCS_BUILT_IN_KERNELS
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/aux_translation.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_rect.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_buffer.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_buffer_to_image3d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image3d_to_buffer.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image1d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image2d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/copy_image_to_image3d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_buffer.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image1d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image2d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/fill_image3d.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_motion_estimate_intel.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_advanced_motion_estimate_check_intel.igdrcl_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.igdrcl_built_in
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_BUILT_IN_KERNELS})

if(NOT (TARGET ${BUILTINS_BINARIES_LIB_NAME}))
  include(builtins_binary.cmake)
endif()
