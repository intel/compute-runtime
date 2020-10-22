/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/event/event.h"

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
    auto eventObject = castToObjectOrAbort<Event>(event);
    if (freeDt->clb == nullptr) {
        auto ctx = eventObject->getContext();
        for (cl_uint i = 0; i < freeDt->numSvmPointers; i++) {
            castToObjectOrAbort<Context>(ctx)->getSVMAllocsManager()->freeSVMAlloc(freeDt->svmPointers[i]);
        }
    } else {
        freeDt->clb(eventObject->getCommandQueue(), freeDt->numSvmPointers,
                    freeDt->svmPointers, freeDt->userData);
    }
    if (freeDt->ownsEventDeletion) {
        castToObjectOrAbort<Event>(event)->release();
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
                                                cl_event *event,
                                                bool externalAppCall) {

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmData == nullptr) {
        return CL_INVALID_VALUE;
    }
    bool blocking = blockingMap == CL_TRUE;

    if (svmData->gpuAllocations.getAllocationType() == GraphicsAllocation::AllocationType::SVM_ZERO_COPY) {
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

        auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(getDevice().getRootDeviceIndex());

        GeneralSurface dstSurface(svmData->cpuAllocation);
        GeneralSurface srcSurface(gpuAllocation);

        Surface *surfaces[] = {&dstSurface, &srcSurface};
        void *svmBasePtr = svmData->cpuAllocation->getUnderlyingBuffer();
        size_t svmOffset = ptrDiff(svmPtr, svmBasePtr);

        BuiltinOpParams dc;
        dc.dstPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddressToPatch());
        dc.dstSvmAlloc = svmData->cpuAllocation;
        dc.dstOffset = {svmOffset, 0, 0};
        dc.srcPtr = reinterpret_cast<void *>(gpuAllocation->getGpuAddressToPatch());
        dc.srcSvmAlloc = gpuAllocation;
        dc.srcOffset = {svmOffset, 0, 0};
        dc.size = {size, 0, 0};
        dc.unifiedMemoryArgsRequireMemSync = externalAppCall;

        MultiDispatchInfo dispatchInfo(dc);

        dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, EBuiltInOps::CopyBufferToBuffer, numEventsInWaitList, eventWaitList, event, blocking);

        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(CL_COMMAND_SVM_MAP);
        }
        bool readOnlyMap = (mapFlags == CL_MAP_READ);
        context->getSVMAllocsManager()->insertSvmMapOperation(svmPtr, size, svmBasePtr, svmOffset, readOnlyMap);
        dispatchInfo.backupUnifiedMemorySyncRequirement();

        return CL_SUCCESS;
    }
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMUnmap(void *svmPtr,
                                                  cl_uint numEventsInWaitList,
                                                  const cl_event *eventWaitList,
                                                  cl_event *event,
                                                  bool externalAppCall) {

    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmData == nullptr) {
        return CL_INVALID_VALUE;
    }

    if (svmData->gpuAllocations.getAllocationType() == GraphicsAllocation::AllocationType::SVM_ZERO_COPY) {
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

        auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(getDevice().getRootDeviceIndex());

        gpuAllocation->setAubWritable(true, GraphicsAllocation::defaultBank);
        gpuAllocation->setTbxWritable(true, GraphicsAllocation::defaultBank);

        GeneralSurface dstSurface(gpuAllocation);
        GeneralSurface srcSurface(svmData->cpuAllocation);

        Surface *surfaces[] = {&dstSurface, &srcSurface};

        BuiltinOpParams dc;
        dc.dstPtr = reinterpret_cast<void *>(gpuAllocation->getGpuAddressToPatch());
        dc.dstSvmAlloc = gpuAllocation;
        dc.dstOffset = {svmOperation->offset, 0, 0};
        dc.srcPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddressToPatch());
        dc.srcSvmAlloc = svmData->cpuAllocation;
        dc.srcOffset = {svmOperation->offset, 0, 0};
        dc.size = {svmOperation->regionSize, 0, 0};
        dc.unifiedMemoryArgsRequireMemSync = externalAppCall;

        MultiDispatchInfo dispatchInfo(dc);

        dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, EBuiltInOps::CopyBufferToBuffer, numEventsInWaitList, eventWaitList, event, false);

        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(CL_COMMAND_SVM_UNMAP);
        }
        context->getSVMAllocsManager()->removeSvmMapOperation(svmPtr);
        dispatchInfo.backupUnifiedMemorySyncRequirement();

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

    auto eventObject = castToObjectOrAbort<Event>(*retEvent);
    eventObject->addCallback(freeSvmEventClb, CL_COMPLETE, pFreeData);

    return CL_SUCCESS;
}

inline void setOperationParams(BuiltinOpParams &operationParams, size_t size,
                               const void *srcPtr, GraphicsAllocation *srcSvmAlloc,
                               void *dstPtr, GraphicsAllocation *dstSvmAlloc) {
    operationParams.size = {size, 0, 0};
    operationParams.srcPtr = const_cast<void *>(alignDown(srcPtr, 4));
    operationParams.srcSvmAlloc = srcSvmAlloc;
    operationParams.srcOffset = {ptrDiff(srcPtr, operationParams.srcPtr), 0, 0};
    operationParams.dstPtr = alignDown(dstPtr, 4);
    operationParams.dstSvmAlloc = dstSvmAlloc;
    operationParams.dstOffset = {ptrDiff(dstPtr, operationParams.dstPtr), 0, 0};
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMemcpy(cl_bool blockingCopy,
                                                   void *dstPtr,
                                                   const void *srcPtr,
                                                   size_t size,
                                                   cl_uint numEventsInWaitList,
                                                   const cl_event *eventWaitList,
                                                   cl_event *event) {

    if ((dstPtr == nullptr) || (srcPtr == nullptr)) {
        return CL_INVALID_VALUE;
    }
    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    auto dstSvmData = context->getSVMAllocsManager()->getSVMAlloc(dstPtr);
    auto srcSvmData = context->getSVMAllocsManager()->getSVMAlloc(srcPtr);

    enum CopyType { HostToHost,
                    SvmToHost,
                    HostToSvm,
                    SvmToSvm };
    CopyType copyType = HostToHost;
    if ((srcSvmData != nullptr) && (dstSvmData != nullptr)) {
        copyType = SvmToSvm;
    } else if ((srcSvmData == nullptr) && (dstSvmData != nullptr)) {
        copyType = HostToSvm;
    } else if (srcSvmData != nullptr) {
        copyType = SvmToHost;
    }

    auto pageFaultManager = context->getMemoryManager()->getPageFaultManager();
    if (dstSvmData && pageFaultManager) {
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(dstSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)->getGpuAddress()));
    }
    if (srcSvmData && pageFaultManager) {
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(srcSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)->getGpuAddress()));
    }

    auto isStatelessRequired = false;
    if (srcSvmData != nullptr) {
        isStatelessRequired = forceStateless(srcSvmData->size);
    }
    if (dstSvmData != nullptr) {
        isStatelessRequired |= forceStateless(dstSvmData->size);
    }

    auto builtInType = EBuiltInOps::CopyBufferToBuffer;
    if (isStatelessRequired) {
        builtInType = EBuiltInOps::CopyBufferToBufferStateless;
    }

    MultiDispatchInfo dispatchInfo;
    BuiltinOpParams operationParams;
    Surface *surfaces[2];
    cl_command_type cmdType;

    if (copyType == SvmToHost) {
        GeneralSurface srcSvmSurf(srcSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
        HostPtrSurface dstHostPtrSurf(dstPtr, size);
        cmdType = CL_COMMAND_READ_BUFFER;
        if (size != 0) {
            auto &csr = getCommandStreamReceiverByCommandType(cmdType);
            bool status = csr.createAllocationForHostSurface(dstHostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            dstPtr = reinterpret_cast<void *>(dstHostPtrSurf.getAllocation()->getGpuAddress());
            notifyEnqueueSVMMemcpy(srcSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex), !!blockingCopy, EngineHelpers::isBcs(csr.getOsContext().getEngineType()));
        }
        setOperationParams(operationParams, size, srcPtr, srcSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex), dstPtr, dstHostPtrSurf.getAllocation());
        surfaces[0] = &srcSvmSurf;
        surfaces[1] = &dstHostPtrSurf;

        dispatchInfo.setBuiltinOpParams(operationParams);

        dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy);

    } else if (copyType == HostToSvm) {
        HostPtrSurface srcHostPtrSurf(const_cast<void *>(srcPtr), size);
        GeneralSurface dstSvmSurf(dstSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
        cmdType = CL_COMMAND_WRITE_BUFFER;
        if (size != 0) {
            auto &csr = getCommandStreamReceiverByCommandType(cmdType);
            bool status = csr.createAllocationForHostSurface(srcHostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcPtr = reinterpret_cast<void *>(srcHostPtrSurf.getAllocation()->getGpuAddress());
        }
        setOperationParams(operationParams, size, srcPtr, srcHostPtrSurf.getAllocation(),
                           dstPtr, dstSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
        surfaces[0] = &dstSvmSurf;
        surfaces[1] = &srcHostPtrSurf;

        dispatchInfo.setBuiltinOpParams(operationParams);

        dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy);

    } else if (copyType == SvmToSvm) {
        GeneralSurface srcSvmSurf(srcSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
        GeneralSurface dstSvmSurf(dstSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
        setOperationParams(operationParams, size, srcPtr, srcSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex),
                           dstPtr, dstSvmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
        surfaces[0] = &srcSvmSurf;
        surfaces[1] = &dstSvmSurf;

        dispatchInfo.setBuiltinOpParams(operationParams);

        dispatchBcsOrGpgpuEnqueue<CL_COMMAND_SVM_MEMCPY>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy);

    } else {
        HostPtrSurface srcHostPtrSurf(const_cast<void *>(srcPtr), size);
        HostPtrSurface dstHostPtrSurf(dstPtr, size);
        cmdType = CL_COMMAND_WRITE_BUFFER;
        if (size != 0) {
            auto &csr = getCommandStreamReceiverByCommandType(cmdType);
            bool status = csr.createAllocationForHostSurface(srcHostPtrSurf, false);
            status &= csr.createAllocationForHostSurface(dstHostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcPtr = reinterpret_cast<void *>(srcHostPtrSurf.getAllocation()->getGpuAddress());
            dstPtr = reinterpret_cast<void *>(dstHostPtrSurf.getAllocation()->getGpuAddress());
        }
        setOperationParams(operationParams, size, srcPtr, srcHostPtrSurf.getAllocation(), dstPtr, dstHostPtrSurf.getAllocation());
        surfaces[0] = &srcHostPtrSurf;
        surfaces[1] = &dstHostPtrSurf;

        dispatchInfo.setBuiltinOpParams(operationParams);

        dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy);
    }
    if (event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(CL_COMMAND_SVM_MEMCPY);
    }
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
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(getDevice().getRootDeviceIndex());

    auto memoryManager = context->getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    auto pageFaultManager = memoryManager->getPageFaultManager();
    if (pageFaultManager) {
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(gpuAllocation->getGpuAddress()));
    }

    auto commandStreamReceieverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
    auto storageWithAllocations = getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    auto allocationType = GraphicsAllocation::AllocationType::FILL_PATTERN;
    auto patternAllocation = storageWithAllocations->obtainReusableAllocation(patternSize, allocationType).release();
    commandStreamReceieverOwnership.unlock();

    if (!patternAllocation) {
        patternAllocation = memoryManager->allocateGraphicsMemoryWithProperties({getDevice().getRootDeviceIndex(), patternSize, allocationType, getDevice().getDeviceBitfield()});
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

    auto builtInType = EBuiltInOps::FillBuffer;
    if (forceStateless(svmData->size)) {
        builtInType = EBuiltInOps::FillBufferStateless;
    }

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            this->getClDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    BuiltinOpParams operationParams;
    auto multiGraphicsAllocation = MultiGraphicsAllocation(getDevice().getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(patternAllocation);

    MemObj patternMemObj(this->context, 0, {}, 0, 0, alignUp(patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), std::move(multiGraphicsAllocation), false, false, true);

    void *alignedDstPtr = alignDown(svmPtr, 4);
    size_t dstPtrOffset = ptrDiff(svmPtr, alignedDstPtr);

    operationParams.srcMemObj = &patternMemObj;
    operationParams.dstPtr = alignedDstPtr;
    operationParams.dstSvmAlloc = gpuAllocation;
    operationParams.dstOffset = {dstPtrOffset, 0, 0};
    operationParams.size = {size, 0, 0};

    MultiDispatchInfo dispatchInfo(operationParams);
    builder.buildDispatchInfos(dispatchInfo);

    GeneralSurface s1(gpuAllocation);
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

    enqueueHandler<CL_COMMAND_SVM_MIGRATE_MEM>(surfaces,
                                               false,
                                               MultiDispatchInfo(),
                                               numEventsInWaitList,
                                               eventWaitList,
                                               event);

    return CL_SUCCESS;
}
} // namespace NEO
