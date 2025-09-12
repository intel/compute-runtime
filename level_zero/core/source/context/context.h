/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"
#include "level_zero/driver_experimental/zex_context.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

struct _ze_context_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_CONTEXT> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_context_handle_t>);

struct _ze_physical_mem_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_PHYSICAL_MEM> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_physical_mem_handle_t>);

namespace NEO {
class Device;
}

namespace L0 {
struct DriverHandle;
struct Image;

class ContextExt;

#pragma pack(1)
struct IpcMemoryData {
    uint64_t handle = 0;
    uint64_t poolOffset = 0;
    uint8_t type = 0;
};
#pragma pack()
static_assert(sizeof(IpcMemoryData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcMemoryData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

enum class IpcHandleType : uint8_t {
    fdHandle = 0,
    ntHandle = 1,
    maxHandle
};

#pragma pack(1)
struct IpcOpaqueMemoryData {
    union IpcHandle {
        int fd;
        uint64_t reserved;
    };
    IpcHandle handle = {};
    uint64_t poolOffset = 0;
    unsigned int processId = 0;
    IpcHandleType type = IpcHandleType::maxHandle;
    uint8_t memoryType = 0;
};
#pragma pack()
static_assert(sizeof(IpcOpaqueMemoryData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcOpaqueMemoryData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

struct IpcHandleTracking {
    uint64_t refcnt = 0;
    NEO::GraphicsAllocation *alloc = nullptr;
    uint32_t handleId = 0;
    uint64_t handle = 0;
    uint64_t ptr = 0;
    bool opaqueIpcHandle = false;
    struct IpcMemoryData ipcData = {};
    struct IpcOpaqueMemoryData opaqueData = {};
};

struct Context : _ze_context_handle_t {
    inline static ze_memory_type_t parseUSMType(InternalMemoryType memoryType) {
        switch (memoryType) {
        case InternalMemoryType::sharedUnifiedMemory:
            return ZE_MEMORY_TYPE_SHARED;
        case InternalMemoryType::deviceUnifiedMemory:
        case InternalMemoryType::reservedDeviceMemory:
            return ZE_MEMORY_TYPE_DEVICE;
        case InternalMemoryType::hostUnifiedMemory:
        case InternalMemoryType::reservedHostMemory:
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
    virtual ze_result_t registerMemoryFreeCallback(zex_memory_free_callback_ext_desc_t *pfnCallbackDesc, void *ptr) = 0;
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
    virtual ze_result_t putIpcMemHandle(ze_ipc_mem_handle_t ipcHandle) = 0;
    virtual ze_result_t getIpcMemHandle(const void *ptr,
                                        ze_ipc_mem_handle_t *pIpcHandle) = 0;
    virtual ze_result_t getIpcHandleFromFd(uint64_t handle, ze_ipc_mem_handle_t *pIpcHandle) = 0;
    virtual ze_result_t getFdFromIpcHandle(ze_ipc_mem_handle_t ipcHandle, uint64_t *pHandle) = 0;
    virtual ze_result_t
    getIpcMemHandles(
        const void *ptr,
        uint32_t *numIpcHandles,
        ze_ipc_mem_handle_t *pIpcHandles) = 0;

    virtual ze_result_t
    openIpcMemHandles(
        ze_device_handle_t hDevice,
        uint32_t numIpcHandles,
        ze_ipc_mem_handle_t *pIpcHandles,
        ze_ipc_memory_flags_t flags,
        void **pptr) = 0;

    virtual ze_result_t openIpcMemHandle(ze_device_handle_t hDevice,
                                         const ze_ipc_mem_handle_t &handle,
                                         ze_ipc_memory_flags_t flags,
                                         void **ptr) = 0;
    virtual ze_result_t getMemAllocProperties(const void *ptr,
                                              ze_memory_allocation_properties_t *pMemAllocProperties,
                                              ze_device_handle_t *phDevice) = 0;
    virtual ze_result_t getImageAllocProperties(Image *image,
                                                ze_image_allocation_ext_properties_t *pAllocProperties) = 0;
    virtual ze_result_t setAtomicAccessAttribute(ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t attr) = 0;
    virtual ze_result_t getAtomicAccessAttribute(ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t *pAttr) = 0;
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
    virtual ze_result_t openEventPoolIpcHandle(const ze_ipc_event_pool_handle_t &ipcEventPoolHandle,
                                               ze_event_pool_handle_t *eventPoolHandle) = 0;
    virtual ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                        uint32_t numDevices,
                                        ze_device_handle_t *phDevices,
                                        ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t createImage(ze_device_handle_t hDevice,
                                    const ze_image_desc_t *desc,
                                    ze_image_handle_t *phImage) = 0;
    virtual ze_result_t getVirtualAddressSpaceIpcHandle(ze_device_handle_t hDevice,
                                                        ze_ipc_mem_handle_t *pIpcHandle) = 0;
    virtual ze_result_t putVirtualAddressSpaceIpcHandle(ze_ipc_mem_handle_t ipcHandle) = 0;
    virtual ze_result_t lockMemory(ze_device_handle_t hDevice, void *ptr, size_t size) = 0;
    virtual bool isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) = 0;
    virtual void *getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, unsigned int processId, ze_ipc_memory_flags_t flags) = 0;
    virtual void getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset) = 0;
    virtual bool isOpaqueHandleSupported(IpcHandleType *handleType) = 0;

    virtual ze_result_t getPitchFor2dImage(
        ze_device_handle_t hDevice,
        size_t imageWidth,
        size_t imageHeight,
        unsigned int elementSizeInBytes,
        size_t *rowPitch) = 0;

    static Context *fromHandle(ze_context_handle_t handle) { return static_cast<Context *>(handle); }
    inline ze_context_handle_t toHandle() { return this; }

    virtual ContextExt *getContextExt() = 0;
};

} // namespace L0
