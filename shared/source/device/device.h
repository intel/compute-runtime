/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/device/device_info.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/options.h"
#include "shared/source/memory_manager/unified_memory_reuse.h"
#include "shared/source/os_interface/performance_counters.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/isa_pool_allocator.h"
#include "shared/source/utilities/reference_tracked_object.h"
#include "shared/source/utilities/timestamp_pool_allocator.h"

#include <array>
#include <mutex>

namespace NEO {
class AILConfiguration;
class BindlessHeapsHelper;
class BuiltIns;
class CompilerInterface;
class CompilerProductHelper;
class Debugger;
class DebuggerL0;
class ExecutionEnvironment;
class GfxCoreHelper;
class GmmClientContext;
class GmmHelper;
class OSTime;
class ProductHelper;
class ReleaseHelper;
class SipExternalLib;
class SubDevice;
class SyncBufferHandler;
class UsmMemAllocPoolsManager;
class UsmMemAllocPool;
enum class EngineGroupType : uint32_t;
struct PhysicalDevicePciBusInfo;

struct SelectorCopyEngine : NonCopyableAndNonMovableClass {
    std::atomic<bool> isMainUsed = false;
    std::atomic<uint32_t> selector = 0;
};

using EnginesT = std::vector<EngineControl>;
struct EngineGroupT {
    EngineGroupType engineGroupType;
    EnginesT engines;
};
using EngineGroupsT = std::vector<EngineGroupT>;
using CsrContainer = std::vector<std::unique_ptr<CommandStreamReceiver>>;

struct SecondaryContexts : NEO::NonCopyableAndNonMovableClass {
    SecondaryContexts() = default;
    SecondaryContexts(SecondaryContexts &&in) noexcept {
        this->engines = std::move(in.engines);
        this->regularCounter = in.regularCounter.load();
        this->highPriorityCounter = in.highPriorityCounter.load();
        this->regularEnginesTotal = in.regularEnginesTotal;
        this->highPriorityEnginesTotal = in.highPriorityEnginesTotal;
    }
    SecondaryContexts &operator=(SecondaryContexts &&other) noexcept = delete;

    EngineControl *getEngine(const EngineUsage usage, int priority);

    EnginesT engines;                                 // vector of secondary EngineControls
    std::atomic<uint8_t> regularCounter = 0;          // Counter used to assign next regular EngineControl
    std::atomic<uint8_t> highPriorityCounter = 0;     // Counter used to assign next highPriority EngineControl
    std::atomic<uint8_t> assignedContextsCounter = 0; // Counter of assigned contexts in group
    uint32_t regularEnginesTotal;
    uint32_t highPriorityEnginesTotal;

    std::vector<int32_t> npIndices;
    std::vector<int32_t> hpIndices;
    std::mutex mutex;
};

static_assert(NEO::NonCopyable<SecondaryContexts>);

struct RTDispatchGlobalsInfo {
    GraphicsAllocation *rtDispatchGlobalsArray = nullptr;
    std::vector<GraphicsAllocation *> rtStacks; // per tile
};

class Device : public ReferenceTrackedObject<Device>, NEO::NonCopyableAndNonMovableClass {
  public:
    ~Device() override;

    template <typename DeviceT, typename... ArgsT>
    static DeviceT *create(ArgsT &&...args) {
        DeviceT *device = new DeviceT(std::forward<ArgsT>(args)...);
        return createDeviceInternals(device);
    }

    virtual void incRefInternal() {
        ReferenceTrackedObject<Device>::incRefInternal();
    }
    virtual unique_ptr_if_unused<Device> decRefInternal() {
        return ReferenceTrackedObject<Device>::decRefInternal();
    }

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const;
    bool getHostTimer(uint64_t *hostTimestamp) const;
    const HardwareInfo &getHardwareInfo() const;
    const DeviceInfo &getDeviceInfo() const;
    EngineControl *tryGetEngine(aub_stream::EngineType engineType, EngineUsage engineUsage);
    EngineControl &getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage);
    EngineGroupsT &getRegularEngineGroups() {
        return this->regularEngineGroups;
    }
    const EngineGroupT *tryGetRegularEngineGroup(EngineGroupType engineGroupType) const;
    size_t getEngineGroupIndexFromEngineGroupType(EngineGroupType engineGroupType) const;
    EngineControl &getEngine(uint32_t index);
    EngineControl &getDefaultEngine();
    EngineControl &getInternalEngine();
    EngineControl *getInternalCopyEngine();
    EngineControl *getHpCopyEngine();
    SelectorCopyEngine &getSelectorCopyEngine();
    MemoryManager *getMemoryManager() const;
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    OSTime *getOSTime() const;
    double getProfilingTimerResolution();
    uint64_t getProfilingTimerClock();
    double getPlatformHostTimerResolution() const;
    GFXCORE_FAMILY getRenderCoreFamily() const;
    PerformanceCounters *getPerformanceCounters() { return performanceCounters.get(); }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    void overridePreemptionMode(PreemptionMode mode) { preemptionMode = mode; }
    Debugger *getDebugger() const;
    DebuggerL0 *getL0Debugger();
    const EnginesT &getAllEngines() const;
    const std::string getDeviceName() const;

    ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    const RootDeviceEnvironment &getRootDeviceEnvironment() const;
    RootDeviceEnvironment &getRootDeviceEnvironmentRef() const;
    bool isFullRangeSvm() const;
    static bool isBlitSplitEnabled();
    static bool isInitDeviceWithFirstSubmissionEnabled(CommandStreamReceiverType csrType);
    static std::vector<DeviceVector> groupDevices(DeviceVector devices);
    bool isBcsSplitSupported();
    bool isInitDeviceWithFirstSubmissionSupported(CommandStreamReceiverType csrType);
    bool areSharedSystemAllocationsAllowed() const;
    template <typename SpecializedDeviceT>
    void setSpecializedDevice(SpecializedDeviceT *specializedDevice) {
        this->specializedDevice = reinterpret_cast<uintptr_t>(specializedDevice);
    }
    template <typename SpecializedDeviceT>
    SpecializedDeviceT *getSpecializedDevice() const {
        return reinterpret_cast<SpecializedDeviceT *>(specializedDevice);
    }
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface() const;
    MOCKABLE_VIRTUAL SipExternalLib *getSipExternalLibInterface() const;
    BuiltIns *getBuiltIns() const;
    void allocateSyncBufferHandler();

    uint32_t getRootDeviceIndex() const {
        return this->rootDeviceIndex;
    }
    uint32_t getNumGenericSubDevices() const;
    Device *getSubDevice(uint32_t deviceId) const;
    Device *getNearestGenericSubDevice(uint32_t deviceId);
    virtual Device *getRootDevice() const = 0;
    DeviceBitfield getDeviceBitfield() const { return deviceBitfield; };
    uint32_t getNumSubDevices() const { return numSubDevices; }
    virtual bool isSubDevice() const = 0;
    bool hasRootCsr() const { return rootCsrCreated; }

    BindlessHeapsHelper *getBindlessHeapsHelper() const;

    static decltype(&PerformanceCounters::create) createPerformanceCountersFunc;
    std::unique_ptr<SyncBufferHandler> syncBufferHandler;
    GraphicsAllocation *getRTMemoryBackedBuffer() { return rtMemoryBackedBuffer; }
    RTDispatchGlobalsInfo *getRTDispatchGlobals(uint32_t maxBvhLevels);
    bool rayTracingIsInitialized() const { return rtMemoryBackedBuffer != nullptr && rtDispatchGlobalsInfos.size() != 0; }
    void initializeRTMemoryBackedBuffer();
    void initializeRayTracing(uint32_t maxBvhLevels);
    void allocateRTDispatchGlobals(uint32_t maxBvhLevels);

    MOCKABLE_VIRTUAL uint64_t getGlobalMemorySize(uint32_t deviceBitfield) const;
    const std::vector<SubDevice *> &getSubDevices() const { return subdevices; }
    bool getUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid);
    void generateUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid);
    void getAdapterLuid(std::array<uint8_t, ProductHelper::luidSize> &luid);
    MOCKABLE_VIRTUAL bool verifyAdapterLuid();
    void getAdapterMask(uint32_t &nodeMask);
    const GfxCoreHelper &getGfxCoreHelper() const;
    const ProductHelper &getProductHelper() const;
    const CompilerProductHelper &getCompilerProductHelper() const;
    MOCKABLE_VIRTUAL ReleaseHelper *getReleaseHelper() const;
    MOCKABLE_VIRTUAL AILConfiguration *getAilConfigurationHelper() const;
    ISAPoolAllocator &getIsaPoolAllocator() {
        return isaPoolAllocator;
    }
    TimestampPoolAllocator &getDeviceTimestampPoolAllocator() {
        return deviceTimestampPoolAllocator;
    }
    UsmMemAllocPoolsManager *getUsmMemAllocPoolsManager() {
        return deviceUsmMemAllocPoolsManager.get();
    }
    UsmMemAllocPool *getUsmMemAllocPool() {
        return usmMemAllocPool.get();
    }
    MOCKABLE_VIRTUAL void stopDirectSubmissionAndWaitForCompletion();
    bool isAnyDirectSubmissionEnabled() const;
    bool isAnyDirectSubmissionLightEnabled() const;
    bool isStateSipRequired() const {
        return (getPreemptionMode() == PreemptionMode::MidThread || getDebugger() != nullptr) && getCompilerInterface();
    }

    MOCKABLE_VIRTUAL EngineControl *getSecondaryEngineCsr(EngineTypeUsage engineTypeUsage, int priority, bool allocateInterrupt);
    bool isSecondaryContextEngineType(aub_stream::EngineType type) {
        return EngineHelpers::isCcs(type) || EngineHelpers::isBcs(type);
    }

    GraphicsAllocation *getDebugSurface() const { return debugSurface; }
    void setDebugSurface(GraphicsAllocation *debugSurface) { this->debugSurface = debugSurface; };
    const CsrContainer &getSecondaryCsrs() const { return secondaryCsrs; }

    std::atomic<uint32_t> debugExecutionCounter = 0;

    void stopDirectSubmissionForCopyEngine();

    bool shouldLimitAllocationsReuse() const;

    uint32_t getMicrosecondResolution() const {
        return microsecondResolution;
    }

    void updateMaxPoolCount(uint32_t maxPoolCount) {
        maxBufferPoolCount = maxPoolCount;
    }

    bool requestPoolCreate(uint32_t count) {
        if (maxBufferPoolCount >= count + bufferPoolCount.fetch_add(count)) {
            return true;
        } else {
            bufferPoolCount -= count;
            return false;
        }
    }

    void recordPoolsFreed(uint32_t size) {
        bufferPoolCount -= size;
    }

    UsmReuseInfo usmReuseInfo;

    void resetUsmAllocationPool(UsmMemAllocPool *usmMemAllocPool);
    void cleanupUsmAllocationPool();

  protected:
    Device() = delete;
    Device(ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex);

    MOCKABLE_VIRTUAL void initializeCaps();

    template <typename T>
    static T *createDeviceInternals(T *device) {
        if (false == device->createDeviceImpl()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    MOCKABLE_VIRTUAL bool createDeviceImpl();
    bool initDeviceWithEngines();
    bool initializeCommonResources();
    bool initDeviceFully();
    void initUsmReuseLimits();
    virtual bool createEngines();

    void addEngineToEngineGroup(EngineControl &engine);
    MOCKABLE_VIRTUAL bool createEngine(EngineTypeUsage engineTypeUsage);
    MOCKABLE_VIRTUAL bool initializeEngines();
    MOCKABLE_VIRTUAL bool createSecondaryEngine(CommandStreamReceiver *primaryCsr, EngineTypeUsage engineTypeUsage);

    MOCKABLE_VIRTUAL std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const;
    MOCKABLE_VIRTUAL SubDevice *createSubDevice(uint32_t subDeviceIndex);
    MOCKABLE_VIRTUAL size_t getMaxParameterSizeFromIGC() const;
    MOCKABLE_VIRTUAL bool isAnyDirectSubmissionEnabledImpl(bool light) const;
    double getPercentOfGlobalMemoryAvailable() const;
    virtual void createBindlessHeapsHelper() {}
    bool createSubDevices();
    bool createGenericSubDevices();
    bool genericSubDevicesAllowed();
    void finalizeRayTracing();
    void createSecondaryContexts(const EngineControl &primaryEngine, SecondaryContexts &secondaryEnginesForType, uint32_t contextCount, uint32_t regularPriorityCount, uint32_t highPriorityContextCount);
    void allocateDebugSurface(size_t debugSurfaceSize);

    DeviceInfo deviceInfo = {};

    std::unique_ptr<PerformanceCounters> performanceCounters;
    CsrContainer commandStreamReceivers;
    EnginesT allEngines;

    std::unordered_map<aub_stream::EngineType, SecondaryContexts> secondaryEngines;
    CsrContainer secondaryCsrs;

    EngineGroupsT regularEngineGroups;
    std::vector<SubDevice *> subdevices;

    PreemptionMode preemptionMode = PreemptionMode::Disabled;
    ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t defaultEngineIndex = 0;
    uint32_t numSubDevices = 0;
    std::atomic_uint32_t regularCommandQueuesCreatedWithinDeviceCount{0};
    std::bitset<8> availableEnginesForCommandQueueusRoundRobin = 0;
    uint32_t queuesPerEngineCount = 1;
    bool hasGenericSubDevices = false;
    bool rootCsrCreated = false;
    const uint32_t rootDeviceIndex;
    GraphicsAllocation *debugSurface = nullptr;

    SelectorCopyEngine selectorCopyEngine = {};
    EngineControl *hpCopyEngine = nullptr;

    DeviceBitfield deviceBitfield = 1;

    uintptr_t specializedDevice = reinterpret_cast<uintptr_t>(nullptr);

    GraphicsAllocation *rtMemoryBackedBuffer = nullptr;
    std::vector<RTDispatchGlobalsInfo *> rtDispatchGlobalsInfos;

    ISAPoolAllocator isaPoolAllocator;
    TimestampPoolAllocator deviceTimestampPoolAllocator;
    std::unique_ptr<UsmMemAllocPoolsManager> deviceUsmMemAllocPoolsManager;
    std::unique_ptr<UsmMemAllocPool> usmMemAllocPool;

    std::atomic_uint32_t bufferPoolCount = 0u;
    uint32_t maxBufferPoolCount = 0u;
    uint32_t microsecondResolution = 1000u;

    struct {
        bool isValid = false;
        std::array<uint8_t, ProductHelper::uuidSize> id;
    } uuid;
    bool generateUuidFromPciBusInfo(const PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, ProductHelper::uuidSize> &uuid);
};

inline EngineControl &Device::getDefaultEngine() {
    return allEngines[defaultEngineIndex];
}

inline SelectorCopyEngine &Device::getSelectorCopyEngine() {
    return selectorCopyEngine;
}

static_assert(NEO::NonCopyableAndNonMovable<Device>);

} // namespace NEO
