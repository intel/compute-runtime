/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/preemption_mode.h"
#include "core/gmm_helper/gmm_lib.h"
#include "core/helpers/debug_helpers.h"
#include "core/memory_manager/gfx_partition.h"
#include "core/os_interface/os_context.h"
#include "core/utilities/spinlock.h"

#include "sku_info.h"

#include <memory>
#include <mutex>

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

struct WddmSubmitArguments {
    MonitoredFence *monitorFence;
    D3DKMT_HANDLE contextHandle;
    D3DKMT_HANDLE hwQueueHandle;
};

enum class WddmVersion : uint32_t {
    WDDM_2_0 = 0,
    WDDM_2_3
};

class Wddm {
  public:
    typedef HRESULT(WINAPI *CreateDXGIFactoryFcn)(REFIID riid, void **ppFactory);
    typedef void(WINAPI *GetSystemInfoFcn)(SYSTEM_INFO *pSystemInfo);
    typedef BOOL(WINAPI *VirtualFreeFcn)(LPVOID ptr, SIZE_T size, DWORD flags);
    typedef LPVOID(WINAPI *VirtualAllocFcn)(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type);

    virtual ~Wddm();

    static Wddm *createWddm(RootDeviceEnvironment &rootDeviceEnvironment);
    bool init(HardwareInfo &outHardwareInfo);

    MOCKABLE_VIRTUAL bool evict(const D3DKMT_HANDLE *handleList, uint32_t numOfHandles, uint64_t &sizeToTrim);
    MOCKABLE_VIRTUAL bool makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim);
    MOCKABLE_VIRTUAL bool mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr);
    bool mapGpuVirtualAddress(AllocationStorageData *allocationStorageData);
    MOCKABLE_VIRTUAL D3DGPU_VIRTUAL_ADDRESS reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_SIZE_T size);
    MOCKABLE_VIRTUAL bool createContext(OsContextWin &osContext);
    MOCKABLE_VIRTUAL void applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData, OsContextWin &osContext);
    MOCKABLE_VIRTUAL bool freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size);
    MOCKABLE_VIRTUAL NTSTATUS createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, uint32_t shareable);
    MOCKABLE_VIRTUAL bool createAllocation64k(const Gmm *gmm, D3DKMT_HANDLE &outHandle);
    MOCKABLE_VIRTUAL NTSTATUS createAllocationsAndMapGpuVa(OsHandleStorage &osHandles);
    MOCKABLE_VIRTUAL bool destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);
    MOCKABLE_VIRTUAL bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc);
    bool openNTHandle(HANDLE handle, WddmAllocation *alloc);
    MOCKABLE_VIRTUAL void *lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock);
    MOCKABLE_VIRTUAL void unlockResource(const D3DKMT_HANDLE &handle);
    MOCKABLE_VIRTUAL void kmDafLock(D3DKMT_HANDLE handle);
    MOCKABLE_VIRTUAL bool isKmDafEnabled() const { return featureTable->ftrKmdDaf; }

    MOCKABLE_VIRTUAL bool destroyContext(D3DKMT_HANDLE context);
    MOCKABLE_VIRTUAL bool queryAdapterInfo();

    MOCKABLE_VIRTUAL bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments);
    MOCKABLE_VIRTUAL bool waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence);

    NTSTATUS escape(D3DKMT_ESCAPE &escapeCommand);
    MOCKABLE_VIRTUAL VOID *registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController);
    void unregisterTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, VOID *trimCallbackHandle);
    MOCKABLE_VIRTUAL void releaseReservedAddress(void *reservedAddress);
    MOCKABLE_VIRTUAL bool reserveValidAddressRange(size_t size, void *&reservedMem);

    MOCKABLE_VIRTUAL void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type);
    MOCKABLE_VIRTUAL int virtualFree(void *ptr, size_t size, unsigned long flags);

    bool configureDeviceAddressSpace();

    GT_SYSTEM_INFO *getGtSysInfo() const {
        DEBUG_BREAK_IF(!gtSystemInfo);
        return gtSystemInfo.get();
    }

    const GMM_GFX_PARTITIONING &getGfxPartition() const {
        return gfxPartition;
    }

    void initGfxPartition(GfxPartition &outGfxPartition) const;

    const std::string &getDeviceRegistryPath() const {
        return deviceRegistryPath;
    }

    uint64_t getSystemSharedMemory() const;
    uint64_t getDedicatedVideoMemory() const;

    uint64_t getMaxApplicationAddress() const;

    D3DKMT_HANDLE getAdapter() const { return adapter; }
    D3DKMT_HANDLE getDevice() const { return device; }
    D3DKMT_HANDLE getPagingQueue() const { return pagingQueue; }
    D3DKMT_HANDLE getPagingQueueSyncObject() const { return pagingQueueSyncObject; }
    Gdi *getGdi() const { return gdi.get(); }

    PFND3DKMT_ESCAPE getEscapeHandle() const;

    uint32_t getHwContextId() const {
        return static_cast<uint32_t>(hwContextId);
    }

    std::unique_ptr<SettingsReader> registryReader;

    uintptr_t getWddmMinAddress() const {
        return this->minAddress;
    }
    WddmInterface *getWddmInterface() const {
        return wddmInterface.get();
    }

    unsigned int readEnablePreemptionRegKey();
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
    void waitOnPagingFenceFromCpu();

    void setGmmInputArg(void *args);

    WddmVersion getWddmVersion();

  protected:
    std::unique_ptr<Gdi> gdi;
    D3DKMT_HANDLE adapter = 0;
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
    GMM_GFX_PARTITIONING gfxPartition;
    ADAPTER_BDF adapterBDF;
    uint64_t systemSharedMemory = 0;
    uint64_t dedicatedVideoMemory = 0;
    uint32_t maxRenderFrequency = 0;
    bool instrumentationEnabled = false;
    std::string deviceRegistryPath;
    RootDeviceEnvironment &rootDeviceEnvironment;

    unsigned long hwContextId = 0;
    LUID adapterLuid;
    uintptr_t maximumApplicationAddress = 0;
    std::unique_ptr<GmmMemory> gmmMemory;
    uintptr_t minAddress = 0;

    Wddm(RootDeviceEnvironment &rootDeviceEnvironment);
    MOCKABLE_VIRTUAL bool openAdapter();
    MOCKABLE_VIRTUAL bool waitOnGPU(D3DKMT_HANDLE context);
    bool createDevice(PreemptionMode preemptionMode);
    bool createPagingQueue();
    bool destroyPagingQueue();
    bool destroyDevice();
    bool closeAdapter();
    void getDeviceState();
    void handleCompletion(OsContextWin &osContext);

    static CreateDXGIFactoryFcn createDxgiFactory;
    static GetSystemInfoFcn getSystemInfo;
    static VirtualFreeFcn virtualFreeFnc;
    static VirtualAllocFcn virtualAllocFnc;

    std::unique_ptr<KmDafListener> kmDafListener;
    std::unique_ptr<WddmInterface> wddmInterface;
    std::unique_ptr<WddmResidentAllocationsContainer> temporaryResources;
};
} // namespace NEO
