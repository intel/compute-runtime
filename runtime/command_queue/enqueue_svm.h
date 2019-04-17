/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/event/event.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/memory_manager/svm_memory_manager.h"

#include <new>

namespace NEO {

using SvmFreeClbT = void(CL_CALLBACK *)(cl_command_queue queue,
                                        cl_uint numSvmPointers,
                                        void *svmPointers[],
                                        void *userData);

struct SvmFreeUserData {
    cl_uint numSvmPointers;
    void **svmPointers;
    SvmFreeClbT clb;
    void *userData;
    bool ownsEventDeletion;

    SvmFreeUserData(cl_uint numSvmPointers,
                    void **svmPointers, SvmFreeClbT clb,
                    void *userData,
                    bool ownsEventDeletion)
        : numSvmPointers(numSvmPointers),
          svmPointers(svmPointers),
          clb(clb),
          userData(userData),
          ownsEventDeletion(ownsEventDeletion){};
};

inline void CL_CALLBACK freeSvmEventClb(cl_event event,
                                        cl_int commandExecCallbackType,
                                        void *usrData) {
    auto freeDt = reinterpret_cast<SvmFreeUserData *>(usrData);
    auto eventObject = castToObject<Event>(event);
    if (freeDt->clb == nullptr) {
        auto ctx = eventObject->getContext();
        for (cl_uint i = 0; i < freeDt->numSvmPointers; i++) {
            castToObject<Context>(ctx)->getSVMAllocsManager()->freeSVMAlloc(freeDt->svmPointers[i]);
        }
    } else {
        freeDt->clb(eventObject->getCommandQueue(), freeDt->numSvmPointers,
                    freeDt->svmPointers, freeDt->userData);
    }
    if (freeDt->ownsEventDeletion) {
        castToObject<Event>(event)->release();
    }
    delete freeDt;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMap(cl_bool blockingMap,
                                                cl_map_flags mapFlags,
                                                void *svmPtr,
                                                size_t size,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList,
                                                cl_event *event) {

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmData == nullptr) {
        return CL_INVALID_VALUE;
    }
    bool blocking = blockingMap == CL_TRUE;

    if (svmData->gpuAllocation->getAllocationType() == GraphicsAllocation::AllocationType::SVM_ZERO_COPY) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_SVM_MAP_DOESNT_REQUIRE_COPY_DATA, svmPtr);
        }

        enqueueHandler<CL_COMMAND_SVM_MAP>(surfaces,
                                           blocking,
                                           MultiDispatchInfo(),
                                           numEventsInWaitList,
                                           eventWaitList,
                                           event);

        return CL_SUCCESS;
    } else {
        auto svmOperation = context->getSVMAllocsManager()->getSvmMapOperation(svmPtr);
        if (svmOperation) {
            NullSurface s;
            Surface *surfaces[] = {&s};
            enqueueHandler<CL_COMMAND_SVM_MAP>(surfaces,
                                               blocking,
                                               MultiDispatchInfo(),
                                               numEventsInWaitList,
                                               eventWaitList,
                                               event);
            return CL_SUCCESS;
        }

        MultiDispatchInfo dispatchInfo;
        auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                            this->getContext(), this->getDevice());
        BuiltInOwnershipWrapper builtInLock(builder, this->context);

        GeneralSurface dstSurface(svmData->cpuAllocation);
        GeneralSurface srcSurface(svmData->gpuAllocation);

        Surface *surfaces[] = {&dstSurface, &srcSurface};
        void *svmBasePtr = svmData->cpuAllocation->getUnderlyingBuffer();
        size_t svmOffset = ptrDiff(svmPtr, svmBasePtr);

        BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
        dc.dstPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddressToPatch());
        dc.dstSvmAlloc = svmData->cpuAllocation;
        dc.dstOffset = {svmOffset, 0, 0};
        dc.srcPtr = reinterpret_cast<void *>(svmData->gpuAllocation->getGpuAddressToPatch());
        dc.srcSvmAlloc = svmData->gpuAllocation;
        dc.srcOffset = {svmOffset, 0, 0};
        dc.size = {size, 0, 0};
        builder.buildDispatchInfos(dispatchInfo, dc);

        enqueueHandler<CL_COMMAND_READ_BUFFER>(
            surfaces,
            blocking,
            dispatchInfo,
            numEventsInWaitList,
            eventWaitList,
            event);
        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(CL_COMMAND_SVM_MAP);
        }
        bool readOnlyMap = !isValueSet(mapFlags, CL_MAP_WRITE) && !isValueSet(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION);
        context->getSVMAllocsManager()->insertSvmMapOperation(svmPtr, size, svmBasePtr, svmOffset, readOnlyMap);

        return CL_SUCCESS;
    }
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMUnmap(void *svmPtr,
                                                  cl_uint numEventsInWaitList,
                                                  const cl_event *eventWaitList,
                                                  cl_event *event) {

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmData == nullptr) {
        return CL_INVALID_VALUE;
    }

    if (svmData->gpuAllocation->getAllocationType() == GraphicsAllocation::AllocationType::SVM_ZERO_COPY) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                             false,
                                             MultiDispatchInfo(),
                                             numEventsInWaitList,
                                             eventWaitList,
                                             event);

        return CL_SUCCESS;
    } else {
        auto svmOperation = context->getSVMAllocsManager()->getSvmMapOperation(svmPtr);
        if (!svmOperation) {
            NullSurface s;
            Surface *surfaces[] = {&s};
            enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                                 false,
                                                 MultiDispatchInfo(),
                                                 numEventsInWaitList,
                                                 eventWaitList,
                                                 event);
            return CL_SUCCESS;
        }
        if (svmOperation->readOnlyMap) {
            NullSurface s;
            Surface *surfaces[] = {&s};
            enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                                 false,
                                                 MultiDispatchInfo(),
                                                 numEventsInWaitList,
                                                 eventWaitList,
                                                 event);
            context->getSVMAllocsManager()->removeSvmMapOperation(svmPtr);
            return CL_SUCCESS;
        }

        MultiDispatchInfo dispatchInfo;
        auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                            this->getContext(), this->getDevice());
        BuiltInOwnershipWrapper builtInLock(builder, this->context);

        GeneralSurface dstSurface(svmData->gpuAllocation);
        GeneralSurface srcSurface(svmData->cpuAllocation);

        Surface *surfaces[] = {&dstSurface, &srcSurface};

        BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
        dc.dstPtr = reinterpret_cast<void *>(svmData->gpuAllocation->getGpuAddressToPatch());
        dc.dstSvmAlloc = svmData->gpuAllocation;
        dc.dstOffset = {svmOperation->offset, 0, 0};
        dc.srcPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddressToPatch());
        dc.srcSvmAlloc = svmData->cpuAllocation;
        dc.srcOffset = {svmOperation->offset, 0, 0};
        dc.size = {svmOperation->regionSize, 0, 0};
        builder.buildDispatchInfos(dispatchInfo, dc);

        enqueueHandler<CL_COMMAND_READ_BUFFER>(
            surfaces,
            false,
            dispatchInfo,
            numEventsInWaitList,
            eventWaitList,
            event);
        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(CL_COMMAND_SVM_UNMAP);
        }
        context->getSVMAllocsManager()->removeSvmMapOperation(svmPtr);

        return CL_SUCCESS;
    }
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMFree(cl_uint numSvmPointers,
                                                 void *svmPointers[],
                                                 SvmFreeClbT clb,
                                                 void *userData,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *retEvent) {
    cl_event event = nullptr;
    bool ownsEventDeletion = false;
    if (retEvent == nullptr) {
        ownsEventDeletion = true;
        retEvent = &event;
    }

    SvmFreeUserData *pFreeData = new SvmFreeUserData(numSvmPointers,
                                                     svmPointers,
                                                     clb,
                                                     userData,
                                                     ownsEventDeletion);

    NullSurface s;
    Surface *surfaces[] = {&s};

    enqueueHandler<CL_COMMAND_SVM_FREE>(surfaces,
                                        false,
                                        MultiDispatchInfo(),
                                        numEventsInWaitList,
                                        eventWaitList,
                                        retEvent);

    auto eventObject = castToObject<Event>(*retEvent);
    eventObject->addCallback(freeSvmEventClb, CL_COMPLETE, pFreeData);

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMemcpy(cl_bool blockingCopy,
                                                   void *dstPtr,
                                                   const void *srcPtr,
                                                   size_t size,
                                                   cl_uint numEventsInWaitList,
                                                   const cl_event *eventWaitList,
                                                   cl_event *event) {

    auto dstSvmData = context->getSVMAllocsManager()->getSVMAlloc(dstPtr);
    auto srcSvmData = context->getSVMAllocsManager()->getSVMAlloc(srcPtr);
    if ((dstSvmData == nullptr) || (srcSvmData == nullptr)) {
        return CL_INVALID_VALUE;
    }

    MultiDispatchInfo dispatchInfo;

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams operationParams;
    operationParams.srcPtr = const_cast<void *>(srcPtr);
    operationParams.dstPtr = dstPtr;
    operationParams.srcSvmAlloc = srcSvmData->gpuAllocation;
    operationParams.dstSvmAlloc = dstSvmData->gpuAllocation;
    operationParams.srcOffset = {0, 0, 0};
    operationParams.dstOffset = {0, 0, 0};
    operationParams.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, operationParams);

    GeneralSurface s1(srcSvmData->gpuAllocation), s2(dstSvmData->gpuAllocation);
    Surface *surfaces[] = {&s1, &s2};

    enqueueHandler<CL_COMMAND_SVM_MEMCPY>(
        surfaces,
        blockingCopy ? true : false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMemFill(void *svmPtr,
                                                    const void *pattern,
                                                    size_t patternSize,
                                                    size_t size,
                                                    cl_uint numEventsInWaitList,
                                                    const cl_event *eventWaitList,
                                                    cl_event *event) {

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmData == nullptr) {
        return CL_INVALID_VALUE;
    }

    auto memoryManager = getDevice().getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    auto commandStreamReceieverOwnership = getCommandStreamReceiver().obtainUniqueOwnership();
    auto storageWithAllocations = getCommandStreamReceiver().getInternalAllocationStorage();
    auto allocationType = GraphicsAllocation::AllocationType::FILL_PATTERN;
    auto patternAllocation = storageWithAllocations->obtainReusableAllocation(patternSize, allocationType).release();
    commandStreamReceieverOwnership.unlock();

    if (!patternAllocation) {
        patternAllocation = memoryManager->allocateGraphicsMemoryWithProperties({patternSize, allocationType});
    }

    if (patternSize == 1) {
        int patternInt = (uint32_t)((*(uint8_t *)pattern << 24) | (*(uint8_t *)pattern << 16) | (*(uint8_t *)pattern << 8) | *(uint8_t *)pattern);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(int), &patternInt, sizeof(int));
    } else if (patternSize == 2) {
        int patternInt = (uint32_t)((*(uint16_t *)pattern << 16) | *(uint16_t *)pattern);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(int), &patternInt, sizeof(int));
    } else {
        memcpy_s(patternAllocation->getUnderlyingBuffer(), patternSize, pattern, patternSize);
    }

    MultiDispatchInfo dispatchInfo;

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams operationParams;
    MemObj patternMemObj(this->context, 0, 0, alignUp(patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), patternAllocation, false, false, true);
    operationParams.srcMemObj = &patternMemObj;
    operationParams.dstPtr = svmPtr;
    operationParams.dstSvmAlloc = svmData->gpuAllocation;
    operationParams.dstOffset = {0, 0, 0};
    operationParams.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, operationParams);

    GeneralSurface s1(svmData->gpuAllocation);
    GeneralSurface s2(patternAllocation);
    Surface *surfaces[] = {&s1, &s2};

    enqueueHandler<CL_COMMAND_SVM_MEMFILL>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    storageWithAllocations->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(patternAllocation), REUSABLE_ALLOCATION, taskCount);

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMigrateMem(cl_uint numSvmPointers,
                                                       const void **svmPointers,
                                                       const size_t *sizes,
                                                       const cl_mem_migration_flags flags,
                                                       cl_uint numEventsInWaitList,
                                                       const cl_event *eventWaitList,
                                                       cl_event *event) {
    NullSurface s;
    Surface *surfaces[] = {&s};

    enqueueHandler<CL_COMMAND_MIGRATE_MEM_OBJECTS>(surfaces,
                                                   false,
                                                   MultiDispatchInfo(),
                                                   numEventsInWaitList,
                                                   eventWaitList,
                                                   event);

    return CL_SUCCESS;
}
} // namespace NEO
