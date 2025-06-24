/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/device/device.h"

#include <map>
#include <mutex>

namespace NEO {
class AllocationsList;
class DriverInfo;
} // namespace NEO

namespace L0 {
struct SysmanDevice;
struct FabricVertex;
class CacheReservation;

struct DeviceImp : public Device, NEO::NonCopyableAndNonMovableClass {
    DeviceImp();
    ze_result_t getStatus() override;
    MOCKABLE_VIRTUAL ze_result_t queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth);
    ze_result_t canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) override;
    ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                  ze_command_list_handle_t *commandList) override;
    MOCKABLE_VIRTUAL ze_result_t createInternalCommandList(const ze_command_list_desc_t *desc,
                                                           ze_command_list_handle_t *commandList);
    ze_result_t createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                           ze_command_list_handle_t *phCommandList) override;
    ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                   ze_command_queue_handle_t *commandQueue) override;
    MOCKABLE_VIRTUAL ze_result_t createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                                            ze_command_queue_handle_t *commandQueue);
    ze_result_t createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage) override;
    ze_result_t createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                             ze_module_build_log_handle_t *buildLog, ModuleType type) override;
    ze_result_t createSampler(const ze_sampler_desc_t *pDesc,
                              ze_sampler_handle_t *phSampler) override;
    ze_result_t getComputeProperties(ze_device_compute_properties_t *pComputeProperties) override;
    ze_result_t getP2PProperties(ze_device_handle_t hPeerDevice,
                                 ze_device_p2p_properties_t *pP2PProperties) override;
    ze_result_t getKernelProperties(ze_device_module_properties_t *pKernelProperties) override;
    ze_result_t getPciProperties(ze_pci_ext_properties_t *pPciProperties) override;
    ze_result_t getRootDevice(ze_device_handle_t *phRootDevice) override;
    ze_result_t getMemoryProperties(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) override;
    ze_result_t getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) override;
    ze_result_t getProperties(ze_device_properties_t *pDeviceProperties) override;
    ze_result_t getVectorWidthPropertiesExt(uint32_t *pCount, ze_device_vector_width_properties_ext_t *pVectorWidthProperties) override;
    ze_result_t getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) override;
    ze_result_t getCacheProperties(uint32_t *pCount, ze_device_cache_properties_t *pCacheProperties) override;
    ze_result_t reserveCache(size_t cacheLevel, size_t cacheReservationSize) override;
    ze_result_t setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) override;
    ze_result_t imageGetProperties(const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) override;
    ze_result_t getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) override;
    uint32_t getCopyQueueGroupsFromSubDevice(uint32_t numberOfSubDeviceCopyEngineGroupsRequested,
                                             ze_command_queue_group_properties_t *pCommandQueueGroupProperties);
    ze_result_t getCommandQueueGroupProperties(uint32_t *pCount,
                                               ze_command_queue_group_properties_t *pCommandQueueGroupProperties) override;
    ze_result_t getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) override;
    ze_result_t getGlobalTimestamps(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) override;
    ze_result_t getDebugProperties(zet_device_debug_properties_t *pDebugProperties) override;

    ze_result_t systemBarrier() override;
    ze_result_t synchronize() override;
    BuiltinFunctionsLib *getBuiltinFunctionsLib() override;
    uint32_t getMOCS(bool l3enabled, bool l1enabled) override;
    const NEO::GfxCoreHelper &getGfxCoreHelper() override;
    const L0GfxCoreHelper &getL0GfxCoreHelper() override;
    const NEO::ProductHelper &getProductHelper() override;
    const NEO::CompilerProductHelper &getCompilerProductHelper() override;
    const NEO::HardwareInfo &getHwInfo() const override;
    NEO::OSInterface *getOsInterface() override;
    uint32_t getPlatformInfo() const override;
    MetricDeviceContext &getMetricDeviceContext() override;
    DebugSession *getDebugSession(const zet_debug_config_t &config) override;
    void setDebugSession(DebugSession *session);
    DebugSession *createDebugSession(const zet_debug_config_t &config, ze_result_t &result, bool isRootAttach) override;
    void removeDebugSession() override;

    uint32_t getMaxNumHwThreads() const override;
    ze_result_t activateMetricGroupsDeferred(uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups) override;

    DriverHandle *getDriverHandle() override;
    void setDriverHandle(DriverHandle *driverHandle) override;
    NEO::PreemptionMode getDevicePreemptionMode() const override;
    const NEO::DeviceInfo &getDeviceInfo() const override;

    void activateMetricGroups() override;
    void getExtendedDeviceModuleProperties(ze_base_desc_t *pExtendedProperties);
    uint32_t getAdditionalEngines(uint32_t numAdditionalEnginesRequested,
                                  ze_command_queue_group_properties_t *pCommandQueueGroupProperties);
    NEO::GraphicsAllocation *getDebugSurface() const override {
        return this->getNEODevice()->getDebugSurface();
    }
    void setDebugSurface(NEO::GraphicsAllocation *debugSurface) {
        this->getNEODevice()->setDebugSurface(debugSurface);
    };
    ~DeviceImp() override;
    NEO::GraphicsAllocation *allocateManagedMemoryFromHostPtr(void *buffer, size_t size, struct CommandList *commandList) override;
    NEO::GraphicsAllocation *allocateMemoryFromHostPtr(const void *buffer, size_t size, bool hostCopyAllowed) override;
    void setSysmanHandle(SysmanDevice *pSysman) override;
    SysmanDevice *getSysmanHandle() override;
    ze_result_t getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index, ze_command_queue_priority_t priority, int priorityLevel, bool allocateInterrupt) override;
    ze_result_t getCsrForLowPriority(NEO::CommandStreamReceiver **csr, bool copyOnly) override;
    ze_result_t getCsrForHighPriority(NEO::CommandStreamReceiver **csr, bool copyOnly);
    bool isSuitableForLowPriority(ze_command_queue_priority_t priority, bool copyOnly);
    NEO::GraphicsAllocation *obtainReusableAllocation(size_t requiredSize, NEO::AllocationType type) override;
    void storeReusableAllocation(NEO::GraphicsAllocation &alloc) override;
    NEO::Device *getActiveDevice() const;

    bool toPhysicalSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t &deviceIndex);
    bool toApiSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t deviceIndex);
    uint32_t getPhysicalSubDeviceId();

    bool isSubdevice = false;
    std::unique_ptr<BuiltinFunctionsLib> builtins;
    std::unique_ptr<MetricDeviceContext> metricContext;
    std::unique_ptr<CacheReservation> cacheReservation;
    uint32_t maxNumHwThreads = 0;
    uint32_t numSubDevices = 0;
    std::vector<Device *> subDevices;
    std::unordered_map<uint32_t, bool> crossAccessEnabledDevices;
    DriverHandle *driverHandle = nullptr;
    FabricVertex *fabricVertex = nullptr;
    CommandList *pageFaultCommandList = nullptr;
    ze_pci_speed_ext_t pciMaxSpeed = {-1, -1, -1};
    Device *rootDevice = nullptr;

    std::mutex printfKernelMutex;

    BcsSplit bcsSplit;

    ze_command_list_handle_t globalTimestampCommandList = nullptr;
    void *globalTimestampAllocation = nullptr;
    std::mutex globalTimestampMutex;

    bool resourcesReleased = false;
    bool calculationForDisablingEuFusionWithDpasNeeded = false;
    void releaseResources();

    NEO::SVMAllocsManager::MapBasedAllocationTracker peerAllocations;
    NEO::SVMAllocsManager::MapBasedAllocationTracker peerCounterAllocations;
    std::unordered_map<const void *, L0::Image *> peerImageAllocations;
    NEO::SpinLock peerImageAllocationsMutex;
    std::map<NEO::SvmAllocationData *, NEO::MemAdviseFlags> memAdviseSharedAllocations;
    std::map<NEO::SvmAllocationData *, ze_memory_atomic_attr_exp_flags_t> atomicAccessAllocations;
    std::unique_ptr<NEO::AllocationsList> allocationsForReuse;
    std::unique_ptr<NEO::DriverInfo> driverInfo;
    void createSysmanHandle(bool isSubDevice);
    void populateSubDeviceCopyEngineGroups();
    bool isQueueGroupOrdinalValid(uint32_t ordinal);
    void setFabricVertex(FabricVertex *inFabricVertex) { fabricVertex = inFabricVertex; }

    using CmdListCreateFunPtrT = L0::CommandList *(*)(uint32_t, Device *, NEO::EngineGroupType, ze_command_list_flags_t, ze_result_t &, bool);
    CmdListCreateFunPtrT getCmdListCreateFunc(const ze_base_desc_t *desc);
    void getAdditionalExtProperties(ze_base_properties_t *extendedProperties);
    void getAdditionalMemoryExtProperties(ze_base_properties_t *extProperties, const NEO::HardwareInfo &hwInfo);
    ze_result_t getFabricVertex(ze_fabric_vertex_handle_t *phVertex) override;

    ze_result_t queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties);
    uint32_t getEventMaxPacketCount() const override;
    uint32_t getEventMaxKernelCount() const override;
    uint32_t queryDeviceNodeMask();
    NEO::EngineGroupType getInternalEngineGroupType();
    uint32_t getCopyEngineOrdinal() const;
    std::optional<uint32_t> tryGetCopyEngineOrdinal() const;

  protected:
    ze_result_t queryPeerAccess(DeviceImp *peerDevice);
    bool submitCopyForP2P(DeviceImp *hPeerDevice, ze_result_t &result);
    ze_result_t getGlobalTimestampsUsingSubmission(uint64_t *hostTimestamp, uint64_t *deviceTimestamp);
    ze_result_t getGlobalTimestampsUsingOsInterface(uint64_t *hostTimestamp, uint64_t *deviceTimestamp);
    const char *getDeviceMemoryName();
    void adjustCommandQueueDesc(uint32_t &ordinal, uint32_t &index);
    NEO::EngineGroupType getEngineGroupTypeForOrdinal(uint32_t ordinal) const;
    void getP2PPropertiesDirectFabricConnection(DeviceImp *peerDeviceImp,
                                                ze_device_p2p_bandwidth_exp_properties_t *bandwidthPropertiesDesc);
    bool tryAssignSecondaryContext(aub_stream::EngineType engineType, NEO::EngineUsage engineUsage, int priorityLevel, NEO::CommandStreamReceiver **csr, bool allocateInterrupt);
    NEO::EngineGroupsT subDeviceCopyEngineGroups{};

    SysmanDevice *pSysmanDevice = nullptr;
    std::unique_ptr<DebugSession> debugSession;
};

static_assert(NEO::NonCopyableAndNonMovable<DeviceImp>);

void transferAndUnprotectMemoryWithHints(NEO::CpuPageFaultManager *pageFaultHandler, void *allocPtr, NEO::CpuPageFaultManager::PageFaultData &pageFaultData);

} // namespace L0
