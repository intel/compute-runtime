/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/gmm_memory.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/gmm_helper/page_table_mngr.h"
#include "runtime/os_interface/windows/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/registry_reader.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/wddm_helper.h"
#include "runtime/command_stream/linear_stream.h"
#include <dxgi.h>
#include <ntstatus.h>
#include "CL/cl.h"

namespace OCLRT {
extern Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory();
extern Wddm::GetSystemInfoFcn getGetSystemInfo();

class WddmMemoryManager;

Wddm::CreateDXGIFactoryFcn Wddm::createDxgiFactory = getCreateDxgiFactory();
Wddm::GetSystemInfoFcn Wddm::getSystemInfo = getGetSystemInfo();

Wddm::Wddm(Gdi *gdi) : initialized(false),
                       gdiAllocated(false),
                       gdi(gdi),
                       adapter(0),
                       context(0),
                       device(0),
                       pagingQueue(0),
                       pagingQueueSyncObject(0),
                       pagingFenceAddress(nullptr),
                       currentPagingFenceValue(0),
                       hwContextId(0),
                       trimCallbackHandle(nullptr) {
    adapterInfo = reinterpret_cast<ADAPTER_INFO *>(alignedMalloc(sizeof(ADAPTER_INFO), 64));
    memset(adapterInfo, 0, sizeof(ADAPTER_INFO));
    registryReader.reset(new RegistryReader("System\\CurrentControlSet\\Control\\GraphicsDrivers\\Scheduler"));
    adapterLuid.HighPart = 0;
    adapterLuid.LowPart = 0;
    maximumApplicationAddress = 0;
    node = GPUNODE_3D;
    gmmMemory = std::unique_ptr<GmmMemory>(GmmMemory::create());
}

Wddm::Wddm() : Wddm(new Gdi()) {
    gdiAllocated = true;
}

Wddm::~Wddm() {
    resetPageTableManager(nullptr);
    alignedFree(adapterInfo);
    if (initialized)
        Gmm::destroyContext();
    destroyContext(context);
    destroyPagingQueue();
    destroyDevice();
    closeAdapter();
    if (gdiAllocated)
        delete gdi;
}

bool Wddm::enumAdapters(unsigned int devNum, ADAPTER_INFO *adapterInfo) {
    bool success = false;
    if (devNum > 0)
        return false;
    if (adapterInfo == nullptr)
        return false;

    Wddm *wddm = createWddm();
    DEBUG_BREAK_IF(wddm == nullptr);

    if (wddm->gdi->isInitialized()) {
        do {
            success = wddm->openAdapter();
            if (!success)
                break;
            success = wddm->queryAdapterInfo();
            if (!success)
                break;
            memcpy_s(adapterInfo, sizeof(ADAPTER_INFO), wddm->adapterInfo, sizeof(ADAPTER_INFO));
        } while (!success);
    }
    delete wddm;
    return success;
}

void Wddm::setupFeatureTableFromAdapterInfo(FeatureTable *table, ADAPTER_INFO *adapterInfo) {
#define COPY_FTR(DST_VAL_NAME, SRC_VAL_NAME) table->DST_VAL_NAME = adapterInfo->SkuTable.SRC_VAL_NAME
    COPY_FTR(ftrDesktop, FtrDesktop);
    COPY_FTR(ftrChannelSwizzlingXOREnabled, FtrChannelSwizzlingXOREnabled);

    COPY_FTR(ftrGtBigDie, FtrGtBigDie);
    COPY_FTR(ftrGtMediumDie, FtrGtMediumDie);
    COPY_FTR(ftrGtSmallDie, FtrGtSmallDie);

    COPY_FTR(ftrGT1, FtrGT1);
    COPY_FTR(ftrGT1_5, FtrGT1_5);
    COPY_FTR(ftrGT2, FtrGT2);
    COPY_FTR(ftrGT2_5, FtrGT2_5);
    COPY_FTR(ftrGT3, FtrGT3);
    COPY_FTR(ftrGT4, FtrGT4);

    COPY_FTR(ftrIVBM0M1Platform, FtrIVBM0M1Platform);
    COPY_FTR(ftrSGTPVSKUStrapPresent, FtrSGTPVSKUStrapPresent);
    COPY_FTR(ftrGTA, FtrGTA);
    COPY_FTR(ftrGTC, FtrGTC);
    COPY_FTR(ftrGTX, FtrGTX);
    COPY_FTR(ftr5Slice, Ftr5Slice);

    COPY_FTR(ftrGpGpuMidBatchPreempt, FtrGpGpuMidBatchPreempt);
    COPY_FTR(ftrGpGpuThreadGroupLevelPreempt, FtrGpGpuThreadGroupLevelPreempt);
    COPY_FTR(ftrGpGpuMidThreadLevelPreempt, FtrGpGpuMidThreadLevelPreempt);

    COPY_FTR(ftrIoMmuPageFaulting, FtrIoMmuPageFaulting);
    COPY_FTR(ftrWddm2Svm, FtrWddm2Svm);
    COPY_FTR(ftrPooledEuEnabled, FtrPooledEuEnabled);

    COPY_FTR(ftrResourceStreamer, FtrResourceStreamer);

    COPY_FTR(ftrPPGTT, FtrPPGTT);
    COPY_FTR(ftrSVM, FtrSVM);
    COPY_FTR(ftrEDram, FtrEDram);
    COPY_FTR(ftrL3IACoherency, FtrL3IACoherency);
    COPY_FTR(ftrIA32eGfxPTEs, FtrIA32eGfxPTEs);

    COPY_FTR(ftr3dMidBatchPreempt, Ftr3dMidBatchPreempt);
    COPY_FTR(ftr3dObjectLevelPreempt, Ftr3dObjectLevelPreempt);
    COPY_FTR(ftrPerCtxtPreemptionGranularityControl, FtrPerCtxtPreemptionGranularityControl);

    COPY_FTR(ftrDisplayYTiling, FtrDisplayYTiling);
    COPY_FTR(ftrTranslationTable, FtrTranslationTable);
    COPY_FTR(ftrUserModeTranslationTable, FtrUserModeTranslationTable);

    COPY_FTR(ftrEnableGuC, FtrEnableGuC);

    COPY_FTR(ftrFbc, FtrFbc);
    COPY_FTR(ftrFbc2AddressTranslation, FtrFbc2AddressTranslation);
    COPY_FTR(ftrFbcBlitterTracking, FtrFbcBlitterTracking);
    COPY_FTR(ftrFbcCpuTracking, FtrFbcCpuTracking);

    COPY_FTR(ftrVcs2, FtrVcs2);
    COPY_FTR(ftrVEBOX, FtrVEBOX);
    COPY_FTR(ftrSingleVeboxSlice, FtrSingleVeboxSlice);
    COPY_FTR(ftrULT, FtrULT);
    COPY_FTR(ftrLCIA, FtrLCIA);
    COPY_FTR(ftrGttCacheInvalidation, FtrGttCacheInvalidation);
    COPY_FTR(ftrTileMappedResource, FtrTileMappedResource);
    COPY_FTR(ftrAstcHdr2D, FtrAstcHdr2D);
    COPY_FTR(ftrAstcLdr2D, FtrAstcLdr2D);

    COPY_FTR(ftrStandardMipTailFormat, FtrStandardMipTailFormat);
    COPY_FTR(ftrFrameBufferLLC, FtrFrameBufferLLC);
    COPY_FTR(ftrCrystalwell, FtrCrystalwell);
    COPY_FTR(ftrLLCBypass, FtrLLCBypass);
    COPY_FTR(ftrDisplayEngineS3d, FtrDisplayEngineS3d);
    COPY_FTR(ftrVERing, FtrVERing);
#undef COPY_FTR
}
void Wddm::setupWorkaroundTableFromAdapterInfo(WorkaroundTable *table, ADAPTER_INFO *adapterInfo) {
#define COPY_WA(DST_VAL_NAME, SRC_VAL_NAME) table->DST_VAL_NAME = adapterInfo->WaTable.SRC_VAL_NAME
    COPY_WA(waDoNotUseMIReportPerfCount, WaDoNotUseMIReportPerfCount);

    COPY_WA(waEnablePreemptionGranularityControlByUMD, WaEnablePreemptionGranularityControlByUMD);
    COPY_WA(waSendMIFLUSHBeforeVFE, WaSendMIFLUSHBeforeVFE);
    COPY_WA(waReportPerfCountUseGlobalContextID, WaReportPerfCountUseGlobalContextID);
    COPY_WA(waDisableLSQCROPERFforOCL, WaDisableLSQCROPERFforOCL);
    COPY_WA(waMsaa8xTileYDepthPitchAlignment, WaMsaa8xTileYDepthPitchAlignment);
    COPY_WA(waLosslessCompressionSurfaceStride, WaLosslessCompressionSurfaceStride);
    COPY_WA(waFbcLinearSurfaceStride, WaFbcLinearSurfaceStride);
    COPY_WA(wa4kAlignUVOffsetNV12LinearSurface, Wa4kAlignUVOffsetNV12LinearSurface);
    COPY_WA(waEncryptedEdramOnlyPartials, WaEncryptedEdramOnlyPartials);
    COPY_WA(waDisableEdramForDisplayRT, WaDisableEdramForDisplayRT);
    COPY_WA(waForcePcBbFullCfgRestore, WaForcePcBbFullCfgRestore);
    COPY_WA(waCompressedResourceRequiresConstVA21, WaCompressedResourceRequiresConstVA21);
    COPY_WA(waDisablePerCtxtPreemptionGranularityControl, WaDisablePerCtxtPreemptionGranularityControl);
    COPY_WA(waLLCCachingUnsupported, WaLLCCachingUnsupported);
    COPY_WA(waUseVAlign16OnTileXYBpp816, WaUseVAlign16OnTileXYBpp816);
#undef COPY_WA
}

bool Wddm::queryAdapterInfo() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_QUERYADAPTERINFO QueryAdapterInfo = {0};
    QueryAdapterInfo.hAdapter = adapter;
    QueryAdapterInfo.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    QueryAdapterInfo.pPrivateDriverData = adapterInfo;
    QueryAdapterInfo.PrivateDriverDataSize = sizeof(ADAPTER_INFO);

    status = gdi->queryAdapterInfo(&QueryAdapterInfo);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    // translate
    if (status == STATUS_SUCCESS) {
        featureTable.reset(new FeatureTable());
        Wddm::setupFeatureTableFromAdapterInfo(featureTable.get(), adapterInfo);

        waTable.reset(new WorkaroundTable());
        Wddm::setupWorkaroundTableFromAdapterInfo(waTable.get(), adapterInfo);
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::createPagingQueue() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATEPAGINGQUEUE CreatePagingQueue = {0};
    CreatePagingQueue.hDevice = device;
    CreatePagingQueue.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;

    status = gdi->createPagingQueue(&CreatePagingQueue);

    if (status == STATUS_SUCCESS) {
        pagingQueue = CreatePagingQueue.hPagingQueue;
        pagingQueueSyncObject = CreatePagingQueue.hSyncObject;
        pagingFenceAddress = reinterpret_cast<UINT64 *>(CreatePagingQueue.FenceValueCPUVirtualAddress);
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::destroyPagingQueue() {
    D3DDDI_DESTROYPAGINGQUEUE DestroyPagingQueue = {0};
    if (pagingQueue) {
        DestroyPagingQueue.hPagingQueue = pagingQueue;

        NTSTATUS status = gdi->destroyPagingQueue(&DestroyPagingQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        pagingQueue = 0;
    }
    return true;
}

bool Wddm::createDevice() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATEDEVICE CreateDevice = {{0}};
    if (adapter) {
        CreateDevice.hAdapter = adapter;
        CreateDevice.Flags.LegacyMode = FALSE;
        if (DebugManager.flags.ForcePreemptionMode.get() != PreemptionMode::Disabled) {
            CreateDevice.Flags.DisableGpuTimeout = readEnablePreemptionRegKey();
        }

        status = gdi->createDevice(&CreateDevice);
        if (status == STATUS_SUCCESS) {
            device = CreateDevice.hDevice;
        }
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::destroyDevice() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_DESTROYDEVICE DestroyDevice = {0};
    if (device) {
        DestroyDevice.hDevice = device;

        status = gdi->destroyDevice(&DestroyDevice);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        device = 0;
    }
    return true;
}

bool Wddm::createMonitoredFence() {
    NTSTATUS Status;
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 CreateSynchronizationObject = {0};
    DEBUG_BREAK_IF(!device);
    CreateSynchronizationObject.hDevice = device;
    CreateSynchronizationObject.Info.Type = D3DDDI_MONITORED_FENCE;
    CreateSynchronizationObject.Info.MonitoredFence.InitialFenceValue = 0;

    Status = gdi->createSynchronizationObject2(&CreateSynchronizationObject);

    DEBUG_BREAK_IF(STATUS_SUCCESS != Status);

    monitoredFence.currentFenceValue = 1;
    monitoredFence.fenceHandle = CreateSynchronizationObject.hSyncObject;
    monitoredFence.cpuAddress = reinterpret_cast<UINT64 *>(CreateSynchronizationObject.Info.MonitoredFence.FenceValueCPUVirtualAddress);
    monitoredFence.lastSubmittedFence = 0;

    monitoredFence.gpuAddress = CreateSynchronizationObject.Info.MonitoredFence.FenceValueGPUVirtualAddress;

    return Status == STATUS_SUCCESS;
}

bool Wddm::closeAdapter() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CLOSEADAPTER CloseAdapter = {0};
    CloseAdapter.hAdapter = adapter;
    status = gdi->closeAdapter(&CloseAdapter);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    adapter = 0;
    return true;
}

bool Wddm::openAdapter() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_OPENADAPTERFROMLUID OpenAdapterData = {{0}};
    DXGI_ADAPTER_DESC1 OpenAdapterDesc = {{0}};

    IDXGIFactory1 *pFactory = nullptr;
    IDXGIAdapter1 *pAdapter = nullptr;
    DWORD iDevNum = 0;

    HRESULT hr = Wddm::createDxgiFactory(__uuidof(IDXGIFactory), (void **)(&pFactory));
    if ((hr != S_OK) || (pFactory == nullptr)) {
        return false;
    }

    while (pFactory->EnumAdapters1(iDevNum++, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        hr = pAdapter->GetDesc1(&OpenAdapterDesc);
        if (hr == S_OK) {
            // Check for adapters that include either "Intel" or "Citrix" (which may
            // be virtualizing one of our adapters) in the description
            if ((wcsstr(OpenAdapterDesc.Description, L"Intel") != 0) ||
                (wcsstr(OpenAdapterDesc.Description, L"Citrix") != 0)) {
                break;
            }
        }
        // Release all the non-Intel adapters
        pAdapter->Release();
        pAdapter = nullptr;
    }

    OpenAdapterData.AdapterLuid = OpenAdapterDesc.AdapterLuid;
    status = gdi->openAdapterFromLuid(&OpenAdapterData);

    if (pAdapter != nullptr) {
        // If an Intel adapter was found, release it here
        pAdapter->Release();
        pAdapter = nullptr;
    }
    if (pFactory != nullptr) {
        pFactory->Release();
        pFactory = nullptr;
    }

    if (status == STATUS_SUCCESS) {
        adapter = OpenAdapterData.hAdapter;
        adapterLuid = OpenAdapterDesc.AdapterLuid;
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::evict(D3DKMT_HANDLE *handleList, uint32_t numOfHandles, uint64_t &sizeToTrim) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_EVICT Evict = {0};
    Evict.AllocationList = handleList;
    Evict.hDevice = device;
    Evict.NumAllocations = numOfHandles;
    Evict.NumBytesToTrim = 0;

    status = gdi->evict(&Evict);

    sizeToTrim = Evict.NumBytesToTrim;

    return status == STATUS_SUCCESS;
}

bool Wddm::makeResident(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_MAKERESIDENT makeResident = {0};
    UINT priority = 0;
    bool success = false;

    makeResident.AllocationList = handles;
    makeResident.hPagingQueue = pagingQueue;
    makeResident.NumAllocations = count;
    makeResident.PriorityList = &priority;
    makeResident.Flags.CantTrimFurther = cantTrimFurther ? 1 : 0;
    makeResident.Flags.MustSucceed = cantTrimFurther ? 1 : 0;

    status = gdi->makeResident(&makeResident);

    if (status == STATUS_PENDING) {
        interlockedMax(currentPagingFenceValue, makeResident.PagingFenceValue);
        success = true;
    } else if (status == STATUS_SUCCESS) {
        success = true;
    } else {
        DEBUG_BREAK_IF(true);
        if (numberOfBytesToTrim != nullptr)
            *numberOfBytesToTrim = makeResident.NumBytesToTrim;
        UNRECOVERABLE_IF(cantTrimFurther);
    }

    return success;
}

bool Wddm::evict(OsHandleStorage &osHandles) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_EVICT Evict = {0};
    auto sizeToTrim = 0uLL;

    D3DKMT_HANDLE handles[max_fragments_count] = {0};
    for (uint32_t allocationId = 0; allocationId < osHandles.fragmentCount; allocationId++) {
        handles[allocationId] = osHandles.fragmentStorageData[allocationId].osHandleStorage->handle;
        sizeToTrim += osHandles.fragmentStorageData[allocationId].fragmentSize;
    }

    Evict.AllocationList = handles;
    Evict.hDevice = device;
    Evict.NumAllocations = osHandles.fragmentCount;
    Evict.NumBytesToTrim = sizeToTrim;

    return status == STATUS_SUCCESS;
}

bool Wddm::mapGpuVirtualAddress(WddmAllocation *allocation, void *cpuPtr, uint64_t size, bool allocation32bit, bool use64kbPages) {
    return mapGpuVirtualAddressImpl(allocation->gmm, allocation->handle, cpuPtr, size, allocation->gpuPtr, allocation32bit, use64kbPages);
}

bool Wddm::mapGpuVirtualAddress(AllocationStorageData *allocationStorageData, bool allocation32bit, bool use64kbPages) {
    return mapGpuVirtualAddressImpl(allocationStorageData->osHandleStorage->gmm,
                                    allocationStorageData->osHandleStorage->handle,
                                    const_cast<void *>(allocationStorageData->cpuPtr),
                                    allocationStorageData->fragmentSize,
                                    allocationStorageData->osHandleStorage->gpuPtr,
                                    allocation32bit, use64kbPages);
}

bool Wddm::mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, uint64_t size, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, bool allocation32bit, bool use64kbPages) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_MAPGPUVIRTUALADDRESS MapGPUVA = {0};
    D3DDDIGPUVIRTUALADDRESS_PROTECTION_TYPE protectionType = {{{0}}};
    protectionType.Write = TRUE;

    MapGPUVA.hPagingQueue = pagingQueue;
    MapGPUVA.hAllocation = handle;
    MapGPUVA.Protection = protectionType;
    MapGPUVA.SizeInPages = size / MemoryConstants::pageSize;
    MapGPUVA.OffsetInPages = 0;

    if (use64kbPages) {
        MapGPUVA.MinimumAddress = adapterInfo->GfxPartition.Standard64KB.Base;
        MapGPUVA.MaximumAddress = adapterInfo->GfxPartition.Standard64KB.Limit;
    } else {
        MapGPUVA.BaseAddress = reinterpret_cast<D3DGPU_VIRTUAL_ADDRESS>(cpuPtr);
        MapGPUVA.MinimumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0x0);
        MapGPUVA.MaximumAddress = static_cast<D3DGPU_VIRTUAL_ADDRESS>((sizeof(size_t) == 8) ? 0x7ffffffffff : (D3DGPU_VIRTUAL_ADDRESS)0xffffffff);

        if (!cpuPtr) {
            MapGPUVA.MinimumAddress = adapterInfo->GfxPartition.Standard.Base;
            MapGPUVA.MaximumAddress = adapterInfo->GfxPartition.Standard.Limit;
        }
        if (allocation32bit) {
            MapGPUVA.MinimumAddress = adapterInfo->GfxPartition.Heap32[0].Base;
            MapGPUVA.MaximumAddress = adapterInfo->GfxPartition.Heap32[0].Limit;
            MapGPUVA.BaseAddress = 0;
        }
    }

    status = gdi->mapGpuVirtualAddress(&MapGPUVA);
    gpuPtr = Gmm::canonize(MapGPUVA.VirtualAddress);

    if (status == STATUS_PENDING) {
        interlockedMax(currentPagingFenceValue, MapGPUVA.PagingFenceValue);
        status = STATUS_SUCCESS;
    }

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    if (gmm && gmm->isRenderCompressed) {
        GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
        ddiUpdateAuxTable.BaseGpuVA = gpuPtr;
        ddiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
        ddiUpdateAuxTable.DoNotWait = true;
        ddiUpdateAuxTable.Map = true;
        return updateAuxTable(ddiUpdateAuxTable);
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::freeGpuVirtualAddres(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_FREEGPUVIRTUALADDRESS FreeGPUVA = {0};
    FreeGPUVA.hAdapter = adapter;
    FreeGPUVA.BaseAddress = gpuPtr;
    FreeGPUVA.Size = size;

    status = gdi->freeGpuVirtualAddress(&FreeGPUVA);
    gpuPtr = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0);
    return status == STATUS_SUCCESS;
}

bool Wddm::createAllocation(WddmAllocation *alloc) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_ALLOCATIONINFO AllocationInfo = {0};
    D3DKMT_CREATEALLOCATION CreateAllocation = {0};
    bool success = false;
    size_t size;

    if (alloc == nullptr)
        return false;
    size = alloc->getUnderlyingBufferSize();
    if (size == 0)
        return false;

    AllocationInfo.pSystemMem = alloc->getAlignedCpuPtr();
    AllocationInfo.pPrivateDriverData = alloc->gmm->gmmResourceInfo->peekHandle();
    AllocationInfo.PrivateDriverDataSize = static_cast<unsigned int>(sizeof(GMM_RESOURCE_INFO));
    AllocationInfo.Flags.Primary = 0;

    CreateAllocation.hGlobalShare = 0;
    CreateAllocation.PrivateRuntimeDataSize = 0;
    CreateAllocation.PrivateDriverDataSize = 0;
    CreateAllocation.Flags.Reserved = 0;
    CreateAllocation.NumAllocations = 1;
    CreateAllocation.pPrivateRuntimeData = NULL;
    CreateAllocation.pPrivateDriverData = NULL;
    CreateAllocation.Flags.NonSecure = FALSE;
    CreateAllocation.Flags.CreateShared = FALSE;
    CreateAllocation.Flags.RestrictSharedAccess = FALSE;
    CreateAllocation.Flags.CreateResource = alloc->getAlignedCpuPtr() == 0 ? TRUE : FALSE;
    CreateAllocation.pAllocationInfo = &AllocationInfo;
    CreateAllocation.hDevice = device;

    while (!success) {

        status = gdi->createAllocation(&CreateAllocation);

        if (status != STATUS_SUCCESS) {
            DEBUG_BREAK_IF(true);
            break;
        }

        alloc->handle = AllocationInfo.hAllocation;
        success = mapGpuVirtualAddress(alloc, alloc->getAlignedCpuPtr(), size, alloc->is32BitAllocation, false);

        if (!success) {
            DEBUG_BREAK_IF(true);
            break;
        }

        success = true;
    }
    return success;
}

bool Wddm::createAllocation64k(WddmAllocation *alloc) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_ALLOCATIONINFO AllocationInfo = {0};
    D3DKMT_CREATEALLOCATION CreateAllocation = {0};
    bool success = false;

    AllocationInfo.pSystemMem = 0;
    AllocationInfo.pPrivateDriverData = alloc->gmm->gmmResourceInfo->peekHandle();
    AllocationInfo.PrivateDriverDataSize = static_cast<unsigned int>(sizeof(GMM_RESOURCE_INFO));
    AllocationInfo.Flags.Primary = 0;

    CreateAllocation.NumAllocations = 1;
    CreateAllocation.pPrivateRuntimeData = NULL;
    CreateAllocation.pPrivateDriverData = NULL;
    CreateAllocation.Flags.CreateResource = TRUE;
    CreateAllocation.pAllocationInfo = &AllocationInfo;
    CreateAllocation.hDevice = device;

    while (!success) {

        status = gdi->createAllocation(&CreateAllocation);

        if (status != STATUS_SUCCESS) {
            DEBUG_BREAK_IF(true);
            break;
        }

        alloc->handle = AllocationInfo.hAllocation;

        success = true;
    }
    return true;
}

bool Wddm::createAllocationsAndMapGpuVa(OsHandleStorage &osHandles) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_ALLOCATIONINFO AllocationInfo[max_fragments_count] = {{0}};
    D3DKMT_CREATEALLOCATION CreateAllocation = {0};
    bool success = false;

    auto allocationCount = 0;
    for (unsigned int i = 0; i < max_fragments_count; i++) {
        if (!osHandles.fragmentStorageData[i].osHandleStorage) {
            break;
        }

        if (osHandles.fragmentStorageData[i].osHandleStorage->handle == (D3DKMT_HANDLE) nullptr && osHandles.fragmentStorageData[i].fragmentSize) {
            AllocationInfo[allocationCount].pPrivateDriverData = osHandles.fragmentStorageData[i].osHandleStorage->gmm->gmmResourceInfo->peekHandle();
            auto pSysMem = osHandles.fragmentStorageData[i].cpuPtr;
            auto PSysMemFromGmm = osHandles.fragmentStorageData[i].osHandleStorage->gmm->gmmResourceInfo->getSystemMemPointer(CL_TRUE);
            DEBUG_BREAK_IF(PSysMemFromGmm != pSysMem);
            AllocationInfo[allocationCount].pSystemMem = osHandles.fragmentStorageData[i].cpuPtr;
            AllocationInfo[allocationCount].PrivateDriverDataSize = static_cast<unsigned int>(sizeof(GMM_RESOURCE_INFO));
            allocationCount++;
        }
    }
    if (allocationCount == 0)
        return true;

    CreateAllocation.hGlobalShare = 0;
    CreateAllocation.PrivateRuntimeDataSize = 0;
    CreateAllocation.PrivateDriverDataSize = 0;
    CreateAllocation.Flags.Reserved = 0;
    CreateAllocation.NumAllocations = allocationCount;
    CreateAllocation.pPrivateRuntimeData = NULL;
    CreateAllocation.pPrivateDriverData = NULL;
    CreateAllocation.Flags.NonSecure = FALSE;
    CreateAllocation.Flags.CreateShared = FALSE;
    CreateAllocation.Flags.RestrictSharedAccess = FALSE;
    CreateAllocation.Flags.CreateResource = FALSE;
    CreateAllocation.pAllocationInfo = AllocationInfo;
    CreateAllocation.hDevice = device;

    while (!success) {

        status = gdi->createAllocation(&CreateAllocation);

        if (status != STATUS_SUCCESS) {
            DBG_LOG(PrintDebugMessages, __FUNCTION__, "status: ", status);
            DEBUG_BREAK_IF(true);
            break;
        }
        auto allocationIndex = 0;
        for (int i = 0; i < allocationCount; i++) {
            while (osHandles.fragmentStorageData[allocationIndex].osHandleStorage->handle) {
                allocationIndex++;
            }
            osHandles.fragmentStorageData[allocationIndex].osHandleStorage->handle = AllocationInfo[i].hAllocation;
            success = mapGpuVirtualAddress(&osHandles.fragmentStorageData[allocationIndex], false, false);
            allocationIndex++;
        }

        if (!success) {
            DBG_LOG(PrintDebugMessages, __FUNCTION__, "mapGpuVirtualAddress: ", success);
            DEBUG_BREAK_IF(true);
            break;
        }

        success = true;
    }
    return success;
}

bool Wddm::destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue, D3DKMT_HANDLE resourceHandle) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_DESTROYALLOCATION2 DestroyAllocation = {0};
    DEBUG_BREAK_IF(!(allocationCount <= 1 || resourceHandle == 0));
    waitFromCpu(lastFenceValue);

    DestroyAllocation.hDevice = device;
    DestroyAllocation.hResource = resourceHandle;
    DestroyAllocation.phAllocationList = handles;
    DestroyAllocation.AllocationCount = allocationCount;

    DestroyAllocation.Flags.AssumeNotInUse = 1;

    status = gdi->destroyAllocation2(&DestroyAllocation);

    return status == STATUS_SUCCESS;
}

bool Wddm::openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFO QueryResourceInfo = {0};
    QueryResourceInfo.hDevice = device;
    QueryResourceInfo.hGlobalShare = handle;
    auto status = gdi->queryResourceInfo(&QueryResourceInfo);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    if (QueryResourceInfo.NumAllocations == 0) {
        return false;
    }

    std::unique_ptr<char[]> allocPrivateData(new char[QueryResourceInfo.TotalPrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateData(new char[QueryResourceInfo.ResourcePrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateRuntimeData(new char[QueryResourceInfo.PrivateRuntimeDataSize]);
    std::unique_ptr<D3DDDI_OPENALLOCATIONINFO[]> allocationInfo(new D3DDDI_OPENALLOCATIONINFO[QueryResourceInfo.NumAllocations]);

    D3DKMT_OPENRESOURCE OpenResource = {0};

    OpenResource.hDevice = device;
    OpenResource.hGlobalShare = handle;
    OpenResource.NumAllocations = QueryResourceInfo.NumAllocations;
    OpenResource.pOpenAllocationInfo = allocationInfo.get();
    OpenResource.pTotalPrivateDriverDataBuffer = allocPrivateData.get();
    OpenResource.TotalPrivateDriverDataBufferSize = QueryResourceInfo.TotalPrivateDriverDataSize;
    OpenResource.pResourcePrivateDriverData = resPrivateData.get();
    OpenResource.ResourcePrivateDriverDataSize = QueryResourceInfo.ResourcePrivateDriverDataSize;
    OpenResource.pPrivateRuntimeData = resPrivateRuntimeData.get();
    OpenResource.PrivateRuntimeDataSize = QueryResourceInfo.PrivateRuntimeDataSize;

    status = gdi->openResource(&OpenResource);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    alloc->handle = allocationInfo[0].hAllocation;
    alloc->resourceHandle = OpenResource.hResource;

    alloc->gmm = Gmm::create((PGMM_RESOURCE_INFO)(allocationInfo[0].pPrivateDriverData));

    return true;
}

bool Wddm::openNTHandle(HANDLE handle, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE queryResourceInfoFromNtHandle = {};
    queryResourceInfoFromNtHandle.hDevice = device;
    queryResourceInfoFromNtHandle.hNtHandle = handle;
    auto status = gdi->queryResourceInfoFromNtHandle(&queryResourceInfoFromNtHandle);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    std::unique_ptr<char[]> allocPrivateData(new char[queryResourceInfoFromNtHandle.TotalPrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateData(new char[queryResourceInfoFromNtHandle.ResourcePrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateRuntimeData(new char[queryResourceInfoFromNtHandle.PrivateRuntimeDataSize]);
    std::unique_ptr<D3DDDI_OPENALLOCATIONINFO2[]> allocationInfo2(new D3DDDI_OPENALLOCATIONINFO2[queryResourceInfoFromNtHandle.NumAllocations]);

    D3DKMT_OPENRESOURCEFROMNTHANDLE openResourceFromNtHandle = {};

    openResourceFromNtHandle.hDevice = device;
    openResourceFromNtHandle.hNtHandle = handle;
    openResourceFromNtHandle.NumAllocations = queryResourceInfoFromNtHandle.NumAllocations;
    openResourceFromNtHandle.pOpenAllocationInfo2 = allocationInfo2.get();
    openResourceFromNtHandle.pTotalPrivateDriverDataBuffer = allocPrivateData.get();
    openResourceFromNtHandle.TotalPrivateDriverDataBufferSize = queryResourceInfoFromNtHandle.TotalPrivateDriverDataSize;
    openResourceFromNtHandle.pResourcePrivateDriverData = resPrivateData.get();
    openResourceFromNtHandle.ResourcePrivateDriverDataSize = queryResourceInfoFromNtHandle.ResourcePrivateDriverDataSize;
    openResourceFromNtHandle.pPrivateRuntimeData = resPrivateRuntimeData.get();
    openResourceFromNtHandle.PrivateRuntimeDataSize = queryResourceInfoFromNtHandle.PrivateRuntimeDataSize;

    status = gdi->openResourceFromNtHandle(&openResourceFromNtHandle);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    alloc->handle = allocationInfo2[0].hAllocation;
    alloc->resourceHandle = openResourceFromNtHandle.hResource;

    alloc->gmm = Gmm::create((PGMM_RESOURCE_INFO)(allocationInfo2[0].pPrivateDriverData));

    return true;
}

void *Wddm::lockResource(WddmAllocation *wddmAllocation) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_LOCK2 lock2 = {};

    lock2.hAllocation = wddmAllocation->handle;
    lock2.hDevice = this->device;

    status = gdi->lock2(&lock2);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    return lock2.pData;
}

void Wddm::unlockResource(WddmAllocation *wddmAllocation) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_UNLOCK2 unlock2 = {};

    unlock2.hAllocation = wddmAllocation->handle;
    unlock2.hDevice = this->device;

    status = gdi->unlock2(&unlock2);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
}

D3DKMT_HANDLE Wddm::createContext() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATECONTEXTVIRTUAL CreateContext = {0};
    CREATECONTEXT_PVTDATA PrivateData = {{0}};

    PrivateData.IsProtectedProcess = FALSE;
    PrivateData.IsDwm = FALSE;
    PrivateData.ProcessID = GetCurrentProcessId();
    PrivateData.GpuVAContext = TRUE;
    PrivateData.pHwContextId = &hwContextId;
    PrivateData.IsMediaUsage = false;

    CreateContext.EngineAffinity = 0;
    CreateContext.Flags.NullRendering = (UINT)DebugManager.flags.EnableNullHardware.get();
    CreateContext.PrivateDriverDataSize = sizeof(PrivateData);
    CreateContext.NodeOrdinal = node;
    CreateContext.pPrivateDriverData = &PrivateData;
    CreateContext.ClientHint = D3DKMT_CLIENTHINT_OPENGL;
    CreateContext.hDevice = device;

    status = gdi->createContext(&CreateContext);
    if (status == STATUS_SUCCESS) {
        return CreateContext.hContext;
    }
    return static_cast<D3DKMT_HANDLE>(0);
}

bool Wddm::destroyContext(D3DKMT_HANDLE context) {
    D3DKMT_DESTROYCONTEXT DestroyContext = {0};
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (context != static_cast<D3DKMT_HANDLE>(0)) {
        DestroyContext.hContext = context;
        status = gdi->destroyContext(&DestroyContext);
    }
    return status == STATUS_SUCCESS ? true : false;
}

bool Wddm::submit(void *commandBuffer, size_t size, void *commandHeader) {
    D3DKMT_SUBMITCOMMAND SubmitCommand = {0};
    NTSTATUS status = STATUS_SUCCESS;
    bool success = true;

    SubmitCommand.Commands = reinterpret_cast<D3DGPU_VIRTUAL_ADDRESS>(commandBuffer);
    SubmitCommand.CommandLength = static_cast<UINT>(size);
    SubmitCommand.BroadcastContextCount = 1;
    SubmitCommand.BroadcastContext[0] = context;
    SubmitCommand.Flags.NullRendering = (UINT)DebugManager.flags.EnableNullHardware.get();

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    pHeader->MonitorFenceVA = monitoredFence.gpuAddress;
    pHeader->MonitorFenceValue = monitoredFence.currentFenceValue;

    // Note: Private data should be the CPU VA Address
    SubmitCommand.pPrivateDriverData = commandHeader;
    SubmitCommand.PrivateDriverDataSize = sizeof(COMMAND_BUFFER_HEADER);

    if (currentPagingFenceValue > *pagingFenceAddress) {
        success = waitOnGPU();
    }

    if (success) {

        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", monitoredFence.currentFenceValue);

        status = gdi->submitCommand(&SubmitCommand);
        if (STATUS_SUCCESS != status) {
            success = false;
        } else {
            monitoredFence.lastSubmittedFence = monitoredFence.currentFenceValue;
            monitoredFence.currentFenceValue++;
        }
    }

    getDeviceState();
    UNRECOVERABLE_IF(!success);

    return success;
}

bool Wddm::getDeviceState() {
#ifdef _DEBUG
    D3DKMT_GETDEVICESTATE GetDevState;
    memset(&GetDevState, 0, sizeof(GetDevState));
    NTSTATUS status = STATUS_SUCCESS;

    GetDevState.hDevice = device;
    GetDevState.StateType = D3DKMT_DEVICESTATE_EXECUTION;

    status = gdi->getDeviceState(&GetDevState);
    if (status == STATUS_SUCCESS) {
        if (GetDevState.ExecutionState == D3DKMT_DEVICEEXECUTION_ERROR_OUTOFMEMORY) {
            DEBUG_BREAK_IF(true);
        }
    }
#endif
    return true;
}

void Wddm::handleCompletion() {
    if (monitoredFence.cpuAddress) {
        auto *currentTag = monitoredFence.cpuAddress;
        while (*currentTag < monitoredFence.currentFenceValue - 1)
            ;
    }
}

unsigned int Wddm::readEnablePreemptionRegKey() {
    return static_cast<unsigned int>(registryReader->getSetting("EnablePreemption", 1));
}

bool Wddm::waitOnGPU() {
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU WaitOnGPU = {0};

    WaitOnGPU.hContext = context;
    WaitOnGPU.ObjectCount = 1;
    WaitOnGPU.ObjectHandleArray = &pagingQueueSyncObject;
    uint64_t localPagingFenceValue = currentPagingFenceValue;

    WaitOnGPU.MonitoredFenceValueArray = &localPagingFenceValue;
    NTSTATUS status = gdi->waitForSynchronizationObjectFromGpu(&WaitOnGPU);

    return status == STATUS_SUCCESS;
}

bool Wddm::waitFromCpu(uint64_t lastFenceValue) {
    NTSTATUS status = STATUS_SUCCESS;

    if (lastFenceValue > *monitoredFence.cpuAddress) {
        D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu = {0};
        waitFromCpu.ObjectCount = 1;
        waitFromCpu.ObjectHandleArray = &monitoredFence.fenceHandle;
        waitFromCpu.FenceValueArray = &lastFenceValue;
        waitFromCpu.hDevice = device;
        waitFromCpu.hAsyncEvent = NULL;

        status = gdi->waitForSynchronizationObjectFromCpu(&waitFromCpu);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }

    return status == STATUS_SUCCESS;
}

uint64_t Wddm::getSystemSharedMemory() {
    return adapterInfo->SystemSharedMemory;
}

uint64_t Wddm::getMaxApplicationAddress() {
    return maximumApplicationAddress;
}

NTSTATUS Wddm::escape(D3DKMT_ESCAPE &escapeCommand) {
    escapeCommand.hAdapter = adapter;
    return gdi->escape(&escapeCommand);
};

PFND3DKMT_ESCAPE Wddm::getEscapeHandle() const {
    return gdi->escape;
}

uint64_t Wddm::getHeap32Base() {
    return alignUp(adapterInfo->GfxPartition.Heap32[0].Base, MemoryConstants::pageSize);
}

uint64_t Wddm::getHeap32Size() {
    return alignDown(adapterInfo->GfxPartition.Heap32[0].Limit, MemoryConstants::pageSize);
}

void Wddm::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmMemoryManager *memoryManager) {
    D3DKMT_REGISTERTRIMNOTIFICATION registerTrimNotification;
    registerTrimNotification.Callback = callback;
    registerTrimNotification.AdapterLuid = this->adapterLuid;
    registerTrimNotification.Context = memoryManager;
    registerTrimNotification.hDevice = this->device;

    NTSTATUS status = gdi->registerTrimNotification(&registerTrimNotification);
    if (status == STATUS_SUCCESS) {
        trimCallbackHandle = registerTrimNotification.Handle;
    }
}

void Wddm::releaseGpuPtr(void *gpuPtr) {
    if (gpuPtr) {
        auto status = VirtualFree(gpuPtr, 0, MEM_RELEASE);
        DEBUG_BREAK_IF(status != 1);
    }
}

void Wddm::initPageTableManagerRegisters(LinearStream &stream) {
    if (pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManager->initContextTRTableRegister(&stream, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);
        pageTableManager->initContextAuxTableRegister(&stream, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);

        pageTableManagerInitialized = true;
    }
}

bool Wddm::updateAuxTable(GMM_DDI_UPDATEAUXTABLE &ddiUpdateAuxTable) {
    return pageTableManager->updateAuxTable(&ddiUpdateAuxTable) == GMM_STATUS::GMM_SUCCESS;
}

void Wddm::resetPageTableManager(GmmPageTableMngr *newPageTableManager) {
    pageTableManager.reset(newPageTableManager);
}

} // namespace OCLRT
