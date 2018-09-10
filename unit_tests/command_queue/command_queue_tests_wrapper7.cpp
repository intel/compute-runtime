/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

//1
#include "unit_tests/command_queue/command_queue_flush_waitlist_tests.inl"
#include "unit_tests/command_queue/enqueue_barrier_tests.inl"
#include "unit_tests/command_queue/enqueue_copy_image_to_buffer_tests.inl"

// 2


// 3
#include "unit_tests/command_queue/enqueue_thread_tests.inl"
#include "unit_tests/command_queue/enqueue_marker_tests.inl"

// 4
#include "unit_tests/command_queue/enqueue_waitlist_tests.inl"
#include "unit_tests/command_queue/enqueue_write_buffer_event_tests.inl"
#include "unit_tests/command_queue/enqueue_write_buffer_rect_tests.inl"

// 5
#include "unit_tests/command_queue/get_command_queue_info_tests.inl"
#include "unit_tests/command_queue/get_size_required_image_tests.inl"
#include "unit_tests/command_queue/get_size_required_tests.inl"

// 6
#include "unit_tests/command_queue/enqueue_unmap_memobject_tests.inl"