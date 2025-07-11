/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

#include <map>

namespace L0 {
struct StructuresLookupTable;
struct DriverHandleImp;
struct Device;
struct IpcCounterBasedEventData;

ContextExt *createContextExt(DriverHandle *driverHandle);
void destroyContextExt(ContextExt *ctxExt);

struct ContextImp : Context, NEO::NonCopyableAndNonMovableClass {
    ContextImp(DriverHandle *driverHandle);
    ~ContextImp() override;
    ze_result_t destroy() override;
    ze_result_t getStatus() override;
    DriverHandle *getDriverHandle() override;
    ze_result_t allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                             size_t size,
                             size_t alignment,
                             void **ptr) override;
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override;
    ze_result_t allocSharedMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               const ze_host_mem_alloc_desc_t *hostDesc,
                               size_t size,
                               size_t alignment,
                               void **ptr) override;
    ze_result_t freeMem(const void *ptr) override;
    ze_result_t freeMem(const void *ptr, bool blocking) override;
    ze_result_t freeMemExt(const ze_memory_free_ext_desc_t *pMemFreeDesc,
                           void *ptr) override;
    ze_result_t makeMemoryResident(ze_device_handle_t hDevice,
                                   void *ptr,
                                   size_t size) override;
    ze_result_t evictMemory(ze_device_handle_t hDevice,
                            void *ptr,
                            size_t size) override;
    ze_result_t makeImageResident(ze_device_handle_t hDevice, ze_image_handle_t hImage) override;
    ze_result_t evictImage(ze_device_handle_t hDevice, ze_image_handle_t hImage) override;
    ze_result_t getMemAddressRange(const void *ptr,
                                   void **pBase,
                                   size_t *pSize) override;
    ze_result_t closeIpcMemHandle(const void *ptr) override;
    ze_result_t putIpcMemHandle(ze_ipc_mem_handle_t ipcHandle) override;
    ze_result_t getIpcMemHandle(const void *ptr,
                                ze_ipc_mem_handle_t *pIpcHandle) override;
    ze_result_t openIpcMemHandle(ze_device_handle_t hDevice,
                                 const ze_ipc_mem_handle_t &handle,
                                 ze_ipc_memory_flags_t flags,
                                 void **ptr) override;
    ze_result_t getIpcHandleFromFd(uint64_t handle, ze_ipc_mem_handle_t *pIpcHandle) override;
    ze_result_t getFdFromIpcHandle(ze_ipc_mem_handle_t ipcHandle, uint64_t *pHandle) override;
    ze_result_t lockMemory(ze_device_handle_t hDevice, void *ptr, size_t size) override;

    ze_result_t
    getIpcMemHandles(
        const void *ptr,
        uint32_t *numIpcHandles,
        ze_ipc_mem_handle_t *pIpcHandles) override;

    ze_result_t
    openIpcMemHandles(
        ze_device_handle_t hDevice,
        uint32_t numIpcHandles,
        ze_ipc_mem_handle_t *pIpcHandles,
        ze_ipc_memory_flags_t flags,
        void **pptr) override;

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override;
    ze_result_t getImageAllocProperties(Image *image,
                                        ze_image_allocation_ext_properties_t *pAllocProperties) override;
    ze_result_t setAtomicAccessAttribute(ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t attr) override;
    ze_result_t getAtomicAccessAttribute(ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t *pAttr) override;
    ze_result_t createModule(ze_device_handle_t hDevice,
                             const ze_module_desc_t *desc,
                             ze_module_handle_t *phModule,
                             ze_module_build_log_handle_t *phBuildLog) override;
    ze_result_t createSampler(ze_device_handle_t hDevice,
                              const ze_sampler_desc_t *pDesc,
                              ze_sampler_handle_t *phSampler) override;
    ze_result_t createCommandQueue(ze_device_handle_t hDevice,
                                   const ze_command_queue_desc_t *desc,
                                   ze_command_queue_handle_t *commandQueue) override;
    ze_result_t createCommandList(ze_device_handle_t hDevice,
                                  const ze_command_list_desc_t *desc,
                                  ze_command_list_handle_t *commandList) override;
    ze_result_t createCommandListImmediate(ze_device_handle_t hDevice,
                                           const ze_command_queue_desc_t *desc,
                                           ze_command_list_handle_t *commandList) override;
    ze_result_t activateMetricGroups(zet_device_handle_t hDevice,
                                     uint32_t count,
                                     zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t reserveVirtualMem(const void *pStart,
                                  size_t size,
                                  void **pptr) override;
    ze_result_t freeVirtualMem(const void *ptr,
                               size_t size) override;
    ze_result_t queryVirtualMemPageSize(ze_device_handle_t hDevice,
                                        size_t size,
                                        size_t *pagesize) override;
    ze_result_t createPhysicalMem(ze_device_handle_t hDevice,
                                  ze_physical_mem_desc_t *desc,
                                  ze_physical_mem_handle_t *phPhysicalMemory) override;
    ze_result_t destroyPhysicalMem(ze_physical_mem_handle_t hPhysicalMemory) override;
    ze_result_t mapVirtualMem(const void *ptr,
                              size_t size,
                              ze_physical_mem_handle_t hPhysicalMemory,
                              size_t offset,
                              ze_memory_access_attribute_t access) override;
    ze_result_t unMapVirtualMem(const void *ptr,
                                size_t size) override;
    ze_result_t setVirtualMemAccessAttribute(const void *ptr,
                                             size_t size,
                                             ze_memory_access_attribute_t access) override;
    ze_result_t getVirtualMemAccessAttribute(const void *ptr,
                                             size_t size,
                                             ze_memory_access_attribute_t *access,
                                             size_t *outSize) override;
    ze_result_t openEventPoolIpcHandle(const ze_ipc_event_pool_handle_t &ipcEventPoolHandle,
                                       ze_event_pool_handle_t *eventPoolHandle) override;

    ze_result_t openCounterBasedIpcHandle(const IpcCounterBasedEventData &ipcData, ze_event_handle_t *phEvent);

    ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                uint32_t numDevices,
                                ze_device_handle_t *phDevices,
                                ze_event_pool_handle_t *phEventPool) override;
    ze_result_t createImage(ze_device_handle_t hDevice,
                            const ze_image_desc_t *desc,
                            ze_image_handle_t *phImage) override;
    ze_result_t getVirtualAddressSpaceIpcHandle(ze_device_handle_t hDevice,
                                                ze_ipc_mem_handle_t *pIpcHandle) override;
    ze_result_t putVirtualAddressSpaceIpcHandle(ze_ipc_mem_handle_t ipcHandle) override;

    std::map<uint32_t, ze_device_handle_t> &getDevices() {
        return devices;
    }

    void freePeerAllocations(const void *ptr, bool blocking, Device *device);

    ze_result_t handleAllocationExtensions(NEO::GraphicsAllocation *alloc, ze_memory_type_t type,
                                           void *pNext, struct DriverHandleImp *driverHandle);

    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitfields;

    bool isDeviceDefinedForThisContext(Device *inDevice);
    bool isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice) override;
    void *getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, ze_ipc_memory_flags_t flags) override;

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

    ze_result_t getPitchFor2dImage(
        ze_device_handle_t hDevice,
        size_t imageWidth,
        size_t imageHeight,
        unsigned int elementSizeInBytes,
        size_t *rowPitch) override;

    ContextExt *getContextExt() override {
        return contextExt;
    }
    uint32_t getNumDevices() const {
        return numDevices;
    }

  protected:
    ze_result_t getIpcMemHandlesImpl(const void *ptr, uint32_t *numIpcHandles, ze_ipc_mem_handle_t *pIpcHandles);
    template <typename IpcDataT>
    void setIPCHandleData(NEO::GraphicsAllocation *graphicsAllocation, uint64_t handle, IpcDataT &ipcData, uint64_t ptrAddress, uint8_t type, NEO::UsmMemAllocPool *usmPool) {
        std::map<uint64_t, IpcHandleTracking *>::iterator ipcHandleIterator;

        ipcData = {};
        if constexpr (std::is_same_v<IpcDataT, IpcMemoryData>) {
            ipcData.handle = handle;
            ipcData.type = type;
        } else if constexpr (std::is_same_v<IpcDataT, IpcOpaqueMemoryData>) {
            printf("opaque type used\n");
            ipcData.memoryType = type;
            ipcData.processId = NEO::SysCalls::getCurrentProcessId();
            ipcData.type = IpcHandleType::fdHandle;
            ipcData.handle.fd = static_cast<int>(handle);
        }

        if (usmPool) {
            ipcData.poolOffset = usmPool->getOffsetInPool(addrToPtr(ptrAddress));
            ptrAddress = usmPool->getPoolAddress();
        }

        auto lock = this->driverHandle->lockIPCHandleMap();
        ipcHandleIterator = this->driverHandle->getIPCHandleMap().find(handle);
        if (ipcHandleIterator != this->driverHandle->getIPCHandleMap().end()) {
            ipcHandleIterator->second->refcnt += 1;
        } else {
            IpcHandleTracking *handleTracking = new IpcHandleTracking;
            handleTracking->alloc = graphicsAllocation;
            handleTracking->refcnt = 1;
            handleTracking->ptr = ptrAddress;
            handleTracking->handle = handle;
            if constexpr (std::is_same_v<IpcDataT, IpcMemoryData>) {
                handleTracking->ipcData = ipcData;
            }
            this->driverHandle->getIPCHandleMap().insert(std::pair<uint64_t, IpcHandleTracking *>(handle, handleTracking));
        }
    }
    bool isAllocationSuitableForCompression(const StructuresLookupTable &structuresLookupTable, Device &device, size_t allocSize);
    size_t getPageAlignedSizeRequired(size_t size, NEO::HeapIndex *heapRequired, size_t *pageSizeRequired);
    NEO::UsmMemAllocPool *getUsmPoolOwningPtr(const void *ptr, NEO::SvmAllocationData *svmData);
    bool tryFreeViaPooling(const void *ptr, NEO::SvmAllocationData *svmData, NEO::UsmMemAllocPool *usmPool);

    std::map<uint32_t, ze_device_handle_t> devices;
    std::vector<ze_device_handle_t> deviceHandles;
    DriverHandleImp *driverHandle = nullptr;
    uint32_t numDevices = 0;
    ContextExt *contextExt = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<ContextImp>);

} // namespace L0
