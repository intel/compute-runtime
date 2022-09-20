/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/unified_memory/unified_memory.h"

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <map>
#include <vector>

struct _ze_command_list_handle_t {};

namespace L0 {
struct Device;
struct EventPool;
struct Event;
struct Kernel;
struct CommandQueue;

struct CmdListKernelLaunchParams {
    bool isIndirect = false;
    bool isPredicate = false;
    bool isCooperative = false;
    bool isKernelSplitOperation = false;
    bool isBuiltInKernel = false;
    bool isDestinationAllocationInSystemMemory = false;
};

struct CmdListReturnPoint {
    NEO::StreamProperties configSnapshot;
    uint64_t gpuAddress = 0;
    NEO::GraphicsAllocation *currentCmdBuffer = nullptr;
};

struct CommandList : _ze_command_list_handle_t {
    static constexpr uint32_t defaultNumIddsPerBlock = 64u;
    static constexpr uint32_t commandListimmediateIddsPerBlock = 1u;

    CommandList() = delete;
    CommandList(uint32_t numIddsPerBlock);

    template <typename Type>
    struct Allocator {
        static CommandList *allocate(uint32_t numIddsPerBlock) { return new Type(numIddsPerBlock); }
    };

    struct CommandToPatch {
        enum CommandType {
            FrontEndState,
            PauseOnEnqueueSemaphoreStart,
            PauseOnEnqueueSemaphoreEnd,
            PauseOnEnqueuePipeControlStart,
            PauseOnEnqueuePipeControlEnd,
            Invalid
        };
        void *pDestination = nullptr;
        void *pCommand = nullptr;
        CommandType type = Invalid;
    };
    using CommandsToPatch = StackVec<CommandToPatch, 16>;

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
    virtual ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle, const ze_group_count_t *threadGroupDimensions,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                           const CmdListKernelLaunchParams &launchParams) = 0;
    virtual ze_result_t appendLaunchCooperativeKernel(ze_kernel_handle_t kernelHandle,
                                                      const ze_group_count_t *launchKernelArgs,
                                                      ze_event_handle_t hSignalEvent,
                                                      uint32_t numWaitEvents,
                                                      ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                                   const ze_group_count_t *pDispatchArgumentsBuffer,
                                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                   ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendLaunchMultipleKernelsIndirect(uint32_t numKernels, const ze_kernel_handle_t *kernelHandles,
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
                                               ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                         size_t patternSize, size_t size, ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendMemoryPrefetch(const void *ptr, size_t count) = 0;
    virtual ze_result_t appendSignalEvent(ze_event_handle_t hEvent) = 0;
    virtual ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent) = 0;
    virtual ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                                   uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc,
                                                    const void *srcptr, size_t size, ze_event_handle_t hSignalEvent,
                                                    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;

    virtual void *asMutable() { return nullptr; };

    virtual ze_result_t reserveSpace(size_t size, void **ptr) = 0;
    virtual ze_result_t reset() = 0;

    virtual ze_result_t appendMetricMemoryBarrier() = 0;
    virtual ze_result_t appendMetricStreamerMarker(zet_metric_streamer_handle_t hMetricStreamer,
                                                   uint32_t value) = 0;
    virtual ze_result_t appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) = 0;
    virtual ze_result_t appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery, ze_event_handle_t hSignalEvent,
                                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;

    virtual ze_result_t appendQueryKernelTimestamps(uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr,
                                                    const size_t *pOffsets, ze_event_handle_t hSignalEvent,
                                                    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;

    virtual ze_result_t appendMILoadRegImm(uint32_t reg, uint32_t value) = 0;
    virtual ze_result_t appendMILoadRegReg(uint32_t reg1, uint32_t reg2) = 0;
    virtual ze_result_t appendMILoadRegMem(uint32_t reg1, uint64_t address) = 0;
    virtual ze_result_t appendMIStoreRegMem(uint32_t reg1, uint64_t address) = 0;
    virtual ze_result_t appendMIMath(void *aluArray, size_t aluCount) = 0;
    virtual ze_result_t appendMIBBStart(uint64_t address, size_t predication, bool secondLevel) = 0;
    virtual ze_result_t appendMIBBEnd() = 0;
    virtual ze_result_t appendMINoop() = 0;
    virtual ze_result_t appendPipeControl(void *dstPtr, uint64_t value) = 0;
    virtual ze_result_t appendWaitOnMemory(void *desc, void *ptr,
                                           uint32_t data, ze_event_handle_t hSignalEvent) = 0;
    virtual ze_result_t appendWriteToMemory(void *desc, void *ptr,
                                            uint64_t data) = 0;

    static CommandList *create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                               ze_command_list_flags_t flags, ze_result_t &resultValue);
    static CommandList *createImmediate(uint32_t productFamily, Device *device,
                                        const ze_command_queue_desc_t *desc,
                                        bool internalUsage, NEO::EngineGroupType engineGroupType,
                                        ze_result_t &resultValue);

    static CommandList *fromHandle(ze_command_list_handle_t handle) {
        return static_cast<CommandList *>(handle);
    }

    inline ze_command_list_handle_t toHandle() { return this; }

    uint32_t getCommandListPerThreadScratchSize() const {
        return commandListPerThreadScratchSize;
    }

    void setCommandListPerThreadScratchSize(uint32_t size) {
        commandListPerThreadScratchSize = size;
    }

    uint32_t getCommandListPerThreadPrivateScratchSize() const {
        return commandListPerThreadPrivateScratchSize;
    }

    void setCommandListPerThreadPrivateScratchSize(uint32_t size) {
        commandListPerThreadPrivateScratchSize = size;
    }

    uint32_t getCommandListSLMEnable() const {
        return commandListSLMEnabled;
    }

    void setCommandListSLMEnable(bool isSLMEnabled) {
        commandListSLMEnabled = isSLMEnabled;
    }

    NEO::PreemptionMode getCommandListPreemptionMode() const {
        return commandListPreemptionMode;
    }

    UnifiedMemoryControls getUnifiedMemoryControls() const {
        return unifiedMemoryControls;
    }

    bool hasIndirectAllocationsAllowed() const {
        return indirectAllocationsAllowed;
    }

    NEO::PreemptionMode obtainKernelPreemptionMode(Kernel *kernel);

    std::vector<Kernel *> &getPrintfKernelContainer() {
        return this->printfKernelContainer;
    }

    void storePrintfKernel(Kernel *kernel);
    void removeDeallocationContainerData();
    void removeHostPtrAllocations();
    void eraseDeallocationContainerEntry(NEO::GraphicsAllocation *allocation);
    void eraseResidencyContainerEntry(NEO::GraphicsAllocation *allocation);
    bool isCopyOnly() const {
        return NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType);
    }
    bool isInternal() const {
        return internalUsage;
    }
    bool containsCooperativeKernels() const {
        return containsCooperativeKernelsFlag;
    }
    bool isMemoryPrefetchRequested() const {
        return performMemoryPrefetch;
    }

    enum CommandListType : uint32_t {
        TYPE_REGULAR = 0u,
        TYPE_IMMEDIATE = 1u
    };

    virtual ze_result_t executeCommandListImmediate(bool performMigration) = 0;
    virtual ze_result_t initialize(Device *device, NEO::EngineGroupType engineGroupType, ze_command_list_flags_t flags) = 0;
    virtual ~CommandList();
    NEO::CommandContainer commandContainer;
    bool getContainsStatelessUncachedResource() { return containsStatelessUncachedResource; }
    std::map<const void *, NEO::GraphicsAllocation *> &getHostPtrMap() {
        return hostPtrMap;
    };

    const NEO::StreamProperties &getRequiredStreamState() {
        return requiredStreamState;
    }
    const NEO::StreamProperties &getFinalStreamState() {
        return finalStreamState;
    }
    const CommandsToPatch &getCommandsToPatch() {
        return commandsToPatch;
    }

    std::vector<CmdListReturnPoint> &getReturnPoints() {
        return returnPoints;
    }

    uint32_t getReturnPointsSize() const {
        return static_cast<uint32_t>(returnPoints.size());
    }

    void makeResidentAndMigrate(bool);
    void migrateSharedAllocations();

    bool getSystolicModeSupport() const {
        return systolicModeSupport;
    }

    ze_context_handle_t hContext = nullptr;
    std::vector<Kernel *> printfKernelContainer;
    CommandQueue *cmdQImmediate = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    Device *device = nullptr;
    NEO::PreemptionMode commandListPreemptionMode = NEO::PreemptionMode::Initial;
    uint32_t cmdListType = CommandListType::TYPE_REGULAR;
    uint32_t commandListPerThreadScratchSize = 0u;
    uint32_t commandListPerThreadPrivateScratchSize = 0u;
    uint32_t partitionCount = 1;
    bool isFlushTaskSubmissionEnabled = false;
    bool isSyncModeQueue = false;
    bool isTbxMode = false;
    bool commandListSLMEnabled = false;
    bool requiresQueueUncachedMocs = false;
    bool isBcsSplitNeeded = false;

  protected:
    NEO::GraphicsAllocation *getAllocationFromHostPtrMap(const void *buffer, uint64_t bufferSize);
    NEO::GraphicsAllocation *getHostPtrAlloc(const void *buffer, uint64_t bufferSize, bool hostCopyAllowed);
    bool setupTimestampEventForMultiTile(Event *signalEvent);

    std::map<const void *, NEO::GraphicsAllocation *> hostPtrMap;
    std::vector<NEO::GraphicsAllocation *> ownedPrivateAllocations;
    std::vector<NEO::GraphicsAllocation *> patternAllocations;
    std::vector<CmdListReturnPoint> returnPoints;

    NEO::StreamProperties requiredStreamState{};
    NEO::StreamProperties finalStreamState{};
    CommandsToPatch commandsToPatch{};
    UnifiedMemoryControls unifiedMemoryControls;

    ze_command_list_flags_t flags = 0u;
    NEO::EngineGroupType engineGroupType;

    bool indirectAllocationsAllowed = false;
    bool internalUsage = false;
    bool containsCooperativeKernelsFlag = false;
    bool containsStatelessUncachedResource = false;
    bool performMemoryPrefetch = false;
    bool multiReturnPointCommandList = false;
    bool systolicModeSupport = false;
    bool pipelineSelectStateTracking = false;
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
