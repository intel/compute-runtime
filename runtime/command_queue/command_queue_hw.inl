/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/enqueue_barrier.h"
#include "runtime/command_queue/enqueue_copy_buffer.h"
#include "runtime/command_queue/enqueue_copy_buffer_rect.h"
#include "runtime/command_queue/enqueue_copy_buffer_to_image.h"
#include "runtime/command_queue/enqueue_copy_image_to_buffer.h"
#include "runtime/command_queue/enqueue_copy_image.h"
#include "runtime/command_queue/enqueue_fill_buffer.h"
#include "runtime/command_queue/enqueue_fill_image.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_svm.h"
#include "runtime/command_queue/enqueue_marker.h"
#include "runtime/command_queue/enqueue_migrate_mem_objects.h"
#include "runtime/command_queue/enqueue_read_buffer.h"
#include "runtime/command_queue/enqueue_read_buffer_rect.h"
#include "runtime/command_queue/enqueue_read_image.h"
#include "runtime/command_queue/enqueue_write_buffer.h"
#include "runtime/command_queue/enqueue_write_buffer_rect.h"
#include "runtime/command_queue/enqueue_write_image.h"
#include "runtime/command_queue/finish.h"
#include "runtime/command_queue/flush.h"

namespace OCLRT {
template <typename Family>
void CommandQueueHw<Family>::notifyEnqueueReadBuffer(Buffer *buffer, bool blockingRead) {
}
template <typename Family>
void CommandQueueHw<Family>::notifyEnqueueReadImage(Image *image, bool blockingRead) {
}
} // namespace OCLRT
