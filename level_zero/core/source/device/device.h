/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

static_assert(NEO::ProductHelper::uuidSize == ZE_MAX_DEVICE_UUID_SIZE);

struct _ze_device_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_DEVICE> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_device_handle_t>);

namespace aub_stream {
enum EngineType : uint32_t;
} // namespace aub_stream

namespace NEO {
class AllocationsList;
class CommandStreamReceiver;
class CompilerProductHelper;
class DebuggerL0;
class Device;
class DriverInfo;
class GfxCoreHelper;
class GraphicsAllocation;
class MemoryManager;
class OSInterface;
class HostFunctionAllocator;
class TagAllocatorBase;
enum class AllocationType;
enum class EngineUsage : uint32_t;
struct DeviceInfo;
struct HardwareInfo;
} // namespace NEO

namespace L0 {
class BcsSplit;
class CacheReservation;
class L0GfxCoreHelper;
class MetricDeviceContext;
struct BuiltinFunctionsLib;
struct CommandList;
struct DebugSession;
class DriverHandle;
struct ExecutionEnvironment;
struct FabricVertex;
struct Image;
struct SysmanDevice;
enum class ModuleType;

struct Device : _ze_device_handle_t, NEO::NonCopyableAndNonMovableClass {
    Device();
    virtual ~Device();

    uint32_t getRootDeviceIndex() const;

    NEO::Device *getNEODevice() const {
        return this->neoDevice;
    }

    MOCKABLE_VIRTUAL ze_result_t canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value);
    MOCKABLE_VIRTUAL ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                                   ze_command_list_handle_t *commandList);
    MOCKABLE_VIRTUAL ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                                           ze_command_list_handle_t *commandList);
    MOCKABLE_VIRTUAL ze_result_t createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                            ze_command_list_handle_t *commandList);
    MOCKABLE_VIRTUAL ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                                    ze_command_queue_handle_t *commandQueue);
    MOCKABLE_VIRTUAL ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                                            ze_command_queue_handle_t *commandQueue);
    MOCKABLE_VIRTUAL ze_result_t createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage);
    MOCKABLE_VIRTUAL ze_result_t createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                              ze_module_build_log_handle_t *buildLog, ModuleType type);
    MOCKABLE_VIRTUAL ze_result_t createSampler(const ze_sampler_desc_t *pDesc,
                                               ze_sampler_handle_t *phSampler);
    MOCKABLE_VIRTUAL ze_result_t getComputeProperties(ze_device_compute_properties_t *pComputeProperties);
    MOCKABLE_VIRTUAL ze_result_t getP2PProperties(ze_device_handle_t hPeerDevice,
                                                  ze_device_p2p_properties_t *pP2PProperties);
    MOCKABLE_VIRTUAL ze_result_t getKernelProperties(ze_device_module_properties_t *pKernelProperties);
    MOCKABLE_VIRTUAL ze_result_t getPciProperties(ze_pci_ext_properties_t *pPciProperties);
    MOCKABLE_VIRTUAL ze_result_t getRootDevice(ze_device_handle_t *phRootDevice);
    MOCKABLE_VIRTUAL ze_result_t getMemoryProperties(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties);
    MOCKABLE_VIRTUAL ze_result_t getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties);
    MOCKABLE_VIRTUAL ze_result_t getProperties(ze_device_properties_t *pDeviceProperties);
    MOCKABLE_VIRTUAL ze_result_t getVectorWidthPropertiesExt(uint32_t *pCount, ze_device_vector_width_properties_ext_t *pVectorWidthProperties);
    MOCKABLE_VIRTUAL ze_result_t getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices);
    MOCKABLE_VIRTUAL ze_result_t getCacheProperties(uint32_t *pCount, ze_device_cache_properties_t *pCacheProperties);
    MOCKABLE_VIRTUAL ze_result_t getStatus();
    MOCKABLE_VIRTUAL ze_result_t reserveCache(size_t cacheLevel, size_t cacheReservationSize);
    MOCKABLE_VIRTUAL ze_result_t setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion);
    MOCKABLE_VIRTUAL ze_result_t imageGetProperties(const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties);
    MOCKABLE_VIRTUAL ze_result_t getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties);
    MOCKABLE_VIRTUAL ze_result_t getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties);
    MOCKABLE_VIRTUAL ze_result_t getGlobalTimestamps(uint64_t *hostTimestamp, uint64_t *deviceTimestamp);
    uint32_t getCopyQueueGroupsFromSubDevice(uint32_t numberOfSubDeviceCopyEngineGroupsRequested,
                                             ze_command_queue_group_properties_t *pCommandQueueGroupProperties);
    MOCKABLE_VIRTUAL ze_result_t getCommandQueueGroupProperties(uint32_t *pCount,
                                                                ze_command_queue_group_properties_t *pCommandQueueGroupProperties);
    MOCKABLE_VIRTUAL ze_result_t getDebugProperties(zet_device_debug_properties_t *pDebugProperties);

    MOCKABLE_VIRTUAL ze_result_t systemBarrier();
    MOCKABLE_VIRTUAL ze_result_t synchronize();

    MOCKABLE_VIRTUAL BuiltinFunctionsLib *getBuiltinFunctionsLib();
    MOCKABLE_VIRTUAL uint32_t getMOCS(bool l3enabled, bool l1enabled);
    MOCKABLE_VIRTUAL uint32_t getMaxNumHwThreads() const;

    MOCKABLE_VIRTUAL const NEO::GfxCoreHelper &getGfxCoreHelper();
    MOCKABLE_VIRTUAL const L0GfxCoreHelper &getL0GfxCoreHelper();
    MOCKABLE_VIRTUAL const NEO::ProductHelper &getProductHelper();
    MOCKABLE_VIRTUAL const NEO::CompilerProductHelper &getCompilerProductHelper();

    bool isImplicitScalingCapable() const {
        return implicitScalingCapable;
    }
    MOCKABLE_VIRTUAL const NEO::HardwareInfo &getHwInfo() const;
    MOCKABLE_VIRTUAL NEO::OSInterface *getOsInterface();
    MOCKABLE_VIRTUAL uint32_t getPlatformInfo() const;
    MOCKABLE_VIRTUAL MetricDeviceContext &getMetricDeviceContext();
    MOCKABLE_VIRTUAL DebugSession *getDebugSession(const zet_debug_config_t &config);
    void setDebugSession(DebugSession *session);
    MOCKABLE_VIRTUAL DebugSession *createDebugSession(const zet_debug_config_t &config, ze_result_t &result, bool isRootAttach);
    MOCKABLE_VIRTUAL void removeDebugSession();

    MOCKABLE_VIRTUAL ze_result_t activateMetricGroupsDeferred(uint32_t count,
                                                              zet_metric_group_handle_t *phMetricGroups);
    MOCKABLE_VIRTUAL void activateMetricGroups();

    MOCKABLE_VIRTUAL DriverHandle *getDriverHandle() {
        return this->driverHandle;
    }
    MOCKABLE_VIRTUAL void setDriverHandle(DriverHandle *driverHandle);

    static Device *fromHandle(ze_device_handle_t handle) { return static_cast<Device *>(handle); }

    inline ze_device_handle_t toHandle() { return this; }

    static Device *create(DriverHandle *driverHandle, NEO::Device *neoDevice, bool isSubDevice, ze_result_t *returnValue);
    static Device *create(DriverHandle *driverHandle, NEO::Device *neoDevice, bool isSubDevice, ze_result_t *returnValue, L0::Device *deviceL0);
    static Device *deviceReinit(DriverHandle *driverHandle, L0::Device *device, std::unique_ptr<NEO::Device> &neoDevice, ze_result_t *returnValue);

    MOCKABLE_VIRTUAL NEO::PreemptionMode getDevicePreemptionMode() const;
    MOCKABLE_VIRTUAL const NEO::DeviceInfo &getDeviceInfo() const;
    NEO::DebuggerL0 *getL0Debugger();

    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *getDebugSurface() const;
    void setDebugSurface(NEO::GraphicsAllocation *debugSurface);

    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *allocateManagedMemoryFromHostPtr(void *buffer,
                                                                               size_t size, struct CommandList *commandList);
    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *allocateMemoryFromHostPtr(const void *buffer, size_t size, bool hostCopyAllowed);
    MOCKABLE_VIRTUAL void setSysmanHandle(SysmanDevice *pSysmanDevice);
    MOCKABLE_VIRTUAL SysmanDevice *getSysmanHandle();
    MOCKABLE_VIRTUAL ze_result_t getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index, ze_command_queue_priority_t priority, std::optional<int> priorityLevel, bool allocateInterrupt);
    MOCKABLE_VIRTUAL ze_result_t getCsrForLowPriority(NEO::CommandStreamReceiver **csr, bool copyOnly);
    ze_result_t getCsrForHighPriority(NEO::CommandStreamReceiver **csr, bool copyOnly);
    bool isSuitableForLowPriority(ze_command_queue_priority_t priority, bool copyOnly);
    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *obtainReusableAllocation(size_t requiredSize, NEO::AllocationType type);
    MOCKABLE_VIRTUAL void storeReusableAllocation(NEO::GraphicsAllocation &alloc);
    NEO::Device *getActiveDevice() const;
    MOCKABLE_VIRTUAL ze_result_t getFabricVertex(ze_fabric_vertex_handle_t *phVertex);
    MOCKABLE_VIRTUAL uint32_t getEventMaxPacketCount() const;
    MOCKABLE_VIRTUAL uint32_t getEventMaxKernelCount() const;
    MOCKABLE_VIRTUAL void bcsSplitReleaseResources();
    NEO::TagAllocatorBase *getDeviceInOrderCounterAllocator();
    NEO::TagAllocatorBase *getHostInOrderCounterAllocator();
    NEO::TagAllocatorBase *getInOrderTimestampAllocator();
    NEO::TagAllocatorBase *getFillPatternAllocator();
    NEO::GraphicsAllocation *getSyncDispatchTokenAllocation() const { return syncDispatchTokenAllocation; }
    uint32_t getNextSyncDispatchQueueId();
    void ensureSyncDispatchTokenAllocation();
    void setIdentifier(uint32_t id) { identifier = id; }
    uint32_t getIdentifier() const { return identifier; }
    ze_result_t getPriorityLevels(int32_t *lowestPriority,
                                  int32_t *highestPriority);
    MOCKABLE_VIRTUAL uint32_t getAggregatedCopyOffloadIncrementValue();

    bool toPhysicalSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t &deviceIndex);
    bool toApiSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t deviceIndex);
    uint32_t getPhysicalSubDeviceId();

    void getExtendedDeviceModuleProperties(ze_base_desc_t *pExtendedProperties);
    uint32_t getAdditionalEngines(uint32_t numAdditionalEnginesRequested,
                                  ze_command_queue_group_properties_t *pCommandQueueGroupProperties);

    void createSysmanHandle(bool isSubDevice);
    void populateSubDeviceCopyEngineGroups();
    const NEO::EngineGroupsT &getSubDeviceCopyEngineGroups() const { return subDeviceCopyEngineGroups; }
    bool isQueueGroupOrdinalValid(uint32_t ordinal);
    void setFabricVertex(FabricVertex *inFabricVertex) { fabricVertex = inFabricVertex; }
    NEO::HostFunctionAllocator *getHostFunctionAllocator(NEO::CommandStreamReceiver *csr);

    using CmdListCreateFunPtrT = L0::CommandList *(*)(uint32_t, Device *, NEO::EngineGroupType, ze_command_list_flags_t, ze_result_t &, bool);
    CmdListCreateFunPtrT getCmdListCreateFunc(const ze_base_desc_t *desc);
    void getAdditionalExtProperties(ze_base_properties_t *extendedProperties);

    ze_result_t queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties);
    uint32_t queryDeviceNodeMask();
    NEO::EngineGroupType getInternalEngineGroupType();
    uint32_t getCopyEngineOrdinal() const;
    std::optional<uint32_t> tryGetCopyEngineOrdinal() const;

    static bool queryPeerAccess(NEO::Device &device, NEO::Device &peerDevice, void **handlePtr, uint64_t *handle);
    static void freeMemoryAllocation(NEO::Device &device, void *memoryAllocation);

    void releaseResources();

    ze_command_list_handle_t globalTimestampCommandList = nullptr;
    void *globalTimestampAllocation = nullptr;

    bool resourcesReleased = false;
    bool calculationForDisablingEuFusionWithDpasNeeded = false;

    NEO::SVMAllocsManager::MapBasedAllocationTracker peerAllocations;
    NEO::SVMAllocsManager::MapBasedAllocationTracker peerCounterAllocations;
    std::unordered_map<const void *, L0::Image *> peerImageAllocations;
    std::mutex globalTimestampMutex;
    std::mutex printfKernelMutex;

    NEO::SpinLock peerImageAllocationsMutex;
    std::map<NEO::SvmAllocationData *, NEO::MemAdviseFlags> memAdviseSharedAllocations;
    std::map<NEO::SvmAllocationData *, ze_memory_atomic_attr_exp_flags_t> atomicAccessAllocations;
    std::vector<Device *> subDevices;

    std::unique_ptr<NEO::AllocationsList> allocationsForReuse;
    std::unique_ptr<NEO::DriverInfo> driverInfo;
    std::unique_ptr<BuiltinFunctionsLib> builtins;
    std::unique_ptr<MetricDeviceContext> metricContext;
    std::unique_ptr<CacheReservation> cacheReservation;
    std::unique_ptr<BcsSplit> bcsSplit;

    DriverHandle *driverHandle = nullptr;
    FabricVertex *fabricVertex = nullptr;
    CommandList *pageFaultCommandList = nullptr;
    Device *rootDevice = nullptr;
    ze_pci_speed_ext_t pciMaxSpeed = {-1, -1, -1};
    uint32_t maxNumHwThreads = 0;
    uint32_t numSubDevices = 0;
    bool isSubdevice = false;

  protected:
    ze_result_t getGlobalTimestampsUsingSubmission(uint64_t *hostTimestamp, uint64_t *deviceTimestamp);
    ze_result_t getGlobalTimestampsUsingOsInterface(uint64_t *hostTimestamp, uint64_t *deviceTimestamp);
    const char *getDeviceMemoryName();
    void adjustCommandQueueDesc(uint32_t &ordinal, uint32_t &index);
    NEO::EngineGroupType getEngineGroupTypeForOrdinal(uint32_t ordinal) const;
    void getP2PPropertiesDirectFabricConnection(Device *peerDevice,
                                                ze_device_p2p_bandwidth_exp_properties_t *bandwidthPropertiesDesc);
    bool tryAssignSecondaryContext(aub_stream::EngineType engineType, NEO::EngineUsage engineUsage, std::optional<uint32_t> priorityLevel, NEO::CommandStreamReceiver **csr, bool allocateInterrupt);
    void getIntelXeDeviceProperties(ze_base_properties_t *extendedProperties) const;
    NEO::EngineGroupsT subDeviceCopyEngineGroups{};

    SysmanDevice *pSysmanDevice = nullptr;
    std::unique_ptr<DebugSession> debugSession;

    NEO::Device *neoDevice = nullptr;
    std::unique_ptr<NEO::TagAllocatorBase> deviceInOrderCounterAllocator;
    std::unique_ptr<NEO::TagAllocatorBase> hostInOrderCounterAllocator;
    std::unique_ptr<NEO::TagAllocatorBase> inOrderTimestampAllocator;
    std::unique_ptr<NEO::TagAllocatorBase> fillPatternAllocator;
    std::unique_ptr<NEO::HostFunctionAllocator> hostFunctionAllocator;

    NEO::GraphicsAllocation *syncDispatchTokenAllocation = nullptr;
    std::mutex inOrderAllocatorMutex;
    std::mutex syncDispatchTokenMutex;
    std::mutex hostFunctionAllocatorMutex;

    std::atomic<uint32_t> syncDispatchQueueIdAllocator = 0;
    uint32_t aggregatedCopyIncValue = 0;
    uint32_t identifier = 0;
    int32_t queuePriorityHigh = 0;
    int32_t queuePriorityLow = 1;
    bool implicitScalingCapable = false;
};

static_assert(NEO::NonCopyableAndNonMovable<Device>);

void transferAndUnprotectMemoryWithHints(NEO::CpuPageFaultManager *pageFaultHandler, void *allocPtr, NEO::CpuPageFaultManager::PageFaultData &pageFaultData);

} // namespace L0
