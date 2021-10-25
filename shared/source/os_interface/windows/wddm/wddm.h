/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/wddm/wddm_defs.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/utilities/spinlock.h"

#include "sku_info.h"

#include <memory>
#include <mutex>

struct _SYSTEM_INFO;
typedef struct _SYSTEM_INFO SYSTEM_INFO;

namespace NEO {
class Gdi;
class Gmm;
class GmmMemory;
class OsContextWin;
class SettingsReader;
class WddmAllocation;
class WddmInterface;
class WddmResidencyController;
class WddmResidentAllocationsContainer;

struct AllocationStorageData;
struct HardwareInfo;
struct KmDafListener;
struct RootDeviceEnvironment;
struct MonitoredFence;
struct OsHandleStorage;

enum class HeapIndex : uint32_t;

unsigned int readEnablePreemptionRegKey();
unsigned int getPid();
bool isShutdownInProgress();

class Wddm : public DriverModel {
  public:
    static constexpr DriverModelType driverModelType = DriverModelType::WDDM;

    typedef HRESULT(WINAPI *CreateDXGIFactoryFcn)(REFIID riid, void **ppFactory);
    typedef HRESULT(WINAPI *DXCoreCreateAdapterFactoryFcn)(REFIID riid, void **ppFactory);
    typedef void(WINAPI *GetSystemInfoFcn)(SYSTEM_INFO *pSystemInfo);

    virtual ~Wddm();

    static Wddm *createWddm(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
    bool init();

    MOCKABLE_VIRTUAL bool evict(const D3DKMT_HANDLE *handleList, uint32_t numOfHandles, uint64_t &sizeToTrim);
    MOCKABLE_VIRTUAL bool makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize);
    MOCKABLE_VIRTUAL bool mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr);
    bool mapGpuVirtualAddress(AllocationStorageData *allocationStorageData);
    MOCKABLE_VIRTUAL D3DGPU_VIRTUAL_ADDRESS reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_SIZE_T size);
    MOCKABLE_VIRTUAL bool createContext(OsContextWin &osContext);
    MOCKABLE_VIRTUAL void applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData, OsContextWin &osContext, const HardwareInfo &hwInfo);
    MOCKABLE_VIRTUAL void applyAdditionalMapGPUVAFields(D3DDDI_MAPGPUVIRTUALADDRESS &MapGPUVA, Gmm *gmm);
    MOCKABLE_VIRTUAL bool freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size);
    MOCKABLE_VIRTUAL NTSTATUS createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, uint64_t *outSharedHandle);
    MOCKABLE_VIRTUAL bool createAllocation(const Gmm *gmm, D3DKMT_HANDLE &outHandle);
    MOCKABLE_VIRTUAL NTSTATUS createAllocationsAndMapGpuVa(OsHandleStorage &osHandles);
    MOCKABLE_VIRTUAL bool destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);
    MOCKABLE_VIRTUAL bool verifySharedHandle(D3DKMT_HANDLE osHandle);
    MOCKABLE_VIRTUAL bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc);
    MOCKABLE_VIRTUAL bool verifyNTHandle(HANDLE handle);
    bool openNTHandle(HANDLE handle, WddmAllocation *alloc);
    MOCKABLE_VIRTUAL void *lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size);
    MOCKABLE_VIRTUAL void unlockResource(const D3DKMT_HANDLE &handle);
    MOCKABLE_VIRTUAL void kmDafLock(D3DKMT_HANDLE handle);
    MOCKABLE_VIRTUAL bool isKmDafEnabled() const { return featureTable->ftrKmdDaf; }

    MOCKABLE_VIRTUAL bool setAllocationPriority(const D3DKMT_HANDLE *handles, uint32_t allocationCount, uint32_t priority);

    MOCKABLE_VIRTUAL bool destroyContext(D3DKMT_HANDLE context);
    MOCKABLE_VIRTUAL bool queryAdapterInfo();
    MOCKABLE_VIRTUAL NTSTATUS createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle);

    MOCKABLE_VIRTUAL bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments);
    MOCKABLE_VIRTUAL bool waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence);

    NTSTATUS escape(D3DKMT_ESCAPE &escapeCommand);
    MOCKABLE_VIRTUAL VOID *registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController);
    void unregisterTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, VOID *trimCallbackHandle);
    MOCKABLE_VIRTUAL void releaseReservedAddress(void *reservedAddress);
    MOCKABLE_VIRTUAL bool reserveValidAddressRange(size_t size, void *&reservedMem);

    MOCKABLE_VIRTUAL void *virtualAlloc(void *inPtr, size_t size, bool topDownHint);
    MOCKABLE_VIRTUAL void virtualFree(void *ptr, size_t size);

    MOCKABLE_VIRTUAL bool isShutdownInProgress();

    bool configureDeviceAddressSpace();
    const FeatureTable &getFeatureTable() const {
        return *featureTable;
    }

    GT_SYSTEM_INFO *getGtSysInfo() const {
        DEBUG_BREAK_IF(!gtSystemInfo);
        return gtSystemInfo.get();
    }

    const GMM_GFX_PARTITIONING &getGfxPartition() const {
        return gfxPartition;
    }

    void initGfxPartition(GfxPartition &outGfxPartition, uint32_t rootDeviceIndex, size_t numRootDevices, bool useFrontWindowPool) const;

    const std::string &getDeviceRegistryPath() const {
        return deviceRegistryPath;
    }

    uint64_t getSystemSharedMemory() const;
    uint64_t getDedicatedVideoMemory() const;

    uint64_t getMaxApplicationAddress() const;

    HwDeviceIdWddm *getHwDeviceId() const {
        return hwDeviceId.get();
    }
    D3DKMT_HANDLE getAdapter() const { return hwDeviceId->getAdapter(); }
    D3DKMT_HANDLE getDeviceHandle() const override { return device; }
    D3DKMT_HANDLE getPagingQueue() const { return pagingQueue; }
    D3DKMT_HANDLE getPagingQueueSyncObject() const { return pagingQueueSyncObject; }
    Gdi *getGdi() const { return hwDeviceId->getGdi(); }
    MOCKABLE_VIRTUAL bool verifyAdapterLuid(LUID adapterLuid) const;
    LUID getAdapterLuid() const;

    PFND3DKMT_ESCAPE getEscapeHandle() const;

    uint32_t getHwContextId() const {
        return static_cast<uint32_t>(hwContextId);
    }

    uintptr_t getWddmMinAddress() const {
        return this->minAddress;
    }
    WddmInterface *getWddmInterface() const {
        return wddmInterface.get();
    }

    unsigned int getEnablePreemptionRegValue();
    MOCKABLE_VIRTUAL uint64_t *getPagingFenceAddress() {
        return pagingFenceAddress;
    }
    WddmResidentAllocationsContainer *getTemporaryResourcesContainer() {
        return temporaryResources.get();
    }
    void updatePagingFenceValue(uint64_t newPagingFenceValue);
    GmmMemory *getGmmMemory() const {
        return gmmMemory.get();
    }
    MOCKABLE_VIRTUAL void waitOnPagingFenceFromCpu();

    void setGmmInputArgs(void *args) override;

    WddmVersion getWddmVersion();
    static CreateDXGIFactoryFcn createDxgiFactory;
    static DXCoreCreateAdapterFactoryFcn dXCoreCreateAdapterFactory;

    uint32_t getRequestedEUCount() const;

    WddmResidencyLogger *getResidencyLogger() {
        return residencyLogger.get();
    }

    const RootDeviceEnvironment &getRootDeviceEnvironment() const { return rootDeviceEnvironment; }

    uint32_t getTimestampFrequency() const { return timestampFrequency; }

    PhysicalDevicePciBusInfo getPciBusInfo() const override;

    size_t getMaxMemAllocSize() const override;

    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevices(ExecutionEnvironment &executionEnvironment);

    ADAPTER_BDF getAdapterBDF() const {
        return adapterBDF;
    }

  protected:
    std::unique_ptr<HwDeviceIdWddm> hwDeviceId;
    D3DKMT_HANDLE device = 0;
    D3DKMT_HANDLE pagingQueue = 0;
    D3DKMT_HANDLE pagingQueueSyncObject = 0;

    uint64_t *pagingFenceAddress = nullptr;
    std::atomic<std::uint64_t> currentPagingFenceValue{0};

    // Adapter information
    std::unique_ptr<PLATFORM> gfxPlatform;
    std::unique_ptr<GT_SYSTEM_INFO> gtSystemInfo;
    std::unique_ptr<FeatureTable> featureTable;
    std::unique_ptr<WorkaroundTable> workaroundTable;
    GMM_GFX_PARTITIONING gfxPartition{};
    ADAPTER_BDF adapterBDF{};
    uint64_t systemSharedMemory = 0;
    uint64_t dedicatedVideoMemory = 0;
    uint32_t maxRenderFrequency = 0;
    uint32_t timestampFrequency = 0u;
    bool instrumentationEnabled = false;
    std::string deviceRegistryPath;
    RootDeviceEnvironment &rootDeviceEnvironment;
    unsigned int enablePreemptionRegValue = 1;

    unsigned long hwContextId = 0;
    uintptr_t maximumApplicationAddress = 0;
    std::unique_ptr<GmmMemory> gmmMemory;
    uintptr_t minAddress = 0;

    Wddm(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
    MOCKABLE_VIRTUAL bool waitOnGPU(D3DKMT_HANDLE context);
    bool createDevice(PreemptionMode preemptionMode);
    bool createPagingQueue();
    bool destroyPagingQueue();
    bool destroyDevice();
    void getDeviceState();
    MOCKABLE_VIRTUAL void createPagingFenceLogger();

    static GetSystemInfoFcn getSystemInfo;

    std::unique_ptr<KmDafListener> kmDafListener;
    std::unique_ptr<WddmInterface> wddmInterface;
    std::unique_ptr<WddmResidentAllocationsContainer> temporaryResources;
    std::unique_ptr<WddmResidencyLogger> residencyLogger;
    std::unique_ptr<OSMemory> osMemory;
};
} // namespace NEO
