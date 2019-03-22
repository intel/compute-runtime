/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/enqueue_barrier.h"
#include "runtime/command_queue/enqueue_copy_buffer.h"
#include "runtime/command_queue/enqueue_copy_buffer_rect.h"
#include "runtime/command_queue/enqueue_copy_buffer_to_image.h"
#include "runtime/command_queue/enqueue_copy_image.h"
#include "runtime/command_queue/enqueue_copy_image_to_buffer.h"
#include "runtime/command_queue/enqueue_fill_buffer.h"
#include "runtime/command_queue/enqueue_fill_image.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_marker.h"
#include "runtime/command_queue/enqueue_migrate_mem_objects.h"
#include "runtime/command_queue/enqueue_read_buffer.h"
#include "runtime/command_queue/enqueue_read_buffer_rect.h"
#include "runtime/command_queue/enqueue_read_image.h"
#include "runtime/command_queue/enqueue_svm.h"
#include "runtime/command_queue/enqueue_write_buffer.h"
#include "runtime/command_queue/enqueue_write_buffer_rect.h"
#include "runtime/command_queue/enqueue_write_image.h"
#include "runtime/command_queue/finish.h"
#include "runtime/command_queue/flush.h"
#include "runtime/command_queue/gpgpu_walker.h"

namespace NEO {
template <typename Family>
void CommandQueueHw<Family>::notifyEnqueueReadBuffer(Buffer *buffer, bool blockingRead) {
    if (DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.get()) {
        buffer->getGraphicsAllocation()->setAllocDumpable(blockingRead);
        buffer->forceDisallowCPUCopy = blockingRead;
    }
}
template <typename Family>
void CommandQueueHw<Family>::notifyEnqueueReadImage(Image *image, bool blockingRead) {
    if (DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.get()) {
        image->getGraphicsAllocation()->setAllocDumpable(blockingRead);
    }
}
template <typename Family>
bool CommandQueueHw<Family>::requiresCacheFlushAfterWalkerBasedOnProperties(const cl_queue_properties *properties) {
    return false;
}
template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForCacheFlush(Surface **surfaces,
                                                             size_t numSurfaces,
                                                             LinearStream *commandStream) {
}
template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isCacheFlushCommand(uint32_t commandType) {
    return false;
}
} // namespace NEO
