/*
 * Copyright (C) 2019 Intel Corporation
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
cl_int CommandQueueHw<Family>::enqueueReadWriteBufferOnCpuWithMemoryTransfer(cl_command_type commandType, Buffer *buffer,
                                                                             size_t offset, size_t size, void *ptr, cl_uint numEventsInWaitList,
                                                                             const cl_event *eventWaitList, cl_event *event) {
    cl_int retVal = CL_SUCCESS;
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    TransferProperties transferProperties(buffer, commandType, 0, true, &offset, &size, ptr, true);
    cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    return retVal;
}

template <typename Family>
cl_int CommandQueueHw<Family>::enqueueReadWriteBufferOnCpuWithoutMemoryTransfer(cl_command_type commandType, Buffer *buffer,
                                                                                size_t offset, size_t size, void *ptr, cl_uint numEventsInWaitList,
                                                                                const cl_event *eventWaitList, cl_event *event) {
    cl_int retVal = CL_SUCCESS;
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    TransferProperties transferProperties(buffer, CL_COMMAND_MARKER, 0, true, &offset, &size, ptr, false);
    cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    if (event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(commandType);
    }

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHintForMemoryTransfer(commandType, false, static_cast<cl_mem>(buffer), ptr);
    }
    return retVal;
}

template <typename Family>
cl_int CommandQueueHw<Family>::enqueueMarkerForReadWriteOperation(MemObj *memObj, void *ptr, cl_command_type commandType, cl_bool blocking, cl_uint numEventsInWaitList,
                                                                  const cl_event *eventWaitList, cl_event *event) {
    MultiDispatchInfo multiDispatchInfo;
    NullSurface s;
    Surface *surfaces[] = {&s};
    enqueueHandler<CL_COMMAND_MARKER>(
        surfaces,
        blocking == CL_TRUE,
        multiDispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);
    if (event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(commandType);
    }

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHintForMemoryTransfer(commandType, false, static_cast<cl_mem>(memObj), ptr);
    }

    return CL_SUCCESS;
}

template <typename Family>
cl_int CommandQueueHw<Family>::enqueueReadWriteBufferWithBlitTransfer(cl_command_type commandType, Buffer *buffer, bool blocking,
                                                                      size_t offset, size_t size, void *ptr, cl_uint numEventsInWaitList,
                                                                      const cl_event *eventWaitList, cl_event *event) {
    auto blitCommandStreamReceiver = context->getCommandStreamReceiverForBlitOperation(*buffer);
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
    TimestampPacketContainer previousTimestampPacketNodes;
    CsrDependencies csrDependencies;

    csrDependencies.fillFromEventsRequestAndMakeResident(eventsRequest, *blitCommandStreamReceiver,
                                                         CsrDependencies::DependenciesType::All);

    obtainNewTimestampPacketNodes(1, previousTimestampPacketNodes, queueDependenciesClearRequired());
    csrDependencies.push_back(&previousTimestampPacketNodes);

    auto copyDirection = (CL_COMMAND_WRITE_BUFFER == commandType) ? BlitterConstants::BlitWithHostPtrDirection::FromHostPtr
                                                                  : BlitterConstants::BlitWithHostPtrDirection::ToHostPtr;
    blitCommandStreamReceiver->blitWithHostPtr(*buffer, ptr, blocking, offset, size, copyDirection, csrDependencies, *timestampPacketContainer);

    MultiDispatchInfo multiDispatchInfo;

    if (CL_COMMAND_WRITE_BUFFER == commandType) {
        enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr, 0, blocking, true, multiDispatchInfo, numEventsInWaitList, eventWaitList, event);
    } else {
        enqueueHandler<CL_COMMAND_READ_BUFFER>(nullptr, 0, blocking, true, multiDispatchInfo, numEventsInWaitList, eventWaitList, event);
    }

    return CL_SUCCESS;
}

} // namespace NEO
