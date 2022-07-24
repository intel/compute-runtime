/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/get_extension_function_lookup_map.h"

#include <map>
#include <mutex>

namespace L0 {
class HostPointerManager;

struct DriverHandleImp : public DriverHandle {
    ~DriverHandleImp() override;
    DriverHandleImp();

    ze_result_t createContext(const ze_context_desc_t *desc,
                              uint32_t numDevices,
                              ze_device_handle_t *phDevices,
                              ze_context_handle_t *phContext) override;
    ze_result_t getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) override;
    ze_result_t getProperties(ze_driver_properties_t *properties) override;
    ze_result_t getApiVersion(ze_api_version_t *version) override;
    ze_result_t getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) override;
    ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) override;
    ze_result_t getExtensionProperties(uint32_t *pCount,
                                       ze_driver_extension_properties_t *pExtensionProperties) override;

    NEO::MemoryManager *getMemoryManager() override;
    void setMemoryManager(NEO::MemoryManager *memoryManager) override;
    MOCKABLE_VIRTUAL void *importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAlloc);
    MOCKABLE_VIRTUAL void *importFdHandles(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, std::vector<NEO::osHandle> handles, NEO::GraphicsAllocation **pAlloc);
    MOCKABLE_VIRTUAL void *importNTHandle(ze_device_handle_t hDevice, void *handle);
    ze_result_t checkMemoryAccessFromDevice(Device *device, const void *ptr) override;
    NEO::SVMAllocsManager *getSvmAllocsManager() override;
    ze_result_t initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices);
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData **allocData) override;
    std::vector<NEO::SvmAllocationData *> findAllocationsWithinRange(const void *buffer,
                                                                     size_t size,
                                                                     bool *allocationRangeCovered) override;

    ze_result_t sysmanEventsListen(uint32_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                   uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;

    ze_result_t sysmanEventsListenEx(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                     uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;

    ze_result_t importExternalPointer(void *ptr, size_t size) override;
    ze_result_t releaseImportedPointer(void *ptr) override;
    ze_result_t getHostPointerBaseAddress(void *ptr, void **baseAddress) override;

    NEO::GraphicsAllocation *findHostPointerAllocation(void *ptr, size_t size, uint32_t rootDeviceIndex) override;
    NEO::GraphicsAllocation *getDriverSystemMemoryAllocation(void *ptr,
                                                             size_t size,
                                                             uint32_t rootDeviceIndex,
                                                             uintptr_t *gpuAddress) override;
    NEO::GraphicsAllocation *getPeerAllocation(Device *device,
                                               NEO::SvmAllocationData *allocData,
                                               void *basePtr,
                                               uintptr_t *peerGpuAddress);
    void createHostPointerManager();
    void sortNeoDevices(std::vector<std::unique_ptr<NEO::Device>> &neoDevices);

    bool isRemoteResourceNeeded(void *ptr,
                                NEO::GraphicsAllocation *alloc,
                                NEO::SvmAllocationData *allocData,
                                Device *device);

    std::unique_ptr<HostPointerManager> hostPointerManager;
    // Experimental functions
    std::unordered_map<std::string, void *> extensionFunctionsLookupMap;

    std::mutex sharedMakeResidentAllocationsLock;
    std::map<void *, NEO::GraphicsAllocation *> sharedMakeResidentAllocations;

    std::vector<Device *> devices;
    // Spec extensions
    const std::vector<std::pair<std::string, uint32_t>> extensionsSupported = {
        {ZE_FLOAT_ATOMICS_EXT_NAME, ZE_FLOAT_ATOMICS_EXT_VERSION_CURRENT},
        {ZE_RELAXED_ALLOCATION_LIMITS_EXP_NAME, ZE_RELAXED_ALLOCATION_LIMITS_EXP_VERSION_CURRENT},
        {ZE_MODULE_PROGRAM_EXP_NAME, ZE_MODULE_PROGRAM_EXP_VERSION_CURRENT},
        {ZE_KERNEL_SCHEDULING_HINTS_EXP_NAME, ZE_SCHEDULING_HINTS_EXP_VERSION_CURRENT},
        {ZE_GLOBAL_OFFSET_EXP_NAME, ZE_GLOBAL_OFFSET_EXP_VERSION_CURRENT},
        {ZE_PCI_PROPERTIES_EXT_NAME, ZE_PCI_PROPERTIES_EXT_VERSION_CURRENT},
        {ZE_MEMORY_COMPRESSION_HINTS_EXT_NAME, ZE_MEMORY_COMPRESSION_HINTS_EXT_VERSION_CURRENT},
        {ZE_MEMORY_FREE_POLICIES_EXT_NAME, ZE_MEMORY_FREE_POLICIES_EXT_VERSION_CURRENT},
        {ZE_DEVICE_MEMORY_PROPERTIES_EXT_NAME, ZE_DEVICE_MEMORY_PROPERTIES_EXT_VERSION_CURRENT}};

    uint64_t uuidTimestamp = 0u;

    NEO::MemoryManager *memoryManager = nullptr;
    NEO::SVMAllocsManager *svmAllocsManager = nullptr;

    uint32_t numDevices = 0;

    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitfields;
    void updateRootDeviceBitFields(std::unique_ptr<NEO::Device> &neoDevice);
    void enableRootDeviceDebugger(std::unique_ptr<NEO::Device> &neoDevice);

    // Environment Variables
    bool enableProgramDebugging = false;
    bool enableSysman = false;
    bool enablePciIdDeviceOrder = false;
    uint8_t powerHint = 0;
};

extern struct DriverHandleImp *GlobalDriver;

} // namespace L0
