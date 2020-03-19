/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/preemption_mode.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <vector>

struct _ze_command_list_handle_t {};

namespace L0 {
struct EventPool;
struct Event;
struct Kernel;

struct CommandList : _ze_command_list_handle_t {
    static constexpr uint32_t maxNumInterfaceDescriptorsPerMediaInterfaceDescriptorLoad = 62u;
    static constexpr uint32_t defaultNumIddsPerBlock = maxNumInterfaceDescriptorsPerMediaInterfaceDescriptorLoad;
    static constexpr uint32_t commandListimmediateIddsPerBlock = 1u;

    CommandList() {}
    CommandList(uint32_t numIddsPerBlock) : commandContainer(numIddsPerBlock) {}

    template <typename Type>
    struct Allocator {
        static CommandList *allocate(uint32_t numIddsPerBlock) { return new Type(numIddsPerBlock); }
    };

    virtual ze_result_t close() = 0;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t appendEventReset(ze_event_handle_t hEvent) = 0;
    virtual ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendMemoryRangesBarrier(uint32_t numRanges, const size_t *pRangeSizes,
                                                  const void **pRanges,
                                                  ze_event_handle_t hSignalEvent,
                                                  uint32_t numWaitEvents,
                                                  ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage, const void *srcptr,
                                                  const ze_image_region_t *pDstRegion,
                                                  ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                  ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendImageCopyToMemory(void *dstptr, ze_image_handle_t hSrcImage,
                                                const ze_image_region_t *pSrcRegion,
                                                ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendImageCopyRegion(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                              const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion,
                                              ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                              ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendImageCopy(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                        ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendLaunchFunction(ze_kernel_handle_t hFunction, const ze_group_count_t *pThreadGroupDimensions,
                                             ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendLaunchCooperativeKernel(ze_kernel_handle_t hKernel,
                                                      const ze_group_count_t *pLaunchFuncArgs,
                                                      ze_event_handle_t hSignalEvent,
                                                      uint32_t numWaitEvents,
                                                      ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendLaunchFunctionIndirect(ze_kernel_handle_t hFunction,
                                                     const ze_group_count_t *pDispatchArgumentsBuffer,
                                                     ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                     ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendLaunchMultipleFunctionsIndirect(uint32_t numFunctions, const ze_kernel_handle_t *phFunctions,
                                                              const uint32_t *pNumLaunchArguments,
                                                              const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hEvent,
                                                              uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendMemAdvise(ze_device_handle_t hDevice, const void *ptr, size_t size,
                                        ze_memory_advice_t advice) = 0;
    virtual ze_result_t appendMemoryCopy(void *dstptr, const void *srcptr, size_t size,
                                         ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr, size_t size, bool flushHost) = 0;
    virtual ze_result_t appendMemoryCopyRegion(void *dstPtr,
                                               const ze_copy_region_t *dstRegion,
                                               uint32_t dstPitch,
                                               uint32_t dstSlicePitch,
                                               const void *srcPtr,
                                               const ze_copy_region_t *srcRegion,
                                               uint32_t srcPitch,
                                               uint32_t srcSlicePitch,
                                               ze_event_handle_t hSignalEvent) = 0;
    virtual ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                         size_t patternSize, size_t size, ze_event_handle_t hEvent) = 0;
    virtual ze_result_t appendMemoryPrefetch(const void *ptr, size_t count) = 0;
    virtual ze_result_t appendSignalEvent(ze_event_handle_t hEvent) = 0;
    virtual ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent) = 0;
    virtual ze_result_t reserveSpace(size_t size, void **ptr) = 0;
    virtual ze_result_t reset() = 0;

    virtual ze_result_t appendMetricMemoryBarrier() = 0;
    virtual ze_result_t appendMetricTracerMarker(zet_metric_tracer_handle_t hMetricTracer,
                                                 uint32_t value) = 0;
    virtual ze_result_t appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) = 0;
    virtual ze_result_t appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery,
                                             ze_event_handle_t hCompletionEvent) = 0;

    virtual ze_result_t appendMILoadRegImm(uint32_t reg, uint32_t value) = 0;
    virtual ze_result_t appendMILoadRegReg(uint32_t reg1, uint32_t reg2) = 0;
    virtual ze_result_t appendMILoadRegMem(uint32_t reg1, uint64_t address) = 0;
    virtual ze_result_t appendMIStoreRegMem(uint32_t reg1, uint64_t address) = 0;
    virtual ze_result_t appendMIMath(void *aluArray, size_t aluCount) = 0;
    virtual ze_result_t appendMIBBStart(uint64_t address, size_t predication, bool secondLevel) = 0;
    virtual ze_result_t appendMIBBEnd() = 0;
    virtual ze_result_t appendMINoop() = 0;

    static CommandList *create(uint32_t productFamily, Device *device);
    static CommandList *createImmediate(uint32_t productFamily, Device *device,
                                        const ze_command_queue_desc_t *desc,
                                        bool internalUsage);

    static CommandList *fromHandle(ze_command_list_handle_t handle) {
        return static_cast<CommandList *>(handle);
    }

    inline ze_command_list_handle_t toHandle() { return this; }

    uint32_t getCommandListPerThreadScratchSize() const {
        return commandListPerThreadScratchSize;
    }

    NEO::PreemptionMode getCommandListPreemptionMode() const {
        return commandListPreemptionMode;
    }

    NEO::PreemptionMode obtainFunctionPreemptionMode(Kernel *function);

    std::vector<Kernel *> &getPrintfFunctionContainer() {
        return this->printfFunctionContainer;
    }

    void storePrintfFunction(Kernel *function);
    void removeDeallocationContainerData();
    void removeHostPtrAllocations();
    void eraseDeallocationContainerEntry(NEO::GraphicsAllocation *allocation);
    void eraseResidencyContainerEntry(NEO::GraphicsAllocation *allocation);

    enum CommandListType : uint32_t {
        TYPE_REGULAR = 0u,
        TYPE_IMMEDIATE = 1u
    };

    CommandQueue *cmdQImmediate = nullptr;
    uint32_t cmdListType = CommandListType::TYPE_REGULAR;
    const ze_command_queue_desc_t *cmdQImmediateDesc = nullptr;

    Device *device = nullptr;
    std::vector<Kernel *> printfFunctionContainer;

    virtual ze_result_t executeCommandListImmediate(bool performMigration) = 0;
    virtual bool initialize(Device *device) = 0;
    virtual ~CommandList();
    NEO::CommandContainer commandContainer;

  protected:
    std::map<const void *, NEO::GraphicsAllocation *> hostPtrMap;
    uint32_t commandListPerThreadScratchSize = 0u;
    NEO::PreemptionMode commandListPreemptionMode = NEO::PreemptionMode::Initial;
};

using CommandListAllocatorFn = CommandList *(*)(uint32_t);
extern CommandListAllocatorFn commandListFactory[];
extern CommandListAllocatorFn commandListFactoryImmediate[];

template <uint32_t productFamily, typename CommandListType>
struct CommandListPopulateFactory {
    CommandListPopulateFactory() {
        commandListFactory[productFamily] = CommandList::Allocator<CommandListType>::allocate;
    }
};

template <uint32_t productFamily, typename CommandListType>
struct CommandListImmediatePopulateFactory {
    CommandListImmediatePopulateFactory() {
        commandListFactoryImmediate[productFamily] = CommandList::Allocator<CommandListType>::allocate;
    }
};

} // namespace L0
