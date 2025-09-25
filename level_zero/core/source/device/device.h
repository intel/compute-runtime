/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <memory>
#include <mutex>

static_assert(NEO::ProductHelper::uuidSize == ZE_MAX_DEVICE_UUID_SIZE);

struct _ze_device_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_DEVICE> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_device_handle_t>);

namespace NEO {
class CommandStreamReceiver;
class DebuggerL0;
class Device;
class GfxCoreHelper;
class MemoryManager;
struct DeviceInfo;
class CompilerProductHelper;
class TagAllocatorBase;
enum class AllocationType;
} // namespace NEO

namespace L0 {
struct DriverHandle;
struct BuiltinFunctionsLib;
struct ExecutionEnvironment;
class MetricDeviceContext;
struct SysmanDevice;
struct DebugSession;
class L0GfxCoreHelper;

enum class ModuleType;

struct Device : _ze_device_handle_t {
    uint32_t getRootDeviceIndex() const;

    NEO::Device *getNEODevice() const {
        return this->neoDevice;
    }

    virtual ze_result_t canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) = 0;
    virtual ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                          ze_command_list_handle_t *commandList) = 0;

    virtual ze_result_t createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                   ze_command_list_handle_t *commandList) = 0;

    virtual ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                           ze_command_queue_handle_t *commandQueue) = 0;

    virtual ze_result_t createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage) = 0;

    virtual ze_result_t createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                     ze_module_build_log_handle_t *buildLog, ModuleType type) = 0;
    virtual ze_result_t createSampler(const ze_sampler_desc_t *pDesc,
                                      ze_sampler_handle_t *phSampler) = 0;
    virtual ze_result_t getComputeProperties(ze_device_compute_properties_t *pComputeProperties) = 0;
    virtual ze_result_t getP2PProperties(ze_device_handle_t hPeerDevice,
                                         ze_device_p2p_properties_t *pP2PProperties) = 0;
    virtual ze_result_t getKernelProperties(ze_device_module_properties_t *pKernelProperties) = 0;
    virtual ze_result_t getPciProperties(ze_pci_ext_properties_t *pPciProperties) = 0;
    virtual ze_result_t getRootDevice(ze_device_handle_t *phRootDevice) = 0;
    virtual ze_result_t getMemoryProperties(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) = 0;
    virtual ze_result_t getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) = 0;
    virtual ze_result_t getProperties(ze_device_properties_t *pDeviceProperties) = 0;
    virtual ze_result_t getVectorWidthPropertiesExt(uint32_t *pCount, ze_device_vector_width_properties_ext_t *pVectorWidthProperties) = 0;
    virtual ze_result_t getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) = 0;
    virtual ze_result_t getCacheProperties(uint32_t *pCount, ze_device_cache_properties_t *pCacheProperties) = 0;
    virtual ze_result_t getStatus() = 0;
    virtual ze_result_t reserveCache(size_t cacheLevel, size_t cacheReservationSize) = 0;
    virtual ze_result_t setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) = 0;
    virtual ze_result_t imageGetProperties(const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) = 0;
    virtual ze_result_t getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) = 0;
    virtual ze_result_t getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) = 0;
    virtual ze_result_t getGlobalTimestamps(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) = 0;

    virtual ze_result_t getCommandQueueGroupProperties(uint32_t *pCount,
                                                       ze_command_queue_group_properties_t *pCommandQueueGroupProperties) = 0;
    virtual ze_result_t getDebugProperties(zet_device_debug_properties_t *pDebugProperties) = 0;

    virtual ze_result_t systemBarrier() = 0;
    virtual ze_result_t synchronize() = 0;

    virtual ~Device() = default;

    virtual BuiltinFunctionsLib *getBuiltinFunctionsLib() = 0;
    virtual uint32_t getMOCS(bool l3enabled, bool l1enabled) = 0;
    virtual uint32_t getMaxNumHwThreads() const = 0;

    virtual const NEO::GfxCoreHelper &getGfxCoreHelper() = 0;
    virtual const L0GfxCoreHelper &getL0GfxCoreHelper() = 0;
    virtual const NEO::ProductHelper &getProductHelper() = 0;
    virtual const NEO::CompilerProductHelper &getCompilerProductHelper() = 0;

    bool isImplicitScalingCapable() const {
        return implicitScalingCapable;
    }
    virtual const NEO::HardwareInfo &getHwInfo() const = 0;
    virtual NEO::OSInterface *getOsInterface() = 0;
    virtual uint32_t getPlatformInfo() const = 0;
    virtual MetricDeviceContext &getMetricDeviceContext() = 0;
    virtual DebugSession *getDebugSession(const zet_debug_config_t &config) = 0;
    virtual DebugSession *createDebugSession(const zet_debug_config_t &config, ze_result_t &result, bool isRootAttach) = 0;
    virtual void removeDebugSession() = 0;

    virtual ze_result_t activateMetricGroupsDeferred(uint32_t count,
                                                     zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual void activateMetricGroups() = 0;

    virtual DriverHandle *getDriverHandle() = 0;
    virtual void setDriverHandle(DriverHandle *driverHandle) = 0;

    static Device *fromHandle(ze_device_handle_t handle) { return static_cast<Device *>(handle); }

    inline ze_device_handle_t toHandle() { return this; }

    static Device *create(DriverHandle *driverHandle, NEO::Device *neoDevice, bool isSubDevice, ze_result_t *returnValue);
    static Device *create(DriverHandle *driverHandle, NEO::Device *neoDevice, bool isSubDevice, ze_result_t *returnValue, L0::Device *deviceL0);
    static Device *deviceReinit(DriverHandle *driverHandle, L0::Device *device, std::unique_ptr<NEO::Device> &neoDevice, ze_result_t *returnValue);

    virtual NEO::PreemptionMode getDevicePreemptionMode() const = 0;
    virtual const NEO::DeviceInfo &getDeviceInfo() const = 0;
    NEO::DebuggerL0 *getL0Debugger();

    virtual NEO::GraphicsAllocation *getDebugSurface() const = 0;

    virtual NEO::GraphicsAllocation *allocateManagedMemoryFromHostPtr(void *buffer,
                                                                      size_t size, struct CommandList *commandList) = 0;

    virtual NEO::GraphicsAllocation *allocateMemoryFromHostPtr(const void *buffer, size_t size, bool hostCopyAllowed) = 0;
    virtual void setSysmanHandle(SysmanDevice *pSysmanDevice) = 0;
    virtual SysmanDevice *getSysmanHandle() = 0;
    virtual ze_result_t getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index, ze_command_queue_priority_t priority, std::optional<int> priorityLevel, bool allocateInterrupt) = 0;
    virtual ze_result_t getCsrForLowPriority(NEO::CommandStreamReceiver **csr, bool copyOnly) = 0;
    virtual NEO::GraphicsAllocation *obtainReusableAllocation(size_t requiredSize, NEO::AllocationType type) = 0;
    virtual void storeReusableAllocation(NEO::GraphicsAllocation &alloc) = 0;
    virtual ze_result_t getFabricVertex(ze_fabric_vertex_handle_t *phVertex) = 0;
    virtual uint32_t getEventMaxPacketCount() const = 0;
    virtual uint32_t getEventMaxKernelCount() const = 0;
    virtual void bcsSplitReleaseResources() = 0;
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
    uint32_t getAggregatedCopyOffloadIncrementValue() const { return aggregatedCopyIncValue; }
    void setAggregatedCopyOffloadIncrementValue(uint32_t val) { aggregatedCopyIncValue = val; }

  protected:
    NEO::Device *neoDevice = nullptr;
    std::unique_ptr<NEO::TagAllocatorBase> deviceInOrderCounterAllocator;
    std::unique_ptr<NEO::TagAllocatorBase> hostInOrderCounterAllocator;
    std::unique_ptr<NEO::TagAllocatorBase> inOrderTimestampAllocator;
    std::unique_ptr<NEO::TagAllocatorBase> fillPatternAllocator;
    NEO::GraphicsAllocation *syncDispatchTokenAllocation = nullptr;
    std::mutex inOrderAllocatorMutex;
    std::mutex syncDispatchTokenMutex;
    std::atomic<uint32_t> syncDispatchQueueIdAllocator = 0;
    uint32_t aggregatedCopyIncValue = 0;
    uint32_t identifier = 0;
    int32_t queuePriorityHigh = 0;
    int32_t queuePriorityLow = 1;
    bool implicitScalingCapable = false;
};

} // namespace L0
