/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper.h"

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/command_queue/enqueue_barrier.h"
#include "opencl/source/command_queue/enqueue_copy_buffer.h"
#include "opencl/source/command_queue/enqueue_copy_buffer_rect.h"
#include "opencl/source/command_queue/enqueue_copy_buffer_to_image.h"
#include "opencl/source/command_queue/enqueue_copy_image.h"
#include "opencl/source/command_queue/enqueue_copy_image_to_buffer.h"
#include "opencl/source/command_queue/enqueue_fill_buffer.h"
#include "opencl/source/command_queue/enqueue_fill_image.h"
#include "opencl/source/command_queue/enqueue_kernel.h"
#include "opencl/source/command_queue/enqueue_marker.h"
#include "opencl/source/command_queue/enqueue_migrate_mem_objects.h"
#include "opencl/source/command_queue/enqueue_read_buffer.h"
#include "opencl/source/command_queue/enqueue_read_buffer_rect.h"
#include "opencl/source/command_queue/enqueue_read_image.h"
#include "opencl/source/command_queue/enqueue_svm.h"
#include "opencl/source/command_queue/enqueue_write_buffer.h"
#include "opencl/source/command_queue/enqueue_write_buffer_rect.h"
#include "opencl/source/command_queue/enqueue_write_image.h"
#include "opencl/source/command_queue/finish.h"
#include "opencl/source/command_queue/flush.h"
#include "opencl/source/command_queue/gpgpu_walker.h"

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
void CommandQueueHw<Family>::dispatchAuxTranslationBuiltin(MultiDispatchInfo &multiDispatchInfo,
                                                           AuxTranslationDirection auxTranslationDirection) {
    if (HwHelperHw<Family>::getAuxTranslationMode() != AuxTranslationMode::Builtin) {
        return;
    }

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, getDevice());
    auto &auxTranslationBuilder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(builder);
    BuiltinOpParams dispatchParams;

    dispatchParams.auxTranslationDirection = auxTranslationDirection;

    auxTranslationBuilder.buildDispatchInfosForAuxTranslation<Family>(multiDispatchInfo, dispatchParams);
}

template <typename Family>
bool CommandQueueHw<Family>::forceStateless(size_t size) {
    return size >= 4ull * MemoryConstants::gigaByte;
}

template <typename Family>
bool CommandQueueHw<Family>::isCacheFlushForBcsRequired() const {
    return true;
}

template <typename Family>
void CommandQueueHw<Family>::setupBlitAuxTranslation(MultiDispatchInfo &multiDispatchInfo) {
    multiDispatchInfo.begin()->dispatchInitCommands.registerMethod(
        TimestampPacketHelper::programSemaphoreWithImplicitDependencyForAuxTranslation<Family, AuxTranslationDirection::AuxToNonAux>);

    multiDispatchInfo.begin()->dispatchInitCommands.registerCommandsSizeEstimationMethod(
        TimestampPacketHelper::getRequiredCmdStreamSizeForAuxTranslationNodeDependency<Family>);

    multiDispatchInfo.rbegin()->dispatchEpilogueCommands.registerMethod(
        TimestampPacketHelper::programSemaphoreWithImplicitDependencyForAuxTranslation<Family, AuxTranslationDirection::NonAuxToAux>);

    multiDispatchInfo.rbegin()->dispatchEpilogueCommands.registerCommandsSizeEstimationMethod(
        TimestampPacketHelper::getRequiredCmdStreamSizeForAuxTranslationNodeDependency<Family>);
}

template <typename Family>
bool CommandQueueHw<Family>::obtainTimestampPacketForCacheFlush(bool isCacheFlushRequired) const {
    return isCacheFlushRequired;
}

} // namespace NEO
