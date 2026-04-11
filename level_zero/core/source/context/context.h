/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"
#include "level_zero/driver_experimental/zex_context.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <map>
#include <mutex>
#include <utility>

struct _ze_context_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_CONTEXT> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_context_handle_t>);

struct _ze_physical_mem_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_PHYSICAL_MEM> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_physical_mem_handle_t>);

namespace NEO {
class GraphicsAllocation;
enum class HeapIndex : uint32_t;
struct SvmAllocationData;
struct VirtualMemoryReservation;
} // namespace NEO

namespace L0 {
class DriverHandle;
struct Image;
struct StructuresLookupTable;
struct Device;
struct IpcCounterBasedEventData;

class ContextExt;

ContextExt *createContextExt(DriverHandle *driverHandle);
void destroyContextExt(ContextExt *ctxExt);

#pragma pack(1)
struct IpcMemoryData {
    uint64_t handle = 0;
    uint64_t poolOffset = 0;
    uint8_t type = 0;
};
#pragma pack()
static_assert(sizeof(IpcMemoryData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcMemoryData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

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
    uint8_t reservedHandleData[32] = {0};
    // Computes and returns the cache ID hash
    uint64_t computeCacheID() const noexcept;
    bool compressedMemory = false;
};
#pragma pack()
static_assert(sizeof(IpcOpaqueMemoryData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcOpaqueMemoryData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

constexpr uint64_t ipcOpaqueHashRatio = 0x9e3779b97f4a7c15ull;

template <class T>
inline void ipcHashCombine(uint64_t &seed, const T &v) {
    std::hash<T> h;
    seed ^= h(v) + ipcOpaqueHashRatio + (seed << 6) + (seed >> 2);
}

inline uint64_t normalizeIPCHandle(const IpcOpaqueMemoryData &x) {
    switch (x.type) {
    case IpcHandleType::fdHandle:
        return static_cast<uint64_t>(static_cast<int64_t>(x.handle.fd));
    case IpcHandleType::ntHandle:
        return x.handle.reserved;
    default:
        return 0ull;
    }
}

struct IpcHandleTracking {
    uint64_t refcnt = 0;
    NEO::GraphicsAllocation *alloc = nullptr;
    uint32_t handleId = 0;
    uint64_t handle = 0;
    uint64_t ptr = 0;
    uint64_t cacheID = 0;
    struct IpcMemoryData ipcData = {};
    struct IpcOpaqueMemoryData opaqueData = {};
    bool hasReservedHandleData = false;
};

#ifndef BIT
#define BIT(x) (1u << (x))
#endif

enum OpaqueHandlingType : uint8_t {
    none = 0,
    pidfd = BIT(0),
    sockets = BIT(1),
    nthandle = BIT(2)
};

struct ContextSettings {
    uint8_t useOpaqueHandle = OpaqueHandlingType::pidfd | OpaqueHandlingType::sockets | OpaqueHandlingType::nthandle;
    bool enableSvmHeapReservation = true;
    IpcHandleType handleType = IpcHandleType::maxHandle;
};

struct Context : _ze_context_handle_t, NEO::NonCopyableAndNonMovableClass {
    Context(DriverHandle *driverHandle);
    virtual ~Context();

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

    MOCKABLE_VIRTUAL ze_result_t destroy();
    MOCKABLE_VIRTUAL ze_result_t getStatus();
    MOCKABLE_VIRTUAL DriverHandle *getDriverHandle();
    MOCKABLE_VIRTUAL ze_result_t allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                                              size_t size,
                                              size_t alignment,
                                              void **ptr);
    MOCKABLE_VIRTUAL ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                                                const ze_device_mem_alloc_desc_t *deviceDesc,
                                                size_t size,
                                                size_t alignment, void **ptr);
    MOCKABLE_VIRTUAL ze_result_t allocSharedMem(ze_device_handle_t hDevice,
                                                const ze_device_mem_alloc_desc_t *deviceDesc,
                                                const ze_host_mem_alloc_desc_t *hostDesc,
                                                size_t size,
                                                size_t alignment,
                                                void **ptr);
    MOCKABLE_VIRTUAL ze_result_t freeMem(const void *ptr);
    MOCKABLE_VIRTUAL ze_result_t freeMem(const void *ptr, bool blocking);
    MOCKABLE_VIRTUAL ze_result_t freeMemExt(const ze_memory_free_ext_desc_t *pMemFreeDesc,
                                            void *ptr);
    MOCKABLE_VIRTUAL ze_result_t registerMemoryFreeCallback(zex_memory_free_callback_ext_desc_t *pfnCallbackDesc, void *ptr);
    MOCKABLE_VIRTUAL ze_result_t makeMemoryResident(ze_device_handle_t hDevice,
                                                    void *ptr,
                                                    size_t size);
    MOCKABLE_VIRTUAL ze_result_t evictMemory(ze_device_handle_t hDevice,
                                             void *ptr,
                                             size_t size);
    MOCKABLE_VIRTUAL ze_result_t makeImageResident(ze_device_handle_t hDevice, ze_image_handle_t hImage);
    MOCKABLE_VIRTUAL ze_result_t evictImage(ze_device_handle_t hDevice, ze_image_handle_t hImage);
    MOCKABLE_VIRTUAL ze_result_t getMemAddressRange(const void *ptr,
                                                    void **pBase,
                                                    size_t *pSize);
    MOCKABLE_VIRTUAL ze_result_t closeIpcMemHandle(const void *ptr);
    MOCKABLE_VIRTUAL ze_result_t putIpcMemHandle(ze_ipc_mem_handle_t ipcHandle);
    MOCKABLE_VIRTUAL ze_result_t getIpcMemHandle(const void *ptr,
                                                 void *pNext,
                                                 ze_ipc_mem_handle_t *pIpcHandle);
    MOCKABLE_VIRTUAL ze_result_t getIpcHandleFromFd(uint64_t handle, ze_ipc_mem_handle_t *pIpcHandle);
    MOCKABLE_VIRTUAL ze_result_t getFdFromIpcHandle(ze_ipc_mem_handle_t ipcHandle, uint64_t *pHandle);
    MOCKABLE_VIRTUAL ze_result_t lockMemory(ze_device_handle_t hDevice, void *ptr, size_t size);

    MOCKABLE_VIRTUAL ze_result_t
    getIpcMemHandles(
        const void *ptr,
        uint32_t *numIpcHandles,
        ze_ipc_mem_handle_t *pIpcHandles);

    MOCKABLE_VIRTUAL ze_result_t
    openIpcMemHandles(
        ze_device_handle_t hDevice,
        uint32_t numIpcHandles,
        ze_ipc_mem_handle_t *pIpcHandles,
        ze_ipc_memory_flags_t flags,
        void **pptr);

    MOCKABLE_VIRTUAL ze_result_t openIpcMemHandle(ze_device_handle_t hDevice,
                                                  const ze_ipc_mem_handle_t &handle,
                                                  ze_ipc_memory_flags_t flags,
                                                  void **ptr);
    MOCKABLE_VIRTUAL ze_result_t getMemAllocProperties(const void *ptr,
                                                       ze_memory_allocation_properties_t *pMemAllocProperties,
                                                       ze_device_handle_t *phDevice);
    MOCKABLE_VIRTUAL ze_result_t getImageAllocProperties(Image *image,
                                                         ze_image_allocation_ext_properties_t *pAllocProperties);
    MOCKABLE_VIRTUAL ze_result_t setAtomicAccessAttribute(ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t attr);
    MOCKABLE_VIRTUAL ze_result_t getAtomicAccessAttribute(ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t *pAttr);
    MOCKABLE_VIRTUAL ze_result_t createModule(ze_device_handle_t hDevice,
                                              const ze_module_desc_t *desc,
                                              ze_module_handle_t *phModule,
                                              ze_module_build_log_handle_t *phBuildLog);
    MOCKABLE_VIRTUAL ze_result_t createSampler(ze_device_handle_t hDevice,
                                               const ze_sampler_desc_t *pDesc,
                                               ze_sampler_handle_t *phSampler);
    MOCKABLE_VIRTUAL ze_result_t createCommandQueue(ze_device_handle_t hDevice,
                                                    const ze_command_queue_desc_t *desc,
                                                    ze_command_queue_handle_t *commandQueue);
    MOCKABLE_VIRTUAL ze_result_t createCommandList(ze_device_handle_t hDevice,
                                                   const ze_command_list_desc_t *desc,
                                                   ze_command_list_handle_t *commandList);
    MOCKABLE_VIRTUAL ze_result_t createCommandListImmediate(ze_device_handle_t hDevice,
                                                            const ze_command_queue_desc_t *desc,
                                                            ze_command_list_handle_t *commandList);
    MOCKABLE_VIRTUAL ze_result_t activateMetricGroups(zet_device_handle_t hDevice,
                                                      uint32_t count,
                                                      zet_metric_group_handle_t *phMetricGroups);
    MOCKABLE_VIRTUAL ze_result_t reserveVirtualMem(const void *pStart,
                                                   size_t size,
                                                   void **pptr);
    MOCKABLE_VIRTUAL ze_result_t freeVirtualMem(const void *ptr,
                                                size_t size);
    MOCKABLE_VIRTUAL ze_result_t queryVirtualMemPageSizeWithStartAddress(ze_device_handle_t hDevice,
                                                                         const void *pStart,
                                                                         size_t size,
                                                                         size_t *pagesize);
    MOCKABLE_VIRTUAL ze_result_t queryVirtualMemPageSize(ze_device_handle_t hDevice,
                                                         size_t size,
                                                         size_t *pagesize);
    MOCKABLE_VIRTUAL ze_result_t createPhysicalMem(ze_device_handle_t hDevice,
                                                   ze_physical_mem_desc_t *desc,
                                                   ze_physical_mem_handle_t *phPhysicalMemory);
    MOCKABLE_VIRTUAL ze_result_t destroyPhysicalMem(ze_physical_mem_handle_t hPhysicalMemory);
    MOCKABLE_VIRTUAL ze_result_t getPhysicalMemProperties(ze_physical_mem_handle_t hPhysicalMemory,
                                                          ze_physical_mem_properties_t *pMemProperties);
    MOCKABLE_VIRTUAL ze_result_t mapVirtualMem(const void *ptr,
                                               size_t size,
                                               ze_physical_mem_handle_t hPhysicalMemory,
                                               size_t offset,
                                               ze_memory_access_attribute_t access);
    MOCKABLE_VIRTUAL ze_result_t unMapVirtualMem(const void *ptr,
                                                 size_t size);
    MOCKABLE_VIRTUAL ze_result_t setVirtualMemAccessAttribute(const void *ptr,
                                                              size_t size,
                                                              ze_memory_access_attribute_t access);
    MOCKABLE_VIRTUAL ze_result_t getVirtualMemAccessAttribute(const void *ptr,
                                                              size_t size,
                                                              ze_memory_access_attribute_t *access,
                                                              size_t *outSize);
    MOCKABLE_VIRTUAL ze_result_t openEventPoolIpcHandle(const ze_ipc_event_pool_handle_t &ipcEventPoolHandle,
                                                        ze_event_pool_handle_t *eventPoolHandle);

    ze_result_t openCounterBasedIpcHandle(const IpcCounterBasedEventData &ipcData, ze_event_handle_t *phEvent);

    MOCKABLE_VIRTUAL ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                                 uint32_t numDevices,
                                                 ze_device_handle_t *phDevices,
                                                 ze_event_pool_handle_t *phEventPool);
    MOCKABLE_VIRTUAL ze_result_t createImage(ze_device_handle_t hDevice,
                                             const ze_image_desc_t *desc,
                                             ze_image_handle_t *phImage);
    MOCKABLE_VIRTUAL ze_result_t getVirtualAddressSpaceIpcHandle(ze_device_handle_t hDevice,
                                                                 ze_ipc_mem_handle_t *pIpcHandle);
    MOCKABLE_VIRTUAL ze_result_t putVirtualAddressSpaceIpcHandle(ze_ipc_mem_handle_t ipcHandle);

    MOCKABLE_VIRTUAL ze_result_t mapDeviceMemToHost(const void *ptr, void **pptr, void *pNext);

    MOCKABLE_VIRTUAL ze_result_t getPitchFor2dImage(
        ze_device_handle_t hDevice,
        size_t imageWidth,
        size_t imageHeight,
        unsigned int elementSizeInBytes,
        size_t *rowPitch);

    MOCKABLE_VIRTUAL bool isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle);
    MOCKABLE_VIRTUAL std::pair<NEO::GraphicsAllocation *, void *> getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, bool isHostIpcAllocation, unsigned int processId, ze_ipc_memory_flags_t flags, uint64_t cacheID, void *reservedHandleData, bool compressedMemory);
    MOCKABLE_VIRTUAL void closeExternalHandle(uint64_t handle);
    MOCKABLE_VIRTUAL void getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset, uint64_t &cacheID, void *&reservedHandleData, bool &compressedMemory);
    MOCKABLE_VIRTUAL uint8_t isOpaqueHandleSupported(IpcHandleType *handleType);

    MOCKABLE_VIRTUAL ContextExt *getContextExt() {
        return contextExt;
    }
    MOCKABLE_VIRTUAL ze_result_t systemBarrier(ze_device_handle_t hDevice);

    static Context *fromHandle(ze_context_handle_t handle) { return static_cast<Context *>(handle); }
    inline ze_context_handle_t toHandle() { return this; }

    static uint64_t computeIpcCacheId(uint64_t handle, uint64_t poolOffset, uint32_t processId, uint8_t handleType, uint8_t memoryType);

    std::map<uint32_t, ze_device_handle_t> &getDevices() {
        return devices;
    }

    MOCKABLE_VIRTUAL void freePeerAllocationsFromAll(const void *ptr, bool blocking);
    void freePeerAllocations(const void *ptr, bool blocking, Device *device);

    ze_result_t handleAllocationExtensions(NEO::GraphicsAllocation *alloc, ze_memory_type_t type,
                                           void *pNext, DriverHandle *driverHandle);

    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitfields;
    ContextSettings settings;

    bool isDeviceDefinedForThisContext(Device *inDevice);

    void initDeviceHandles(uint32_t numDevices, ze_device_handle_t *deviceHandles) {
        this->numDevices = numDevices;
        if (numDevices > 0) {
            this->deviceHandles.assign(deviceHandles, deviceHandles + numDevices);
        }
    }
    void addDeviceHandle(ze_device_handle_t deviceHandle) {
        this->deviceHandles.push_back(deviceHandle);
        this->numDevices = static_cast<uint32_t>(this->deviceHandles.size());
    }
    NEO::VirtualMemoryReservation *findSupportedVirtualReservation(const void *ptr, size_t size);
    ze_result_t checkMemSizeLimit(Device *inDevice, size_t size, bool relaxedSizeAllowed, void **ptr);

    uint32_t getNumDevices() const {
        return numDevices;
    }
    NEO::UsmMemAllocPool *getUsmPoolOwningPtr(const void *ptr, NEO::SvmAllocationData *svmData);

    bool isSocketHandleSharingSupported() const { return (((settings.useOpaqueHandle & OpaqueHandlingType::sockets) == OpaqueHandlingType::sockets) && (settings.handleType == IpcHandleType::fdHandle)); }
    void registerIpcHandleWithServer(uint64_t handleId);
    void unregisterIpcHandleWithServer(uint64_t handleId);

  protected:
    ze_result_t getIpcMemHandlesImpl(const void *ptr, void *pNext, uint32_t *numIpcHandles, ze_ipc_mem_handle_t *pIpcHandles);
    template <typename IpcDataT>
    void setIPCHandleData(NEO::GraphicsAllocation *graphicsAllocation, uint64_t handle, IpcDataT &ipcData, uint64_t ptrAddress, uint8_t type, NEO::UsmMemAllocPool *usmPool, IpcHandleType handleType, void *reservedHandleData);
    bool isAllocationSuitableForCompression(const StructuresLookupTable &structuresLookupTable, Device &device, size_t allocSize);
    size_t getPageAlignedSizeRequired(size_t size, NEO::HeapIndex *heapRequired, size_t *pageSizeRequired) {
        return getPageAlignedSizeRequired(nullptr, size, heapRequired, pageSizeRequired);
    }

    size_t getPageAlignedSizeRequired(const void *pStart, size_t size, NEO::HeapIndex *heapRequired, size_t *pageSizeRequired);
    bool tryFreeViaPooling(const void *ptr, NEO::SvmAllocationData *svmData, NEO::UsmMemAllocPool *usmPool, bool blocking);

    std::map<uint32_t, ze_device_handle_t> devices;
    std::vector<ze_device_handle_t> deviceHandles;
    DriverHandle *driverHandle = nullptr;
    uint32_t numDevices = 0;
    ContextExt *contextExt = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<Context>);

} // namespace L0
