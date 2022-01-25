/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

struct _ze_context_handle_t {
    virtual ~_ze_context_handle_t() = default;
};

namespace L0 {
struct DriverHandle;
struct Image;

struct Context : _ze_context_handle_t {
    inline static ze_memory_type_t parseUSMType(InternalMemoryType memoryType) {
        switch (memoryType) {
        case InternalMemoryType::SHARED_UNIFIED_MEMORY:
            return ZE_MEMORY_TYPE_SHARED;
        case InternalMemoryType::DEVICE_UNIFIED_MEMORY:
            return ZE_MEMORY_TYPE_DEVICE;
        case InternalMemoryType::HOST_UNIFIED_MEMORY:
            return ZE_MEMORY_TYPE_HOST;
        default:
            return ZE_MEMORY_TYPE_UNKNOWN;
        }

        return ZE_MEMORY_TYPE_UNKNOWN;
    }

    virtual ~Context() = default;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getStatus() = 0;
    virtual DriverHandle *getDriverHandle() = 0;
    virtual ze_result_t allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                                     size_t size,
                                     size_t alignment,
                                     void **ptr) = 0;
    virtual ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                                       const ze_device_mem_alloc_desc_t *deviceDesc,
                                       size_t size,
                                       size_t alignment, void **ptr) = 0;
    virtual ze_result_t allocSharedMem(ze_device_handle_t hDevice,
                                       const ze_device_mem_alloc_desc_t *deviceDesc,
                                       const ze_host_mem_alloc_desc_t *hostDesc,
                                       size_t size,
                                       size_t alignment,
                                       void **ptr) = 0;
    virtual ze_result_t freeMem(const void *ptr) = 0;
    virtual ze_result_t freeMem(const void *ptr, bool blocking) = 0;
    virtual ze_result_t freeMemExt(const ze_memory_free_ext_desc_t *pMemFreeDesc,
                                   void *ptr) = 0;
    virtual ze_result_t makeMemoryResident(ze_device_handle_t hDevice,
                                           void *ptr,
                                           size_t size) = 0;
    virtual ze_result_t evictMemory(ze_device_handle_t hDevice,
                                    void *ptr,
                                    size_t size) = 0;
    virtual ze_result_t makeImageResident(ze_device_handle_t hDevice, ze_image_handle_t hImage) = 0;
    virtual ze_result_t evictImage(ze_device_handle_t hDevice, ze_image_handle_t hImage) = 0;
    virtual ze_result_t getMemAddressRange(const void *ptr,
                                           void **pBase,
                                           size_t *pSize) = 0;
    virtual ze_result_t closeIpcMemHandle(const void *ptr) = 0;
    virtual ze_result_t getIpcMemHandle(const void *ptr,
                                        ze_ipc_mem_handle_t *pIpcHandle) = 0;
    virtual ze_result_t openIpcMemHandle(ze_device_handle_t hDevice,
                                         ze_ipc_mem_handle_t handle,
                                         ze_ipc_memory_flags_t flags,
                                         void **ptr) = 0;
    virtual ze_result_t getMemAllocProperties(const void *ptr,
                                              ze_memory_allocation_properties_t *pMemAllocProperties,
                                              ze_device_handle_t *phDevice) = 0;
    virtual ze_result_t getImageAllocProperties(Image *image,
                                                ze_image_allocation_ext_properties_t *pAllocProperties) = 0;
    virtual ze_result_t createModule(ze_device_handle_t hDevice,
                                     const ze_module_desc_t *desc,
                                     ze_module_handle_t *phModule,
                                     ze_module_build_log_handle_t *phBuildLog) = 0;
    virtual ze_result_t createSampler(ze_device_handle_t hDevice,
                                      const ze_sampler_desc_t *pDesc,
                                      ze_sampler_handle_t *phSampler) = 0;
    virtual ze_result_t createCommandQueue(ze_device_handle_t hDevice,
                                           const ze_command_queue_desc_t *desc,
                                           ze_command_queue_handle_t *commandQueue) = 0;
    virtual ze_result_t createCommandList(ze_device_handle_t hDevice,
                                          const ze_command_list_desc_t *desc,
                                          ze_command_list_handle_t *commandList) = 0;
    virtual ze_result_t createCommandListImmediate(ze_device_handle_t hDevice,
                                                   const ze_command_queue_desc_t *desc,
                                                   ze_command_list_handle_t *commandList) = 0;
    virtual ze_result_t activateMetricGroups(zet_device_handle_t hDevice,
                                             uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual ze_result_t reserveVirtualMem(const void *pStart,
                                          size_t size,
                                          void **pptr) = 0;
    virtual ze_result_t freeVirtualMem(const void *ptr,
                                       size_t size) = 0;
    virtual ze_result_t queryVirtualMemPageSize(ze_device_handle_t hDevice,
                                                size_t size,
                                                size_t *pagesize) = 0;
    virtual ze_result_t createPhysicalMem(ze_device_handle_t hDevice,
                                          ze_physical_mem_desc_t *desc,
                                          ze_physical_mem_handle_t *phPhysicalMemory) = 0;
    virtual ze_result_t destroyPhysicalMem(ze_physical_mem_handle_t hPhysicalMemory) = 0;
    virtual ze_result_t mapVirtualMem(const void *ptr,
                                      size_t size,
                                      ze_physical_mem_handle_t hPhysicalMemory,
                                      size_t offset,
                                      ze_memory_access_attribute_t access) = 0;
    virtual ze_result_t unMapVirtualMem(const void *ptr,
                                        size_t size) = 0;
    virtual ze_result_t setVirtualMemAccessAttribute(const void *ptr,
                                                     size_t size,
                                                     ze_memory_access_attribute_t access) = 0;
    virtual ze_result_t getVirtualMemAccessAttribute(const void *ptr,
                                                     size_t size,
                                                     ze_memory_access_attribute_t *access,
                                                     size_t *outSize) = 0;
    virtual ze_result_t openEventPoolIpcHandle(ze_ipc_event_pool_handle_t hIpc,
                                               ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                        uint32_t numDevices,
                                        ze_device_handle_t *phDevices,
                                        ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t createImage(ze_device_handle_t hDevice,
                                    const ze_image_desc_t *desc,
                                    ze_image_handle_t *phImage) = 0;
    virtual bool isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice) = 0;
    virtual void *getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, ze_ipc_memory_flags_t flags) = 0;

    static Context *fromHandle(ze_context_handle_t handle) { return static_cast<Context *>(handle); }
    inline ze_context_handle_t toHandle() { return this; }
};

} // namespace L0
