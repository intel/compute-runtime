/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/heap_base_address_model.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/cmdlist/command_to_patch.h"
#include "level_zero/core/source/helpers/api_handle_helper.h"
#include "level_zero/include/level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "copy_offload_mode.h"

#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

struct _ze_command_list_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_COMMAND_LIST> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_command_list_handle_t>);

namespace NEO {
class ScratchSpaceController;
class TagNodeBase;
struct EncodeDispatchKernelArgs;
} // namespace NEO

namespace L0 {
struct Device;
struct EventPool;
struct Event;
struct Kernel;
struct CommandQueue;
struct CmdListKernelLaunchParams;
struct CmdListMemoryCopyParams;

struct CmdListReturnPoint {
    NEO::StreamProperties configSnapshot;
    uint64_t gpuAddress = 0;
    NEO::GraphicsAllocation *currentCmdBuffer = nullptr;
};

struct MemAdviseOperation {
    ze_device_handle_t hDevice;
    const void *ptr;
    const size_t size;
    ze_memory_advice_t advice;
};

using AppendedMemAdviseOperations = std::vector<MemAdviseOperation>;

struct CommandList : _ze_command_list_handle_t {
    static constexpr uint32_t defaultNumIddsPerBlock = 64u;
    static constexpr uint32_t commandListimmediateIddsPerBlock = 1u;

    CommandList() = delete;
    CommandList(uint32_t numIddsPerBlock);

    template <typename Type>
    struct Allocator {
        static CommandList *allocate(uint32_t numIddsPerBlock) { return new Type(numIddsPerBlock); }
    };

    using CommandsToPatch = StackVec<CommandToPatch, 16>;
    using CmdListReturnPoints = StackVec<CmdListReturnPoint, 32>;

    virtual ze_result_t close() = 0;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t appendEventReset(ze_event_handle_t hEvent) = 0;
    virtual ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) = 0;
    virtual ze_result_t appendMemoryRangesBarrier(uint32_t numRanges, const size_t *pRangeSizes,
                                                  const void **pRanges,
                                                  ze_event_handle_t hSignalEvent,
                                                  uint32_t numWaitEvents,
                                                  ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage, const void *srcptr,
                                                  const ze_image_region_t *pDstRegion,
                                                  ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                  ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendImageCopyToMemory(void *dstptr, ze_image_handle_t hSrcImage,
                                                const ze_image_region_t *pSrcRegion,
                                                ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendImageCopyFromMemoryExt(ze_image_handle_t hDstImage, const void *srcptr,
                                                     const ze_image_region_t *pDstRegion,
                                                     uint32_t srcRowPitch, uint32_t srcSlicePitch,
                                                     ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                     ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendImageCopyToMemoryExt(void *dstptr, ze_image_handle_t hSrcImage,
                                                   const ze_image_region_t *pSrcRegion,
                                                   uint32_t destRowPitch, uint32_t destSlicePitch,
                                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                   ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendImageCopyRegion(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                              const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion,
                                              ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                              ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendImageCopy(ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
                                        ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle, const ze_group_count_t &threadGroupDimensions,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                           CmdListKernelLaunchParams &launchParams) = 0;
    virtual ze_result_t appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                                   const ze_group_count_t &pDispatchArgumentsBuffer,
                                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                   ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) = 0;
    virtual ze_result_t appendLaunchMultipleKernelsIndirect(uint32_t numKernels, const ze_kernel_handle_t *kernelHandles,
                                                            const uint32_t *pNumLaunchArguments,
                                                            const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hEvent,
                                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) = 0;
    virtual ze_result_t appendLaunchKernelWithArguments(ze_kernel_handle_t hKernel,
                                                        const ze_group_count_t groupCounts,
                                                        const ze_group_size_t groupSizes,

                                                        void **pArguments,
                                                        void *pNext,
                                                        ze_event_handle_t hSignalEvent,
                                                        uint32_t numWaitEvents,
                                                        ze_event_handle_t *phWaitEvents) = 0;

    virtual ze_result_t appendMemAdvise(ze_device_handle_t hDevice, const void *ptr, size_t size,
                                        ze_memory_advice_t advice) = 0;
    virtual ze_result_t executeMemAdvise(ze_device_handle_t hDevice,
                                         const void *ptr, size_t size,
                                         ze_memory_advice_t advice) = 0;
    virtual ze_result_t appendMemoryCopy(void *dstptr, const void *srcptr, size_t size,
                                         ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                         ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
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
                                               ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                         size_t patternSize, size_t size, ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) = 0;
    virtual ze_result_t appendMemoryPrefetch(const void *ptr, size_t count) = 0;
    virtual ze_result_t appendSignalEvent(ze_event_handle_t hEvent, bool relaxedOrderingDispatch) = 0;
    virtual ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                           bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) = 0;
    virtual ze_result_t appendWriteGlobalTimestamp(uint64_t *dstptr, ze_event_handle_t hSignalEvent,
                                                   uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendMemoryCopyFromContext(void *dstptr, ze_context_handle_t hContextSrc,
                                                    const void *srcptr, size_t size, ze_event_handle_t hSignalEvent,
                                                    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) = 0;

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

    virtual ze_result_t appendMILoadRegImm(uint32_t reg, uint32_t value, bool isBcs) = 0;
    virtual ze_result_t appendMILoadRegReg(uint32_t reg1, uint32_t reg2) = 0;
    virtual ze_result_t appendMILoadRegMem(uint32_t reg1, uint64_t address) = 0;
    virtual ze_result_t appendMIStoreRegMem(uint32_t reg1, uint64_t address) = 0;
    virtual ze_result_t appendMIMath(void *aluArray, size_t aluCount) = 0;
    virtual ze_result_t appendMIBBStart(uint64_t address, size_t predication, bool secondLevel) = 0;
    virtual ze_result_t appendMIBBEnd() = 0;
    virtual ze_result_t appendMINoop() = 0;
    virtual ze_result_t appendPipeControl(void *dstPtr, uint64_t value) = 0;
    virtual ze_result_t appendSoftwareTag(const char *data) = 0;
    virtual ze_result_t appendWaitOnMemory(void *desc, void *ptr, uint64_t data, ze_event_handle_t signalEventHandle, bool useQwordData) = 0;
    virtual ze_result_t appendWriteToMemory(void *desc, void *ptr,
                                            uint64_t data) = 0;
    virtual ze_result_t appendWaitExternalSemaphores(uint32_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                                     const ze_external_semaphore_wait_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                     uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t appendSignalExternalSemaphores(size_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                                       const ze_external_semaphore_signal_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                       uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t hostSynchronize(uint64_t timeout) = 0;

    virtual ze_result_t getDeviceHandle(ze_device_handle_t *phDevice) = 0;
    virtual ze_result_t getContextHandle(ze_context_handle_t *phContext) = 0;
    virtual ze_result_t getOrdinal(uint32_t *pOrdinal) = 0;
    virtual ze_result_t getImmediateIndex(uint32_t *pIndex) = 0;
    virtual ze_result_t isImmediate(ze_bool_t *pIsImmediate) = 0;

    virtual ze_result_t appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                           ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;

    static CommandList *create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                               ze_command_list_flags_t flags, ze_result_t &resultValue,
                               bool internalUsage);
    static CommandList *createImmediate(uint32_t productFamily, Device *device,
                                        const ze_command_queue_desc_t *desc,
                                        bool internalUsage, NEO::EngineGroupType engineGroupType,
                                        ze_result_t &resultValue);

    static CommandList *fromHandle(ze_command_list_handle_t handle) {
        return static_cast<CommandList *>(handle);
    }

    inline ze_command_list_handle_t toHandle() { return this; }

    uint32_t getCommandListPerThreadScratchSize(uint32_t slotId) const {
        return commandListPerThreadScratchSize[slotId];
    }

    void setAdditionalDispatchKernelArgsFromLaunchParams(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const CmdListKernelLaunchParams &launchParams) const;
    void setAdditionalDispatchKernelArgsFromKernel(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const Kernel *kernel) const;

    ze_result_t validateLaunchParams(const Kernel &kernel, const CmdListKernelLaunchParams &launchParams) const;

    void setOrdinal(uint32_t ord) { ordinal = ord; }
    void setCommandListPerThreadScratchSize(uint32_t slotId, uint32_t size) {
        UNRECOVERABLE_IF(slotId > 1);
        commandListPerThreadScratchSize[slotId] = size;
    }

    NEO::ScratchSpaceController *getCommandListUsedScratchController() const {
        return usedScratchController;
    }

    void setCommandListUsedScratchController(NEO::ScratchSpaceController *scratchController) {
        usedScratchController = scratchController;
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

    std::vector<std::weak_ptr<Kernel>> &getPrintfKernelContainer() {
        return this->printfKernelContainer;
    }

    void storePrintfKernel(Kernel *kernel);
    void removeDeallocationContainerData();
    void removeHostPtrAllocations();
    void removeMemoryPrefetchAllocations();
    void storeFillPatternResourcesForReuse();
    void eraseDeallocationContainerEntry(NEO::GraphicsAllocation *allocation);
    void eraseResidencyContainerEntry(NEO::GraphicsAllocation *allocation);
    bool isCopyOnly(bool copyOffloadOperation) const {
        return NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType) || (copyOffloadOperation && this->isCopyOffloadEnabled());
    }
    bool isCopyOffloadEnabled() const { return copyOffloadMode != CopyOffloadModes::disabled; }
    CopyOffloadMode getCopyOffloadModeForOperation(bool offloadAllowed) const { return offloadAllowed ? copyOffloadMode : CopyOffloadModes::disabled; }

    bool isInternal() const {
        return internalUsage;
    }
    bool containsCooperativeKernels() const {
        return containsCooperativeKernelsFlag;
    }
    bool isMemoryPrefetchRequested() const {
        return performMemoryPrefetch;
    }

    enum class CommandListType : uint32_t {
        typeRegular = 0u,
        typeImmediate = 1u
    };

    virtual ze_result_t initialize(Device *device, NEO::EngineGroupType engineGroupType, ze_command_list_flags_t flags) = 0;
    virtual ~CommandList();

    bool getContainsStatelessUncachedResource() { return containsStatelessUncachedResource; }
    std::map<const void *, NEO::GraphicsAllocation *> &getHostPtrMap() {
        return hostPtrMap;
    };

    const NEO::StreamProperties &getRequiredStreamState() const {
        return requiredStreamState;
    }
    const NEO::StreamProperties &getFinalStreamState() const {
        return finalStreamState;
    }
    NEO::StreamProperties &getRequiredStreamState() {
        return requiredStreamState;
    }
    NEO::StreamProperties &getFinalStreamState() {
        return finalStreamState;
    }
    const CommandsToPatch &getCommandsToPatch() {
        return commandsToPatch;
    }

    CmdListReturnPoints &getReturnPoints() {
        return returnPoints;
    }

    uint32_t getReturnPointsSize() const {
        return static_cast<uint32_t>(returnPoints.size());
    }

    void migrateSharedAllocations();

    bool getSystolicModeSupport() const {
        return systolicModeSupport;
    }

    NEO::PrefetchContext &getPrefetchContext() {
        return prefetchContext;
    }

    AppendedMemAdviseOperations &getMemAdviseOperations() {
        return memAdviseOperations;
    }

    NEO::HeapAddressModel getCmdListHeapAddressModel() const {
        return this->cmdListHeapAddressModel;
    }

    void setCmdListContext(ze_context_handle_t contextHandle) {
        this->hContext = contextHandle;
    }

    ze_context_handle_t getCmdListContext() const {
        return this->hContext;
    }

    uint32_t getPartitionCount() const {
        return this->partitionCount;
    }

    bool isImmediateType() const {
        return (this->cmdListType == CommandListType::typeImmediate);
    }

    bool isRequiredQueueUncachedMocs() const {
        return requiresQueueUncachedMocs;
    }

    Device *getDevice() const {
        return this->device;
    }

    NEO::CommandContainer &getCmdContainer() {
        return this->commandContainer;
    }

    NEO::CommandStreamReceiver *getCsr(bool copyOffload) const;

    bool hasKernelWithAssert() {
        return kernelWithAssertAppended;
    }

    bool isHeaplessModeEnabled() const {
        return heaplessModeEnabled;
    }

    bool isHeaplessStateInitEnabled() const {
        return heaplessStateInitEnabled;
    }

    virtual bool skipInOrderNonWalkerSignalingAllowed(ze_event_handle_t signalEvent) const { return false; }

    bool getCmdListBatchBufferFlag() const {
        return dispatchCmdListBatchBufferAsPrimary;
    }

    bool getCmdListStateBaseAddressTracking() const {
        return stateBaseAddressTracking;
    }

    bool getCmdListScratchAddressPatchingEnabled() const {
        return scratchAddressPatchingEnabled;
    }
    bool isTaskCountUpdateFenceRequired() const {
        return taskCountUpdateFenceRequired;
    }

    bool isStatelessBuiltinsEnabled() const {
        return statelessBuiltinsEnabled;
    }

    bool isTextureCacheFlushPending() const {
        return textureCacheFlushPending;
    }

    void setTextureCacheFlushPending(bool isPending) {
        textureCacheFlushPending = isPending;
    }

    bool consumeTextureCacheFlushPending() {
        bool wasPending = textureCacheFlushPending;
        textureCacheFlushPending = false;
        return wasPending;
    }

    NEO::EngineGroupType getEngineGroupType() const {
        return engineGroupType;
    }

    bool getLocalDispatchSupport() const {
        return localDispatchSupport;
    }

    bool isClosed() const {
        return closedCmdList;
    }
    ze_result_t obtainLaunchParamsFromExtensions(const ze_base_desc_t *desc, CmdListKernelLaunchParams &launchParams, ze_kernel_handle_t kernelHandle) const;

  protected:
    NEO::GraphicsAllocation *getAllocationFromHostPtrMap(const void *buffer, uint64_t bufferSize, bool copyOffload);
    NEO::GraphicsAllocation *getHostPtrAlloc(const void *buffer, uint64_t bufferSize, bool hostCopyAllowed, bool copyOffload);
    bool setupTimestampEventForMultiTile(Event *signalEvent);
    bool isTimestampEventForMultiTile(Event *signalEvent);
    bool getDcFlushRequired(bool externalCondition) const {
        return externalCondition ? dcFlushSupport : false;
    }
    MOCKABLE_VIRTUAL void synchronizeEventList(uint32_t numWaitEvents, ze_event_handle_t *waitEventList);

    bool isDualStreamCopyOffloadOperation(bool offloadOperation) const { return (getCopyOffloadModeForOperation(offloadOperation) == CopyOffloadModes::dualStream); }
    bool isNonDualStreamCopyOffloadOperation(bool offloadOperation) const { return offloadOperation && !isDualStreamCopyOffloadOperation(offloadOperation); }

    std::map<const void *, NEO::GraphicsAllocation *> hostPtrMap;
    NEO::PrivateAllocsToReuseContainer ownedPrivateAllocations;
    std::vector<NEO::GraphicsAllocation *> patternAllocations;
    std::vector<NEO::TagNodeBase *> patternTags;
    std::vector<std::weak_ptr<Kernel>> printfKernelContainer;

    NEO::CommandContainer commandContainer;

    CmdListReturnPoints returnPoints;
    NEO::StreamProperties requiredStreamState{};
    NEO::StreamProperties finalStreamState{};
    CommandsToPatch commandsToPatch{};
    UnifiedMemoryControls unifiedMemoryControls;
    NEO::PrefetchContext prefetchContext;
    AppendedMemAdviseOperations memAdviseOperations;
    NEO::L1CachePolicy l1CachePolicyData{};
    NEO::EncodeDummyBlitWaArgs dummyBlitWa{};

    int64_t currentSurfaceStateBaseAddress = NEO::StreamProperty64::initValue;
    int64_t currentDynamicStateBaseAddress = NEO::StreamProperty64::initValue;
    int64_t currentIndirectObjectBaseAddress = NEO::StreamProperty64::initValue;
    int64_t currentBindingTablePoolBaseAddress = NEO::StreamProperty64::initValue;

    ze_context_handle_t hContext = nullptr;
    CommandQueue *cmdQImmediate = nullptr;
    CommandQueue *cmdQImmediateCopyOffload = nullptr;
    Device *device = nullptr;
    NEO::ScratchSpaceController *usedScratchController = nullptr;

    size_t minimalSizeForBcsSplit = 4 * MemoryConstants::megaByte;
    size_t cmdListCurrentStartOffset = 0;
    size_t maxFillPaternSizeForCopyEngine = 0;

    uint32_t commandListPerThreadScratchSize[2]{};

    ze_command_list_flags_t flags = 0u;
    NEO::PreemptionMode commandListPreemptionMode = NEO::PreemptionMode::Initial;
    NEO::EngineGroupType engineGroupType = NEO::EngineGroupType::maxEngineGroups;
    NEO::HeapAddressModel cmdListHeapAddressModel = NEO::HeapAddressModel::privateHeaps;
    std::optional<uint32_t> ordinal = std::nullopt;

    CommandListType cmdListType = CommandListType::typeRegular;
    CopyOffloadMode copyOffloadMode = CopyOffloadModes::disabled;
    uint32_t partitionCount = 1;
    uint32_t defaultMocsIndex = 0;
    int32_t defaultPipelinedThreadArbitrationPolicy = NEO::ThreadArbitrationPolicy::NotPresent;
    uint32_t maxLocalSubRegionSize = 0;

    bool isSyncModeQueue = false;
    bool isTbxMode = false;
    bool commandListSLMEnabled = false;
    bool requiresQueueUncachedMocs = false;
    bool isBcsSplitNeeded = false;
    bool immediateCmdListHeapSharing = false;
    bool indirectAllocationsAllowed = false;
    bool internalUsage = false;
    bool containsCooperativeKernelsFlag = false;
    bool containsStatelessUncachedResource = false;
    bool performMemoryPrefetch = false;
    bool frontEndStateTracking = false;
    bool dcFlushSupport = false;
    bool systolicModeSupport = false;
    bool pipelineSelectStateTracking = false;
    bool stateComputeModeTracking = false;
    bool signalAllEventPackets = false;
    bool stateBaseAddressTracking = false;
    bool doubleSbaWa = false;
    bool containsAnyKernel = false;
    bool pipeControlMultiKernelEventSync = false;
    bool compactL3FlushEventPacket = false;
    bool dynamicHeapRequired = false;
    bool kernelWithAssertAppended = false;
    bool dispatchCmdListBatchBufferAsPrimary = false;
    bool copyThroughLockedPtrEnabled = false;
    bool useOnlyGlobalTimestamps = false;
    bool heaplessModeEnabled = false;
    bool heaplessStateInitEnabled = false;
    bool scratchAddressPatchingEnabled = false;
    bool taskCountUpdateFenceRequired = false;
    bool statelessBuiltinsEnabled = false;
    bool localDispatchSupport = false;
    bool l3FlushAfterPostSyncRequired = false;
    bool textureCacheFlushPending = false;
    bool closedCmdList = false;
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
