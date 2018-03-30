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

set(RUNTIME_SRCS_COMMAND_QUEUE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/command_queue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_queue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_hw.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_data_transfer_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_barrier.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_common.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer_rect.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_buffer_to_image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_copy_image_to_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_fill_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_fill_image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_marker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_migrate_mem_objects.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_buffer_rect.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_read_image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_svm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_buffer_rect.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_write_image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/finish.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flush.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gpgpu_walker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gpgpu_walker.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.h
  ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen_avx2.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen_sse4.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/local_work_size.cpp
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_COMMAND_QUEUE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_COMMAND_QUEUE ${RUNTIME_SRCS_COMMAND_QUEUE})
