/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/get_extension_function_lookup_map.h"

namespace L0 {

struct DriverHandleImp : public DriverHandle {
    ~DriverHandleImp() override;
    ze_result_t createContext(const ze_context_desc_t *desc, ze_context_handle_t *phContext) override;
    ze_result_t getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) override;
    ze_result_t getProperties(ze_driver_properties_t *properties) override;
    ze_result_t getApiVersion(ze_api_version_t *version) override;
    ze_result_t getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) override;
    ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) override;
    ze_result_t getExtensionProperties(uint32_t *pCount,
                                       ze_driver_extension_properties_t *pExtensionProperties) override;
    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override;

    ze_result_t allocHostMem(ze_host_mem_alloc_flags_t flags, size_t size, size_t alignment, void **ptr) override;

    ze_result_t allocDeviceMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flags_t flags, size_t size,
                               size_t alignment, void **ptr) override;

    ze_result_t allocSharedMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flags_t deviceFlags,
                               ze_host_mem_alloc_flags_t hostFlags, size_t size, size_t alignment,
                               void **ptr) override;

    ze_result_t getMemAddressRange(const void *ptr, void **pBase, size_t *pSize) override;
    ze_result_t freeMem(const void *ptr) override;
    NEO::MemoryManager *getMemoryManager() override;
    void setMemoryManager(NEO::MemoryManager *memoryManager) override;
    ze_result_t closeIpcMemHandle(const void *ptr) override;
    ze_result_t getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) override;
    ze_result_t openIpcMemHandle(ze_device_handle_t hDevice, ze_ipc_mem_handle_t handle,
                                 ze_ipc_memory_flag_t flags, void **ptr) override;
    ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                uint32_t numDevices,
                                ze_device_handle_t *phDevices,
                                ze_event_pool_handle_t *phEventPool) override;
    ze_result_t openEventPoolIpcHandle(ze_ipc_event_pool_handle_t hIpc, ze_event_pool_handle_t *phEventPool) override;
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

    uint32_t parseAffinityMask(std::vector<std::unique_ptr<NEO::Device>> &neoDevices);

    uint32_t numDevices = 0;
    std::unordered_map<std::string, void *> extensionFunctionsLookupMap;
    std::vector<Device *> devices;
    NEO::MemoryManager *memoryManager = nullptr;
    NEO::SVMAllocsManager *svmAllocsManager = nullptr;
    uint64_t uuidTimestamp = 0u;

    // Environment Variables
    std::string affinityMaskString = "";
    bool enableProgramDebugging = false;
    bool enableSysman = false;
};

extern struct DriverHandleImp *GlobalDriver;

} // namespace L0
