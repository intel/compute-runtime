/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_api.h>

struct _ze_driver_handle_t {
    virtual ~_ze_driver_handle_t() = default;
};

namespace L0 {
struct Device;

struct DriverHandle : _ze_driver_handle_t {
    virtual ze_result_t getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) = 0;
    virtual ze_result_t getProperties(ze_driver_properties_t *properties) = 0;
    virtual ze_result_t getApiVersion(ze_api_version_t *version) = 0;
    virtual ze_result_t getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) = 0;
    virtual ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) = 0;
    virtual ze_result_t getMemAllocProperties(const void *ptr,
                                              ze_memory_allocation_properties_t *pMemAllocProperties,
                                              ze_device_handle_t *phDevice) = 0;

    virtual ze_result_t allocHostMem(ze_host_mem_alloc_flag_t flags, size_t size, size_t alignment, void **ptr) = 0;

    virtual ze_result_t allocDeviceMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t flags, size_t size,
                                       size_t alignment, void **ptr) = 0;

    virtual ze_result_t allocSharedMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t deviceFlags,
                                       ze_host_mem_alloc_flag_t hostFlags, size_t size, size_t alignment,
                                       void **ptr) = 0;
    virtual ze_result_t freeMem(const void *ptr) = 0;
    virtual NEO::MemoryManager *getMemoryManager() = 0;
    virtual void setMemoryManager(NEO::MemoryManager *memoryManager) = 0;
    virtual ze_result_t getMemAddressRange(const void *ptr, void **pBase, size_t *pSize) = 0;
    virtual ze_result_t closeIpcMemHandle(const void *ptr) = 0;
    virtual ze_result_t getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) = 0;
    virtual ze_result_t openIpcMemHandle(ze_device_handle_t hDevice, ze_ipc_mem_handle_t handle,
                                         ze_ipc_memory_flag_t flags, void **ptr) = 0;
    virtual ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                        uint32_t numDevices,
                                        ze_device_handle_t *phDevices,
                                        ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t openEventPoolIpcHandle(ze_ipc_event_pool_handle_t hIpc, ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t checkMemoryAccessFromDevice(Device *device, const void *ptr) = 0;
    virtual NEO::GraphicsAllocation *allocateManagedMemoryFromHostPtr(Device *device, void *buffer,
                                                                      size_t size, struct CommandList *commandList) = 0;
    virtual NEO::GraphicsAllocation *allocateMemoryFromHostPtr(Device *device, const void *buffer, size_t size) = 0;
    virtual bool findAllocationDataForRange(const void *buffer,
                                            size_t size,
                                            NEO::SvmAllocationData **allocData) = 0;
    virtual std::vector<NEO::SvmAllocationData *> findAllocationsWithinRange(const void *buffer,
                                                                             size_t size,
                                                                             bool *allocationRangeCovered) = 0;

    virtual NEO::SVMAllocsManager *getSvmAllocsManager() = 0;
    static DriverHandle *fromHandle(ze_driver_handle_t handle) { return static_cast<DriverHandle *>(handle); }
    inline ze_driver_handle_t toHandle() { return this; }

    DriverHandle &operator=(const DriverHandle &) = delete;
    DriverHandle &operator=(DriverHandle &&) = delete;

    static DriverHandle *create(std::vector<std::unique_ptr<NEO::Device>> devices);
};

} // namespace L0
