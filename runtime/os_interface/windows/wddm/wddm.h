/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
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
class WddmMemoryManager;
class Gdi;
class Gmm;
class LinearStream;
class GmmPageTableMngr;
struct FeatureTable;
struct WorkaroundTable;
struct KmDafListener;

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
    MOCKABLE_VIRTUAL bool createContext();
    MOCKABLE_VIRTUAL bool freeGpuVirtualAddres(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size);
    MOCKABLE_VIRTUAL NTSTATUS createAllocation(WddmAllocation *alloc);
    MOCKABLE_VIRTUAL bool createAllocation64k(WddmAllocation *alloc);
    MOCKABLE_VIRTUAL NTSTATUS createAllocationsAndMapGpuVa(OsHandleStorage &osHandles);
    MOCKABLE_VIRTUAL bool destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue, D3DKMT_HANDLE resourceHandle);
    MOCKABLE_VIRTUAL bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc);
    bool openNTHandle(HANDLE handle, WddmAllocation *alloc);
    MOCKABLE_VIRTUAL void *lockResource(WddmAllocation *wddmAllocation);
    MOCKABLE_VIRTUAL void unlockResource(WddmAllocation *wddmAllocation);
    MOCKABLE_VIRTUAL void kmDafLock(WddmAllocation *wddmAllocation);
    MOCKABLE_VIRTUAL bool isKmDafEnabled() { return featureTable->ftrKmdDaf; };

    MOCKABLE_VIRTUAL bool destroyContext(D3DKMT_HANDLE context);
    MOCKABLE_VIRTUAL bool queryAdapterInfo();

    virtual bool submit(uint64_t commandBuffer, size_t size, void *commandHeader);
    MOCKABLE_VIRTUAL bool waitOnGPU();
    MOCKABLE_VIRTUAL bool waitFromCpu(uint64_t lastFenceValue);

    NTSTATUS escape(D3DKMT_ESCAPE &escapeCommand);
    void registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmMemoryManager *memoryManager);
    MOCKABLE_VIRTUAL void releaseReservedAddress(void *reservedAddress);
    MOCKABLE_VIRTUAL bool reserveValidAddressRange(size_t size, void *&reservedMem);

    MOCKABLE_VIRTUAL void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type);
    MOCKABLE_VIRTUAL int virtualFree(void *ptr, size_t size, unsigned long flags);

    template <typename GfxFamily>
    bool configureDeviceAddressSpace();

    template <typename GfxFamily>
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

    MonitoredFence &getMonitoredFence() { return monitoredFence; }

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

    D3DKMT_HANDLE getOsDeviceContext() {
        return context;
    }

    unsigned int readEnablePreemptionRegKey();
    void resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress);

  protected:
    bool initialized;
    std::unique_ptr<Gdi> gdi;
    D3DKMT_HANDLE adapter;
    D3DKMT_HANDLE context;
    D3DKMT_HANDLE device;
    D3DKMT_HANDLE pagingQueue;
    D3DKMT_HANDLE pagingQueueSyncObject;

    uint64_t *pagingFenceAddress;
    std::atomic<std::uint64_t> currentPagingFenceValue;

    MonitoredFence monitoredFence;

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

    unsigned long hwContextId;
    LUID adapterLuid;
    void *trimCallbackHandle;
    uintptr_t maximumApplicationAddress;
    GPUNODE_ORDINAL node;
    PreemptionMode preemptionMode;
    std::unique_ptr<GmmMemory> gmmMemory;
    uintptr_t minAddress;

    Wddm();
    MOCKABLE_VIRTUAL bool mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, bool allocation32bit, bool use64kbPages, bool useHeap1);
    MOCKABLE_VIRTUAL bool openAdapter();
    bool createDevice();
    bool createPagingQueue();
    bool destroyPagingQueue();
    bool destroyDevice();
    bool closeAdapter();
    void getDeviceState();
    void handleCompletion();

    static CreateDXGIFactoryFcn createDxgiFactory;
    static GetSystemInfoFcn getSystemInfo;
    static VirtualFreeFcn virtualFreeFnc;
    static VirtualAllocFcn virtualAllocFnc;

    std::unique_ptr<GmmPageTableMngr> pageTableManager;

    std::unique_ptr<KmDafListener> kmDafListener;
    std::unique_ptr<WddmInterface> wddmInterface;
};
} // namespace OCLRT
