/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debugger/debugger.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/sys_calls_common.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/ze_intel_gpu.h"

#include <map>
#include <mutex>
#include <unordered_map>

namespace L0 {
class HostPointerManager;
struct FabricVertex;
struct FabricEdge;
struct Image;
class ExternalSemaphoreController;
struct DriverHandleImp : public DriverHandle {
    ~DriverHandleImp() override;
    DriverHandleImp();

    static constexpr uint32_t initialDriverVersionValue = 0x01030000;

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
    MOCKABLE_VIRTUAL void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData);
    MOCKABLE_VIRTUAL void *importFdHandles(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, const std::vector<NEO::osHandle> &handles, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData);
    MOCKABLE_VIRTUAL void *importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId);
    NEO::SVMAllocsManager *getSvmAllocsManager() override;
    NEO::StagingBufferManager *getStagingBufferManager() override;
    ze_result_t initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices);
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData *&allocData) override;
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
    ze_result_t getPeerImage(Device *device, L0::Image *image, L0::Image **peerImage);
    NEO::GraphicsAllocation *getPeerAllocation(Device *device,
                                               NEO::SvmAllocationData *allocData,
                                               void *basePtr,
                                               uintptr_t *peerGpuAddress,
                                               NEO::SvmAllocationData **peerAllocData);

    NEO::GraphicsAllocation *getCounterPeerAllocation(Device *device, NEO::GraphicsAllocation &graphicsAllocation);
    void initializeVertexes();
    ze_result_t fabricVertexGetExp(uint32_t *pCount, ze_fabric_vertex_handle_t *phDevices) override;
    void createHostPointerManager();

    bool isRemoteImageNeeded(Image *image, Device *device);
    bool isRemoteResourceNeeded(void *ptr,
                                NEO::GraphicsAllocation *alloc,
                                NEO::SvmAllocationData *allocData,
                                Device *device);
    ze_result_t fabricEdgeGetExp(ze_fabric_vertex_handle_t hVertexA, ze_fabric_vertex_handle_t hVertexB,
                                 uint32_t *pCount, ze_fabric_edge_handle_t *phEdges);
    uint32_t getEventMaxPacketCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const override;
    uint32_t getEventMaxKernelCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const override;

    ze_result_t loadRTASLibrary() override;
    ze_result_t createRTASBuilder(const ze_rtas_builder_exp_desc_t *desc, ze_rtas_builder_exp_handle_t *phBuilder) override;
    ze_result_t createRTASBuilderExt(const ze_rtas_builder_ext_desc_t *desc, ze_rtas_builder_ext_handle_t *phBuilder) override;
    ze_result_t createRTASParallelOperation(ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) override;
    ze_result_t createRTASParallelOperationExt(ze_rtas_parallel_operation_ext_handle_t *phParallelOperation) override;
    ze_result_t formatRTASCompatibilityCheck(ze_rtas_format_exp_t rtasFormatA, ze_rtas_format_exp_t rtasFormatB) override;
    ze_result_t formatRTASCompatibilityCheckExt(ze_rtas_format_ext_t rtasFormatA, ze_rtas_format_ext_t rtasFormatB) override;

    std::map<uint64_t, IpcHandleTracking *> &getIPCHandleMap() { return this->ipcHandles; };
    [[nodiscard]] std::unique_lock<std::mutex> lockIPCHandleMap() { return std::unique_lock<std::mutex>(this->ipcHandleMapMutex); };
    void initHostUsmAllocPool();
    void initDeviceUsmAllocPool(NEO::Device &device, bool multiDevice);

    std::unique_ptr<HostPointerManager> hostPointerManager;

    std::mutex sharedMakeResidentAllocationsLock;
    std::map<void *, NEO::GraphicsAllocation *> sharedMakeResidentAllocations;

    std::vector<Device *> devices;
    std::vector<ze_device_handle_t> devicesToExpose;
    std::vector<FabricVertex *> fabricVertices;
    std::vector<FabricEdge *> fabricEdges;
    std::vector<FabricEdge *> fabricIndirectEdges;

    std::mutex rtasLock;

    // Spec extensions
    static const std::vector<std::pair<std::string, uint32_t>> extensionsSupported;

    uint64_t uuidTimestamp = 0u;
    unsigned int pid = 0;

    NEO::MemoryManager *memoryManager = nullptr;
    NEO::SVMAllocsManager *svmAllocsManager = nullptr;
    NEO::UsmMemAllocPool usmHostMemAllocPool;
    ze_context_handle_t defaultContext = nullptr;
    std::unique_ptr<NEO::StagingBufferManager> stagingBufferManager;

    std::unique_ptr<NEO::OsLibrary> rtasLibraryHandle;
    bool rtasLibraryUnavailable = false;

    std::unique_ptr<ExternalSemaphoreController> externalSemaphoreController;
    std::mutex externalSemaphoreControllerMutex;

    uint32_t numDevices = 0;

    std::map<uint64_t, IpcHandleTracking *> ipcHandles;
    std::mutex ipcHandleMapMutex;

    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitfields;
    void updateRootDeviceBitFields(std::unique_ptr<NEO::Device> &neoDevice);

    // Environment Variables
    NEO::DebuggingMode enableProgramDebugging = NEO::DebuggingMode::disabled;
    bool enableSysman = false;
    bool enablePciIdDeviceOrder = false;
    uint8_t powerHint = 0;

    // Error messages per thread, variable initialized / destroyed per thread,
    // not based on the lifetime of the object of a class.
    std::unordered_map<std::thread::id, std::string> errorDescs;
    std::mutex errorDescsMutex;
    int setErrorDescription(const std::string &str) override;
    ze_result_t getErrorDescription(const char **ppString) override;
    ze_result_t clearErrorDescription() override;

    ze_context_handle_t getDefaultContext() const override {
        return defaultContext;
    }
    void setupDevicesToExpose();

  protected:
    NEO::GraphicsAllocation *getPeerAllocation(Device *device,
                                               NEO::SVMAllocsManager::MapBasedAllocationTracker &storage,
                                               NEO::SvmAllocationData *allocData,
                                               void *basePtr,
                                               uintptr_t *peerGpuAddress,
                                               NEO::SvmAllocationData **peerAllocData);
};

} // namespace L0
