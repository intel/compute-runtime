/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct _ze_driver_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_DRIVER> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_driver_handle_t>);

namespace NEO {
class Device;
class MemoryManager;
class OsLibrary;
class SVMAllocsManager;
class GraphicsAllocation;
class StagingBufferManager;
class IpcSocketServer;
struct SvmAllocationData;
enum class AllocationType;

struct IpcSocketServerDeleter {
    void operator()(IpcSocketServer *ptr) const;
};
} // namespace NEO

namespace L0 {
struct Device;
struct L0EnvVariables;
class HostPointerManager;
struct FabricVertex;
struct FabricEdge;
struct Image;
class ExternalSemaphoreController;

struct BaseDriver : _ze_driver_handle_t {
    virtual ~BaseDriver() = default;
    virtual ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) = 0;
    static BaseDriver *fromHandle(ze_driver_handle_t handle) { return static_cast<BaseDriver *>(handle); }
};

class DriverHandle : public BaseDriver, public NEO::NonCopyableAndNonMovableClass {
  public:
    ~DriverHandle() override;
    DriverHandle();

    static constexpr uint32_t initialDriverVersionValue = 0x01030000;

    MOCKABLE_VIRTUAL ze_result_t createContext(const ze_context_desc_t *desc,
                                               uint32_t numDevices,
                                               ze_device_handle_t *phDevices,
                                               ze_context_handle_t *phContext);
    MOCKABLE_VIRTUAL ze_result_t getDevice(uint32_t *pCount, ze_device_handle_t *phDevices);
    MOCKABLE_VIRTUAL ze_result_t getProperties(ze_driver_properties_t *properties);
    MOCKABLE_VIRTUAL ze_result_t getApiVersion(ze_api_version_t *version);
    MOCKABLE_VIRTUAL ze_result_t getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties);
    ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) override;
    MOCKABLE_VIRTUAL ze_result_t getExtensionProperties(uint32_t *pCount,
                                                        ze_driver_extension_properties_t *pExtensionProperties);

    MOCKABLE_VIRTUAL NEO::MemoryManager *getMemoryManager();
    MOCKABLE_VIRTUAL void setMemoryManager(NEO::MemoryManager *memoryManager);
    MOCKABLE_VIRTUAL void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData);
    MOCKABLE_VIRTUAL void *importFdHandles(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, const std::vector<NEO::osHandle> &handles, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData);
    MOCKABLE_VIRTUAL void *importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId);
    MOCKABLE_VIRTUAL NEO::SVMAllocsManager *getSvmAllocsManager();
    MOCKABLE_VIRTUAL NEO::StagingBufferManager *getStagingBufferManager();
    ze_result_t initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices);
    MOCKABLE_VIRTUAL bool findAllocationDataForRange(const void *buffer,
                                                     size_t size,
                                                     NEO::SvmAllocationData *&allocData);
    MOCKABLE_VIRTUAL std::vector<NEO::SvmAllocationData *> findAllocationsWithinRange(const void *buffer,
                                                                                      size_t size,
                                                                                      bool *allocationRangeCovered);

    MOCKABLE_VIRTUAL ze_result_t sysmanEventsListen(uint32_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                                    uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents);

    MOCKABLE_VIRTUAL ze_result_t sysmanEventsListenEx(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                                      uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents);

    MOCKABLE_VIRTUAL ze_result_t importExternalPointer(void *ptr, size_t size);
    MOCKABLE_VIRTUAL ze_result_t releaseImportedPointer(void *ptr);
    MOCKABLE_VIRTUAL ze_result_t getHostPointerBaseAddress(void *ptr, void **baseAddress);

    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *findHostPointerAllocation(void *ptr, size_t size, uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *getDriverSystemMemoryAllocation(void *ptr,
                                                                              size_t size,
                                                                              uint32_t rootDeviceIndex,
                                                                              uintptr_t *gpuAddress);
    ze_result_t getPeerImage(Device *device, L0::Image *image, L0::Image **peerImage);
    NEO::GraphicsAllocation *getPeerAllocation(Device *device,
                                               NEO::SvmAllocationData *allocData,
                                               void *basePtr,
                                               uintptr_t *peerGpuAddress,
                                               NEO::SvmAllocationData **peerAllocData);

    NEO::GraphicsAllocation *getCounterPeerAllocation(Device *device, NEO::GraphicsAllocation &graphicsAllocation);
    void initializeVertexes();
    MOCKABLE_VIRTUAL ze_result_t fabricVertexGetExp(uint32_t *pCount, ze_fabric_vertex_handle_t *phDevices);
    void createHostPointerManager();

    bool isRemoteImageNeeded(Image *image, Device *device);
    bool isRemoteResourceNeeded(NEO::GraphicsAllocation *alloc, NEO::SvmAllocationData *allocData, Device *device);
    ze_result_t fabricEdgeGetExp(ze_fabric_vertex_handle_t hVertexA, ze_fabric_vertex_handle_t hVertexB,
                                 uint32_t *pCount, ze_fabric_edge_handle_t *phEdges);
    MOCKABLE_VIRTUAL uint32_t getEventMaxPacketCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const;
    MOCKABLE_VIRTUAL uint32_t getEventMaxKernelCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const;

    MOCKABLE_VIRTUAL ze_result_t loadRTASLibrary();
    MOCKABLE_VIRTUAL ze_result_t createRTASBuilder(const ze_rtas_builder_exp_desc_t *desc, ze_rtas_builder_exp_handle_t *phBuilder);
    MOCKABLE_VIRTUAL ze_result_t createRTASBuilderExt(const ze_rtas_builder_ext_desc_t *desc, ze_rtas_builder_ext_handle_t *phBuilder);
    MOCKABLE_VIRTUAL ze_result_t createRTASParallelOperation(ze_rtas_parallel_operation_exp_handle_t *phParallelOperation);
    MOCKABLE_VIRTUAL ze_result_t createRTASParallelOperationExt(ze_rtas_parallel_operation_ext_handle_t *phParallelOperation);
    MOCKABLE_VIRTUAL ze_result_t formatRTASCompatibilityCheck(ze_rtas_format_exp_t rtasFormatA, ze_rtas_format_exp_t rtasFormatB);
    MOCKABLE_VIRTUAL ze_result_t formatRTASCompatibilityCheckExt(ze_rtas_format_ext_t rtasFormatA, ze_rtas_format_ext_t rtasFormatB);

    std::map<uint64_t, IpcHandleTracking *> &getIPCHandleMap() { return this->ipcHandles; };
    [[nodiscard]] std::unique_lock<std::mutex> lockIPCHandleMap() { return std::unique_lock<std::mutex>(this->ipcHandleMapMutex); };
    void initHostUsmAllocPool();
    void initHostUsmAllocPoolOnce();
    void initDeviceUsmAllocPool(NEO::Device &device, bool multiDevice);
    void initDeviceUsmAllocPoolOnce();
    void initUsmPooling();
    NEO::UsmMemAllocPool::CustomCleanupFn getPoolCleanupFn();
    NEO::UsmMemAllocPool *getHostUsmPoolOwningPtr(const void *ptr);

    MOCKABLE_VIRTUAL bool initializeIpcSocketServer();
    void shutdownIpcSocketServer();
    MOCKABLE_VIRTUAL bool registerIpcHandleWithServer(uint64_t handleId, int fd);
    bool unregisterIpcHandleWithServer(uint64_t handleId);
    std::string getIpcSocketServerPath();

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
    std::unique_ptr<NEO::UsmMemAllocPool> usmHostMemAllocPool;
    std::unique_ptr<NEO::UsmMemAllocPoolsManager> usmHostMemAllocPoolManager;
    ze_context_handle_t defaultContext = nullptr;
    std::unique_ptr<NEO::StagingBufferManager> stagingBufferManager;

    std::unique_ptr<NEO::OsLibrary> rtasLibraryHandle;
    bool rtasLibraryUnavailable = false;

    std::unique_ptr<ExternalSemaphoreController> externalSemaphoreController;
    std::mutex externalSemaphoreControllerMutex;

    uint32_t numDevices = 0;

    std::map<uint64_t, IpcHandleTracking *> ipcHandles;
    std::mutex ipcHandleMapMutex;

    std::unique_ptr<NEO::IpcSocketServer, NEO::IpcSocketServerDeleter> ipcSocketServer;
    std::mutex ipcSocketServerMutex;

    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitfields;
    void updateRootDeviceBitFields(std::unique_ptr<NEO::Device> &neoDevice);

    // Environment Variables
    NEO::DebuggingMode enableProgramDebugging = NEO::DebuggingMode::disabled;
    bool enableSysman = false;
    bool enablePciIdDeviceOrder = false;
    bool lazyInitUsmPools = false;
    std::once_flag hostUsmPoolOnceFlag;
    std::once_flag deviceUsmPoolOnceFlag;
    uint8_t powerHint = 0;

    // Error messages per thread, variable initialized / destroyed per thread,
    // not based on the lifetime of the object of a class.
    std::unordered_map<std::thread::id, std::string> errorDescs;
    std::mutex errorDescsMutex;
    int setErrorDescription(const std::string &str);
    ze_result_t getErrorDescription(const char **ppString);
    ze_result_t clearErrorDescription();

    ze_context_handle_t getDefaultContext() const {
        return defaultContext;
    }
    void setupDevicesToExpose();

    static DriverHandle *fromHandle(ze_driver_handle_t handle) { return static_cast<DriverHandle *>(handle); }
    inline ze_driver_handle_t toHandle() { return this; }

    static DriverHandle *create(std::vector<std::unique_ptr<NEO::Device>> devices, const L0EnvVariables &envVariables, ze_result_t *returnValue);

  protected:
    NEO::GraphicsAllocation *getPeerAllocation(Device *device,
                                               NEO::SVMAllocsManager::MapBasedAllocationTracker &storage,
                                               NEO::SvmAllocationData *allocData,
                                               void *basePtr,
                                               uintptr_t *peerGpuAddress,
                                               NEO::SvmAllocationData **peerAllocData);
};

static_assert(NEO::NonCopyableAndNonMovable<DriverHandle>);

} // namespace L0
