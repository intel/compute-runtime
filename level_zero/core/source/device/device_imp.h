/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/topology_map.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/utilities/spinlock.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <map>
#include <mutex>

namespace L0 {
struct SysmanDevice;

struct DeviceImp : public Device {
    ze_result_t canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) override;
    ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                  ze_command_list_handle_t *commandList) override;
    ze_result_t createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                           ze_command_list_handle_t *phCommandList) override;
    ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                   ze_command_queue_handle_t *commandQueue) override;
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
    ze_result_t getMemoryProperties(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) override;
    ze_result_t getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) override;
    ze_result_t getProperties(ze_device_properties_t *pDeviceProperties) override;
    ze_result_t getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) override;
    ze_result_t getCacheProperties(uint32_t *pCount, ze_device_cache_properties_t *pCacheProperties) override;
    ze_result_t reserveCache(size_t cacheLevel, size_t cacheReservationSize) override;
    ze_result_t setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) override;
    ze_result_t imageGetProperties(const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) override;
    ze_result_t getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) override;
    ze_result_t getCommandQueueGroupProperties(uint32_t *pCount,
                                               ze_command_queue_group_properties_t *pCommandQueueGroupProperties) override;
    ze_result_t getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) override;
    ze_result_t getGlobalTimestamps(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) override;
    ze_result_t getDebugProperties(zet_device_debug_properties_t *pDebugProperties) override;

    ze_result_t systemBarrier() override;
    void *getExecEnvironment() override;
    BuiltinFunctionsLib *getBuiltinFunctionsLib() override;
    uint32_t getMOCS(bool l3enabled, bool l1enabled) override;
    NEO::HwHelper &getHwHelper() override;
    const NEO::HardwareInfo &getHwInfo() const override;
    NEO::OSInterface &getOsInterface() override;
    uint32_t getPlatformInfo() const override;
    MetricDeviceContext &getMetricDeviceContext() override;
    DebugSession *getDebugSession(const zet_debug_config_t &config) override;
    DebugSession *createDebugSession(const zet_debug_config_t &config, ze_result_t &result) override;
    void removeDebugSession() override { debugSession.release(); }

    uint32_t getMaxNumHwThreads() const override;
    ze_result_t activateMetricGroupsDeferred(uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups) override;

    DriverHandle *getDriverHandle() override;
    void setDriverHandle(DriverHandle *driverHandle) override;
    NEO::PreemptionMode getDevicePreemptionMode() const override;
    const NEO::DeviceInfo &getDeviceInfo() const override;

    void activateMetricGroups() override;
    void processAdditionalKernelProperties(NEO::HwHelper &hwHelper, ze_device_module_properties_t *pKernelProperties);
    NEO::GraphicsAllocation *getDebugSurface() const override { return debugSurface; }
    void setDebugSurface(NEO::GraphicsAllocation *debugSurface) { this->debugSurface = debugSurface; };
    ~DeviceImp() override;
    NEO::GraphicsAllocation *allocateManagedMemoryFromHostPtr(void *buffer, size_t size, struct CommandList *commandList) override;
    NEO::GraphicsAllocation *allocateMemoryFromHostPtr(const void *buffer, size_t size, bool hostCopyAllowed) override;
    void setSysmanHandle(SysmanDevice *pSysman) override;
    SysmanDevice *getSysmanHandle() override;
    ze_result_t getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index) override;
    ze_result_t getCsrForLowPriority(NEO::CommandStreamReceiver **csr) override;
    NEO::GraphicsAllocation *obtainReusableAllocation(size_t requiredSize, NEO::AllocationType type) override;
    void storeReusableAllocation(NEO::GraphicsAllocation &alloc) override;
    NEO::Device *getActiveDevice() const;

    bool toPhysicalSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t &deviceIndex);
    bool toApiSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t deviceIndex);
    uint32_t getPhysicalSubDeviceId();

    bool isSubdevice = false;
    void *execEnvironment = nullptr;
    std::unique_ptr<BuiltinFunctionsLib> builtins = nullptr;
    std::unique_ptr<MetricDeviceContext> metricContext = nullptr;
    std::unique_ptr<CacheReservation> cacheReservation = nullptr;
    uint32_t maxNumHwThreads = 0;
    uint32_t numSubDevices = 0;
    std::vector<Device *> subDevices;
    std::unordered_map<uint32_t, bool> crossAccessEnabledDevices;
    DriverHandle *driverHandle = nullptr;
    CommandList *pageFaultCommandList = nullptr;
    ze_pci_speed_ext_t pciMaxSpeed = {-1, -1, -1};

    bool resourcesReleased = false;
    void releaseResources();

    NEO::SVMAllocsManager::MapBasedAllocationTracker peerAllocations;
    NEO::SpinLock peerAllocationsMutex;
    std::map<NEO::SvmAllocationData *, NEO::MemAdviseFlags> memAdviseSharedAllocations;
    std::unique_ptr<NEO::AllocationsList> allocationsForReuse;
    std::unique_ptr<NEO::DriverInfo> driverInfo;
    void createSysmanHandle(bool isSubDevice);

  protected:
    NEO::GraphicsAllocation *debugSurface = nullptr;
    SysmanDevice *pSysmanDevice = nullptr;
    std::unique_ptr<DebugSession> debugSession = nullptr;
};

void handleGpuDomainTransferForHwWithHints(NEO::PageFaultManager *pageFaultHandler, void *allocPtr, NEO::PageFaultManager::PageFaultData &pageFaultData);

} // namespace L0
