/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

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

    if (svmData->gpuAllocations.getAllocationType() == AllocationType::svmZeroCopy) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_SVM_MAP_DOESNT_REQUIRE_COPY_DATA, svmPtr);
        }

        return enqueueHandler<CL_COMMAND_SVM_MAP>(surfaces,
                                                  blocking,
                                                  MultiDispatchInfo(),
                                                  numEventsInWaitList,
                                                  eventWaitList,
                                                  event);
    } else {
        auto svmOperation = context->getSVMAllocsManager()->getSvmMapOperation(svmPtr);
        if (svmOperation) {
            NullSurface s;
            Surface *surfaces[] = {&s};
            return enqueueHandler<CL_COMMAND_SVM_MAP>(surfaces,
                                                      blocking,
                                                      MultiDispatchInfo(),
                                                      numEventsInWaitList,
                                                      eventWaitList,
                                                      event);
        }

        CsrSelectionArgs csrSelectionArgs{CL_COMMAND_READ_BUFFER, &svmData->gpuAllocations, {}, device->getRootDeviceIndex(), &size};
        CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

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
        dc.bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);
        dc.direction = csrSelectionArgs.direction;

        MultiDispatchInfo dispatchInfo(dc);
        const bool isStateless = forceStateless(svmData->size);
        const bool useHeapless = this->getHeaplessModeEnabled();
        auto eBuiltInOps = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(isStateless, useHeapless);
        const auto dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, eBuiltInOps, numEventsInWaitList, eventWaitList, event, blocking, csr);
        if (dispatchResult != CL_SUCCESS) {
            return dispatchResult;
        }

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

    if (svmData->gpuAllocations.getAllocationType() == AllocationType::svmZeroCopy) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        return enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                                    false,
                                                    MultiDispatchInfo(),
                                                    numEventsInWaitList,
                                                    eventWaitList,
                                                    event);
    } else {
        auto svmOperation = context->getSVMAllocsManager()->getSvmMapOperation(svmPtr);
        if (!svmOperation) {
            NullSurface s;
            Surface *surfaces[] = {&s};
            return enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                                        false,
                                                        MultiDispatchInfo(),
                                                        numEventsInWaitList,
                                                        eventWaitList,
                                                        event);
        }
        if (svmOperation->readOnlyMap) {
            NullSurface s;
            Surface *surfaces[] = {&s};
            const auto enqueueResult = enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                                                            false,
                                                                            MultiDispatchInfo(),
                                                                            numEventsInWaitList,
                                                                            eventWaitList,
                                                                            event);

            context->getSVMAllocsManager()->removeSvmMapOperation(svmPtr);
            return enqueueResult;
        }

        CsrSelectionArgs csrSelectionArgs{CL_COMMAND_READ_BUFFER, {}, &svmData->gpuAllocations, device->getRootDeviceIndex(), &svmOperation->regionSize};
        CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

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
        dc.bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, svmOperation->regionSize, csr);
        dc.direction = csrSelectionArgs.direction;

        MultiDispatchInfo dispatchInfo(dc);
        const bool isStateless = forceStateless(svmData->size);
        const bool useHeapless = this->getHeaplessModeEnabled();
        auto eBuiltInOps = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(isStateless, useHeapless);
        const auto dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, eBuiltInOps, numEventsInWaitList, eventWaitList, event, false, csr);
        if (dispatchResult != CL_SUCCESS) {
            return dispatchResult;
        }

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

    const auto enqueueResult = enqueueHandler<CL_COMMAND_SVM_FREE>(surfaces,
                                                                   false,
                                                                   MultiDispatchInfo(),
                                                                   numEventsInWaitList,
                                                                   eventWaitList,
                                                                   retEvent);
    if (enqueueResult != CL_SUCCESS) {
        delete pFreeData;

        if (ownsEventDeletion) {
            castToObjectOrAbort<Event>(*retEvent)->release();
            retEvent = nullptr;
        }

        return enqueueResult;
    }

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

template <typename PtrType>
inline std::tuple<SvmAllocationData *, GraphicsAllocation *, PtrType> getExistingAlloc(Context *context,
                                                                                       PtrType ptr,
                                                                                       size_t size,
                                                                                       uint32_t rootDeviceIndex) {
    SvmAllocationData *svmData = context->getSVMAllocsManager()->getSVMAlloc(ptr);
    GraphicsAllocation *allocation = nullptr;
    if (svmData) {
        allocation = svmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        UNRECOVERABLE_IF(!allocation);
    } else {
        context->tryGetExistingMapAllocation(ptr, size, allocation);
        if (allocation) {
            ptr = CommandQueue::convertAddressWithOffsetToGpuVa(ptr, InternalMemoryType::notSpecified, *allocation);
        }
    }
    return std::make_tuple(svmData, allocation, ptr);
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMemcpy(cl_bool blockingCopy,
                                                   void *dstPtr,
                                                   const void *srcPtr,
                                                   size_t size,
                                                   cl_uint numEventsInWaitList,
                                                   const cl_event *eventWaitList,
                                                   cl_event *event, CommandStreamReceiver *csrParam) {

    if ((dstPtr == nullptr) || (srcPtr == nullptr)) {
        return CL_INVALID_VALUE;
    }
    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    auto [dstSvmData, dstAllocation, dstGpuPtr] = getExistingAlloc(context, dstPtr, size, rootDeviceIndex);
    auto [srcSvmData, srcAllocation, srcGpuPtr] = getExistingAlloc(context, srcPtr, size, rootDeviceIndex);

    enum CopyType { HostToHost,
                    SvmToHost,
                    HostToSvm,
                    SvmToSvm };
    CopyType copyType = HostToHost;
    if ((srcAllocation != nullptr) && (dstAllocation != nullptr)) {
        copyType = SvmToSvm;
    } else if ((srcAllocation == nullptr) && (dstAllocation != nullptr)) {
        copyType = HostToSvm;
    } else if (srcAllocation != nullptr) {
        copyType = SvmToHost;
    }

    auto pageFaultManager = context->getMemoryManager()->getPageFaultManager();
    if (dstSvmData && pageFaultManager) {
        UNRECOVERABLE_IF(dstAllocation == nullptr);
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(dstAllocation->getGpuAddress()));
    }
    if (srcSvmData && pageFaultManager) {
        UNRECOVERABLE_IF(srcAllocation == nullptr);
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(srcAllocation->getGpuAddress()));
    }

    bool isStatelessRequired =
        isForceStateless ||
        (srcSvmData != nullptr && forceStateless(srcSvmData->size)) ||
        (dstSvmData != nullptr && forceStateless(dstSvmData->size));

    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(isStatelessRequired, useHeapless);

    auto selectCsr = [csrParam, this](CsrSelectionArgs &csrSelectionArgs) -> CommandStreamReceiver & {
        return csrParam ? *csrParam : selectCsrForBuiltinOperation(csrSelectionArgs);
    };
    MultiDispatchInfo dispatchInfo;
    BuiltinOpParams operationParams;
    Surface *surfaces[2];
    cl_int dispatchResult = CL_SUCCESS;

    if (copyType == SvmToHost) {
        CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, srcAllocation, {}, device->getRootDeviceIndex(), &size};
        CommandStreamReceiver &csr = selectCsr(csrSelectionArgs);

        GeneralSurface srcSvmSurf(srcAllocation);
        HostPtrSurface dstHostPtrSurf(dstGpuPtr, size);

        auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);

        if (size != 0) {
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(dstHostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            dstGpuPtr = reinterpret_cast<void *>(dstHostPtrSurf.getAllocation()->getGpuAddress());
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *dstHostPtrSurf.getAllocation());

            notifyEnqueueSVMMemcpy(srcAllocation, !!blockingCopy, EngineHelpers::isBcs(csr.getOsContext().getEngineType()));
        }
        setOperationParams(operationParams, size, srcGpuPtr, srcAllocation, dstGpuPtr, dstHostPtrSurf.getAllocation());
        surfaces[0] = &srcSvmSurf;
        surfaces[1] = &dstHostPtrSurf;

        operationParams.bcsSplit = bcsSplit;
        operationParams.direction = csrSelectionArgs.direction;
        dispatchInfo.setBuiltinOpParams(operationParams);
        dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy, csr);
    } else if (copyType == HostToSvm) {
        CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, {}, dstAllocation, device->getRootDeviceIndex(), &size};
        CommandStreamReceiver &csr = selectCsr(csrSelectionArgs);

        HostPtrSurface srcHostPtrSurf(const_cast<void *>(srcGpuPtr), size, true);
        GeneralSurface dstSvmSurf(dstAllocation);

        auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);

        if (size != 0) {
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(srcHostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcGpuPtr = reinterpret_cast<void *>(srcHostPtrSurf.getAllocation()->getGpuAddress());
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *srcHostPtrSurf.getAllocation());
        }
        setOperationParams(operationParams, size, srcGpuPtr, srcHostPtrSurf.getAllocation(), dstGpuPtr, dstAllocation);
        surfaces[0] = &dstSvmSurf;
        surfaces[1] = &srcHostPtrSurf;

        operationParams.bcsSplit = bcsSplit;
        operationParams.direction = csrSelectionArgs.direction;
        dispatchInfo.setBuiltinOpParams(operationParams);
        dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy, csr);
    } else if (copyType == SvmToSvm) {
        CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, srcAllocation, dstAllocation, device->getRootDeviceIndex(), &size};
        CommandStreamReceiver &csr = selectCsr(csrSelectionArgs);

        GeneralSurface srcSvmSurf(srcAllocation);
        GeneralSurface dstSvmSurf(dstAllocation);
        setOperationParams(operationParams, size, srcGpuPtr, srcAllocation, dstGpuPtr, dstAllocation);
        surfaces[0] = &srcSvmSurf;
        surfaces[1] = &dstSvmSurf;

        operationParams.bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);
        operationParams.direction = csrSelectionArgs.direction;
        dispatchInfo.setBuiltinOpParams(operationParams);
        dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_SVM_MEMCPY>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy, csr);
    } else {
        CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, &size};
        CommandStreamReceiver &csr = selectCsr(csrSelectionArgs);

        HostPtrSurface srcHostPtrSurf(const_cast<void *>(srcGpuPtr), size);
        HostPtrSurface dstHostPtrSurf(dstGpuPtr, size);

        auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);

        if (size != 0) {
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(srcHostPtrSurf, false);
            status &= selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(dstHostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcGpuPtr = reinterpret_cast<void *>(srcHostPtrSurf.getAllocation()->getGpuAddress());
            dstGpuPtr = reinterpret_cast<void *>(dstHostPtrSurf.getAllocation()->getGpuAddress());
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *srcHostPtrSurf.getAllocation());
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *dstHostPtrSurf.getAllocation());
        }
        setOperationParams(operationParams, size, srcGpuPtr, srcHostPtrSurf.getAllocation(), dstGpuPtr, dstHostPtrSurf.getAllocation());
        surfaces[0] = &srcHostPtrSurf;
        surfaces[1] = &dstHostPtrSurf;

        operationParams.bcsSplit = bcsSplit;
        operationParams.direction = csrSelectionArgs.direction;
        dispatchInfo.setBuiltinOpParams(operationParams);
        dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingCopy, csr);
    }
    if (event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(CL_COMMAND_SVM_MEMCPY);
    }

    return dispatchResult;
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

    auto commandStreamReceiverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
    auto storageWithAllocations = getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    auto allocationType = AllocationType::fillPattern;
    auto patternAllocation = storageWithAllocations->obtainReusableAllocation(patternSize, allocationType).release();
    commandStreamReceiverOwnership.unlock();

    if (!patternAllocation) {
        patternAllocation = memoryManager->allocateGraphicsMemoryWithProperties({getDevice().getRootDeviceIndex(), patternSize, allocationType, getDevice().getDeviceBitfield()});
    }

    if (patternSize == 1) {
        auto patternValue = *reinterpret_cast<const uint8_t *>(pattern);
        int patternInt = static_cast<uint32_t>((patternValue << 24) | (patternValue << 16) | (patternValue << 8) | patternValue);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(uint32_t), &patternInt, sizeof(uint32_t));
    } else if (patternSize == 2) {
        auto patternValue = *reinterpret_cast<const uint16_t *>(pattern);
        int patternInt = static_cast<uint32_t>((patternValue << 16) | patternValue);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(uint32_t), &patternInt, sizeof(uint32_t));
    } else {
        memcpy_s(patternAllocation->getUnderlyingBuffer(), patternSize, pattern, patternSize);
    }

    const bool isStateless = forceStateless(svmData->size);
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::fillBuffer>(isStateless, useHeapless);

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

    const auto enqueueResult = enqueueHandler<CL_COMMAND_SVM_MEMFILL>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    storageWithAllocations->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(patternAllocation), REUSABLE_ALLOCATION, taskCount);

    return enqueueResult;
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

    return enqueueHandler<CL_COMMAND_SVM_MIGRATE_MEM>(surfaces,
                                                      false,
                                                      MultiDispatchInfo(),
                                                      numEventsInWaitList,
                                                      eventWaitList,
                                                      event);
}

} // namespace NEO
