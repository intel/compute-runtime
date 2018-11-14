/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/os_interface/windows/windows_defs.h"
#include "runtime/os_interface/windows/wddm/wddm_interface.h"
#include "umKmInc/sharedata.h"
#include "runtime/helpers/debug_helpers.h"
#include <d3d9types.h>
#include <d3dkmthk.h>
#include "gfxEscape.h"
#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/utilities/debug_settings_reader.h"
#include "runtime/gmm_helper/gmm_lib.h"
#include "runtime/helpers/hw_info.h"
#include "gmm_memory.h"
#include <memory>
#include <atomic>

namespace OCLRT {

class WddmAllocation;
class Gdi;
class Gmm;
class LinearStream;
class GmmPageTableMngr;
struct FeatureTable;
struct WorkaroundTable;
struct KmDafListener;

using OsContextWin = OsContext::OsContextImpl;

enum class WddmInterfaceVersion {
    Wddm20 = 20,
    Wddm23 = 23,
};

class Wddm {
  public:
    typedef HRESULT(WINAPI *CreateDXGIFactoryFcn)(REFIID riid, void **ppFactory);
    typedef void(WINAPI *GetSystemInfoFcn)(SYSTEM_INFO *pSystemInfo);
    typedef BOOL(WINAPI *VirtualFreeFcn)(LPVOID ptr, SIZE_T size, DWORD flags);
    typedef LPVOID(WINAPI *VirtualAllocFcn)(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type);

    virtual ~Wddm();

    static Wddm *createWddm();
    bool enumAdapters(HardwareInfo &outHardwareInfo);

    MOCKABLE_VIRTUAL bool evict(D3DKMT_HANDLE *handleList, uint32_t numOfHandles, uint64_t &sizeToTrim);
    MOCKABLE_VIRTUAL bool makeResident(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim);
    bool mapGpuVirtualAddress(WddmAllocation *allocation, void *cpuPtr, bool allocation32bit, bool use64kbPages, bool useHeap1);
    bool mapGpuVirtualAddress(AllocationStorageData *allocationStorageData, bool allocation32bit, bool use64kbPages);
    MOCKABLE_VIRTUAL bool createContext(D3DKMT_HANDLE &context);
    MOCKABLE_VIRTUAL void applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData);
    MOCKABLE_VIRTUAL bool freeGpuVirtualAddres(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size);
    MOCKABLE_VIRTUAL NTSTATUS createAllocation(WddmAllocation *alloc);
    MOCKABLE_VIRTUAL bool createAllocation64k(WddmAllocation *alloc);
    MOCKABLE_VIRTUAL NTSTATUS createAllocationsAndMapGpuVa(OsHandleStorage &osHandles);
    MOCKABLE_VIRTUAL bool destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);
    MOCKABLE_VIRTUAL bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc);
    bool openNTHandle(HANDLE handle, WddmAllocation *alloc);
    MOCKABLE_VIRTUAL void *lockResource(WddmAllocation *wddmAllocation);
    MOCKABLE_VIRTUAL void unlockResource(WddmAllocation *wddmAllocation);
    MOCKABLE_VIRTUAL void kmDafLock(WddmAllocation *wddmAllocation);
    MOCKABLE_VIRTUAL bool isKmDafEnabled() { return featureTable->ftrKmdDaf; };

    MOCKABLE_VIRTUAL bool destroyContext(D3DKMT_HANDLE context);
    MOCKABLE_VIRTUAL bool queryAdapterInfo();

    MOCKABLE_VIRTUAL bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext);
    MOCKABLE_VIRTUAL bool waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence);

    NTSTATUS escape(D3DKMT_ESCAPE &escapeCommand);
    MOCKABLE_VIRTUAL VOID *registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController);
    void unregisterTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, VOID *trimCallbackHandle);
    MOCKABLE_VIRTUAL void releaseReservedAddress(void *reservedAddress);
    MOCKABLE_VIRTUAL bool reserveValidAddressRange(size_t size, void *&reservedMem);

    MOCKABLE_VIRTUAL void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type);
    MOCKABLE_VIRTUAL int virtualFree(void *ptr, size_t size, unsigned long flags);

    bool configureDeviceAddressSpace();

    bool init();

    bool isInitialized() const {
        return initialized;
    }

    GT_SYSTEM_INFO *getGtSysInfo() const {
        DEBUG_BREAK_IF(!gtSystemInfo);
        return gtSystemInfo.get();
    }

    const GMM_GFX_PARTITIONING &getGfxPartition() const {
        return gfxPartition;
    }

    const std::string &getDeviceRegistryPath() const {
        return deviceRegistryPath;
    }

    uint64_t getSystemSharedMemory() const;

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

    uint64_t getHeap32Base();
    uint64_t getHeap32Size();

    std::unique_ptr<SettingsReader> registryReader;

    void setNode(GPUNODE_ORDINAL node) {
        this->node = node;
    }

    void setPreemptionMode(PreemptionMode mode) {
        this->preemptionMode = mode;
    }

    GmmPageTableMngr *getPageTableManager() const { return pageTableManager.get(); }
    void resetPageTableManager(GmmPageTableMngr *newPageTableManager);
    bool updateAuxTable(D3DGPU_VIRTUAL_ADDRESS gpuVa, Gmm *gmm, bool map);

    uintptr_t getWddmMinAddress() const {
        return this->minAddress;
    }
    WddmInterface *getWddmInterface() const {
        return wddmInterface.get();
    }
    PreemptionMode getPreemptionMode() const {
        return preemptionMode;
    }

    unsigned int readEnablePreemptionRegKey();

  protected:
    bool initialized = false;
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
    std::unique_ptr<WorkaroundTable> waTable;
    GMM_GFX_PARTITIONING gfxPartition;
    uint64_t systemSharedMemory = 0;
    uint32_t maxRenderFrequency = 0;
    bool instrumentationEnabled = false;
    std::string deviceRegistryPath;

    unsigned long hwContextId = 0;
    LUID adapterLuid;
    uintptr_t maximumApplicationAddress = 0;
    GPUNODE_ORDINAL node = GPUNODE_3D;
    PreemptionMode preemptionMode = PreemptionMode::Disabled;
    std::unique_ptr<GmmMemory> gmmMemory;
    uintptr_t minAddress = 0;

    Wddm();
    MOCKABLE_VIRTUAL bool mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, bool allocation32bit, bool use64kbPages, bool useHeap1);
    MOCKABLE_VIRTUAL bool openAdapter();
    MOCKABLE_VIRTUAL bool waitOnGPU(D3DKMT_HANDLE context);
    bool createDevice();
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

    std::unique_ptr<GmmPageTableMngr> pageTableManager;

    std::unique_ptr<KmDafListener> kmDafListener;
    std::unique_ptr<WddmInterface> wddmInterface;
};
} // namespace OCLRT
