/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/interlocked_max.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/windows/debug_registry_reader.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/kmdaf_listener.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/sku_info/operations/windows/sku_info_receiver.h"
#include "shared/source/utilities/stackvec.h"

#include "gmm_memory.h"

#include <dxgi.h>

namespace NEO {
extern Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory();
extern Wddm::GetSystemInfoFcn getGetSystemInfo();
extern Wddm::VirtualAllocFcn getVirtualAlloc();
extern Wddm::VirtualFreeFcn getVirtualFree();

Wddm::CreateDXGIFactoryFcn Wddm::createDxgiFactory = getCreateDxgiFactory();
Wddm::GetSystemInfoFcn Wddm::getSystemInfo = getGetSystemInfo();
Wddm::VirtualAllocFcn Wddm::virtualAllocFnc = getVirtualAlloc();
Wddm::VirtualFreeFcn Wddm::virtualFreeFnc = getVirtualFree();

Wddm::Wddm(std::unique_ptr<HwDeviceId> hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment) : hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {
    UNRECOVERABLE_IF(!hwDeviceId);
    featureTable.reset(new FeatureTable());
    workaroundTable.reset(new WorkaroundTable());
    gtSystemInfo.reset(new GT_SYSTEM_INFO);
    gfxPlatform.reset(new PLATFORM);
    memset(gtSystemInfo.get(), 0, sizeof(*gtSystemInfo));
    memset(gfxPlatform.get(), 0, sizeof(*gfxPlatform));
    this->registryReader.reset(new RegistryReader(false, "System\\CurrentControlSet\\Control\\GraphicsDrivers\\Scheduler"));
    kmDafListener = std::unique_ptr<KmDafListener>(new KmDafListener);
    temporaryResources = std::make_unique<WddmResidentAllocationsContainer>(this);
}

Wddm::~Wddm() {
    temporaryResources.reset();
    destroyPagingQueue();
    destroyDevice();
    UNRECOVERABLE_IF(temporaryResources.get())
}

bool Wddm::init() {
    if (!queryAdapterInfo()) {
        return false;
    }

    auto productFamily = gfxPlatform->eProductFamily;
    if (!hardwareInfoTable[productFamily]) {
        return false;
    }
    auto hardwareInfo = std::make_unique<HardwareInfo>();
    hardwareInfo->platform = *gfxPlatform;
    hardwareInfo->featureTable = *featureTable;
    hardwareInfo->workaroundTable = *workaroundTable;
    hardwareInfo->gtSystemInfo = *gtSystemInfo;

    hardwareInfo->capabilityTable = hardwareInfoTable[productFamily]->capabilityTable;
    hardwareInfo->capabilityTable.maxRenderFrequency = maxRenderFrequency;
    hardwareInfo->capabilityTable.instrumentationEnabled =
        (hardwareInfo->capabilityTable.instrumentationEnabled && instrumentationEnabled);

    HwInfoConfig *hwConfig = HwInfoConfig::get(productFamily);

    hwConfig->adjustPlatformForProductFamily(hardwareInfo.get());
    if (hwConfig->configureHwInfo(hardwareInfo.get(), hardwareInfo.get(), nullptr)) {
        return false;
    }

    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*hardwareInfo);
    rootDeviceEnvironment.setHwInfo(hardwareInfo.get());
    rootDeviceEnvironment.initGmm();

    if (WddmVersion::WDDM_2_3 == getWddmVersion()) {
        wddmInterface = std::make_unique<WddmInterface23>(*this);
    } else {
        wddmInterface = std::make_unique<WddmInterface20>(*this);
    }

    if (!createDevice(preemptionMode)) {
        return false;
    }
    if (!createPagingQueue()) {
        return false;
    }
    if (!gmmMemory) {
        gmmMemory.reset(GmmMemory::create(rootDeviceEnvironment.getGmmClientContext()));
    }

    return configureDeviceAddressSpace();
}

bool Wddm::queryAdapterInfo() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_QUERYADAPTERINFO QueryAdapterInfo = {0};
    ADAPTER_INFO adapterInfo = {0};
    QueryAdapterInfo.hAdapter = getAdapter();
    QueryAdapterInfo.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    QueryAdapterInfo.pPrivateDriverData = &adapterInfo;
    QueryAdapterInfo.PrivateDriverDataSize = sizeof(ADAPTER_INFO);

    status = getGdi()->queryAdapterInfo(&QueryAdapterInfo);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    // translate
    if (status == STATUS_SUCCESS) {
        memcpy_s(gtSystemInfo.get(), sizeof(GT_SYSTEM_INFO), &adapterInfo.SystemInfo, sizeof(GT_SYSTEM_INFO));
        memcpy_s(gfxPlatform.get(), sizeof(PLATFORM), &adapterInfo.GfxPlatform, sizeof(PLATFORM));

        SkuInfoReceiver::receiveFtrTableFromAdapterInfo(featureTable.get(), &adapterInfo);
        SkuInfoReceiver::receiveWaTableFromAdapterInfo(workaroundTable.get(), &adapterInfo);

        memcpy_s(&gfxPartition, sizeof(gfxPartition), &adapterInfo.GfxPartition, sizeof(GMM_GFX_PARTITIONING));
        memcpy_s(&adapterBDF, sizeof(adapterBDF), &adapterInfo.stAdapterBDF, sizeof(ADAPTER_BDF));

        deviceRegistryPath = adapterInfo.DeviceRegistryPath;

        systemSharedMemory = adapterInfo.SystemSharedMemory;
        dedicatedVideoMemory = adapterInfo.DedicatedVideoMemory;
        maxRenderFrequency = adapterInfo.MaxRenderFreq;
        instrumentationEnabled = adapterInfo.Caps.InstrumentationIsEnabled != 0;
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::createPagingQueue() {
    D3DKMT_CREATEPAGINGQUEUE CreatePagingQueue = {0};
    CreatePagingQueue.hDevice = device;
    CreatePagingQueue.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;

    NTSTATUS status = getGdi()->createPagingQueue(&CreatePagingQueue);

    if (status == STATUS_SUCCESS) {
        pagingQueue = CreatePagingQueue.hPagingQueue;
        pagingQueueSyncObject = CreatePagingQueue.hSyncObject;
        pagingFenceAddress = reinterpret_cast<UINT64 *>(CreatePagingQueue.FenceValueCPUVirtualAddress);
        createPagingFenceLogger();
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::destroyPagingQueue() {
    D3DDDI_DESTROYPAGINGQUEUE DestroyPagingQueue = {0};
    if (pagingQueue) {
        DestroyPagingQueue.hPagingQueue = pagingQueue;

        NTSTATUS status = getGdi()->destroyPagingQueue(&DestroyPagingQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        pagingQueue = 0;
    }
    return true;
}

bool Wddm::createDevice(PreemptionMode preemptionMode) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATEDEVICE CreateDevice = {{0}};
    if (hwDeviceId) {
        CreateDevice.hAdapter = getAdapter();
        CreateDevice.Flags.LegacyMode = FALSE;
        if (preemptionMode >= PreemptionMode::MidBatch) {
            CreateDevice.Flags.DisableGpuTimeout = readEnablePreemptionRegKey();
        }

        status = getGdi()->createDevice(&CreateDevice);
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

        status = getGdi()->destroyDevice(&DestroyDevice);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        device = 0;
    }
    return true;
}

std::unique_ptr<HwDeviceId> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid) {
    D3DKMT_OPENADAPTERFROMLUID OpenAdapterData = {{0}};
    OpenAdapterData.AdapterLuid = adapterLuid;
    auto status = osEnvironment.gdi->openAdapterFromLuid(&OpenAdapterData);

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF("openAdapterFromLuid failed");
        return nullptr;
    }

    D3DKMT_QUERYADAPTERINFO QueryAdapterInfo = {0};
    ADAPTER_INFO adapterInfo = {0};
    QueryAdapterInfo.hAdapter = OpenAdapterData.hAdapter;
    QueryAdapterInfo.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    QueryAdapterInfo.pPrivateDriverData = &adapterInfo;
    QueryAdapterInfo.PrivateDriverDataSize = sizeof(ADAPTER_INFO);

    status = osEnvironment.gdi->queryAdapterInfo(&QueryAdapterInfo);

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF("queryAdapterInfo failed");
        return nullptr;
    }

    std::string deviceRegistryPath = adapterInfo.DeviceRegistryPath;
    DriverInfoWindows driverInfo(std::move(deviceRegistryPath));
    if (!driverInfo.isCompatibleDriverStore()) {
        return nullptr;
    }

    return std::make_unique<HwDeviceId>(OpenAdapterData.hAdapter, adapterLuid, &osEnvironment);
}

std::vector<std::unique_ptr<HwDeviceId>> OSInterface::discoverDevices(ExecutionEnvironment &executionEnvironment) {
    std::vector<std::unique_ptr<HwDeviceId>> hwDeviceIds;

    auto osEnvironment = new OsEnvironmentWin();
    auto gdi = osEnvironment->gdi.get();
    executionEnvironment.osEnvironment.reset(osEnvironment);

    if (!gdi->isInitialized()) {
        return hwDeviceIds;
    }

    DXGI_ADAPTER_DESC1 OpenAdapterDesc = {{0}};

    IDXGIFactory1 *pFactory = nullptr;
    IDXGIAdapter1 *pAdapter = nullptr;
    DWORD iDevNum = 0;

    HRESULT hr = Wddm::createDxgiFactory(__uuidof(IDXGIFactory), (void **)(&pFactory));
    if ((hr != S_OK) || (pFactory == nullptr)) {
        return hwDeviceIds;
    }

    while (pFactory->EnumAdapters1(iDevNum++, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        hr = pAdapter->GetDesc1(&OpenAdapterDesc);
        if (hr == S_OK) {
            bool createHwDeviceId = false;
            // Check for adapters that include either "Intel" or "Citrix" (which may
            // be virtualizing one of our adapters) in the description
            if ((wcsstr(OpenAdapterDesc.Description, L"Intel") != 0) ||
                (wcsstr(OpenAdapterDesc.Description, L"Citrix") != 0) ||
                (wcsstr(OpenAdapterDesc.Description, L"Virtual Render") != 0)) {
                char deviceId[16];
                sprintf_s(deviceId, "%X", OpenAdapterDesc.DeviceId);
                createHwDeviceId = (DebugManager.flags.ForceDeviceId.get() == "unk") || (DebugManager.flags.ForceDeviceId.get() == deviceId);
            }
            if (createHwDeviceId) {
                auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnvironment, OpenAdapterDesc.AdapterLuid);
                if (hwDeviceId) {
                    hwDeviceIds.push_back(std::move(hwDeviceId));
                }
            }
        }
        // Release all the non-Intel adapters
        pAdapter->Release();
        pAdapter = nullptr;
    }

    if (pAdapter != nullptr) {
        pAdapter->Release();
        pAdapter = nullptr;
    }
    if (pFactory != nullptr) {
        pFactory->Release();
        pFactory = nullptr;
    }
    size_t numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    if (hwDeviceIds.empty()) {
        return hwDeviceIds;
    }

    while (hwDeviceIds.size() < numRootDevices) {
        hwDeviceIds.push_back(std::make_unique<HwDeviceId>(hwDeviceIds[0]->getAdapter(), hwDeviceIds[0]->getAdapterLuid(), osEnvironment));
    }

    return hwDeviceIds;
}

bool Wddm::evict(const D3DKMT_HANDLE *handleList, uint32_t numOfHandles, uint64_t &sizeToTrim) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_EVICT Evict = {0};
    Evict.AllocationList = handleList;
    Evict.hDevice = device;
    Evict.NumAllocations = numOfHandles;
    Evict.NumBytesToTrim = 0;

    status = getGdi()->evict(&Evict);

    sizeToTrim = Evict.NumBytesToTrim;

    kmDafListener->notifyEvict(featureTable->ftrKmdDaf, getAdapter(), device, handleList, numOfHandles, getGdi()->escape);

    return status == STATUS_SUCCESS;
}

bool Wddm::makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_MAKERESIDENT makeResident = {0};
    UINT priority = 0;
    bool success = false;

    perfLogResidencyReportAllocations(residencyLogger.get(), count, totalSize);

    makeResident.AllocationList = handles;
    makeResident.hPagingQueue = pagingQueue;
    makeResident.NumAllocations = count;
    makeResident.PriorityList = &priority;
    makeResident.Flags.CantTrimFurther = cantTrimFurther ? 1 : 0;
    makeResident.Flags.MustSucceed = cantTrimFurther ? 1 : 0;

    status = getGdi()->makeResident(&makeResident);
    if (status == STATUS_PENDING) {
        perfLogResidencyMakeResident(residencyLogger.get(), true, makeResident.PagingFenceValue);
        updatePagingFenceValue(makeResident.PagingFenceValue);
        success = true;
    } else if (status == STATUS_SUCCESS) {
        perfLogResidencyMakeResident(residencyLogger.get(), false, makeResident.PagingFenceValue);
        success = true;
    } else {
        DEBUG_BREAK_IF(true);
        perfLogResidencyTrimRequired(residencyLogger.get(), makeResident.NumBytesToTrim);
        if (numberOfBytesToTrim != nullptr)
            *numberOfBytesToTrim = makeResident.NumBytesToTrim;
        UNRECOVERABLE_IF(cantTrimFurther);
    }

    kmDafListener->notifyMakeResident(featureTable->ftrKmdDaf, getAdapter(), device, handles, count, getGdi()->escape);

    return success;
}

bool Wddm::mapGpuVirtualAddress(AllocationStorageData *allocationStorageData) {
    return mapGpuVirtualAddress(allocationStorageData->osHandleStorage->gmm,
                                allocationStorageData->osHandleStorage->handle,
                                0u, MemoryConstants::maxSvmAddress, castToUint64(allocationStorageData->cpuPtr),
                                allocationStorageData->osHandleStorage->gpuPtr);
}

bool Wddm::mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr) {
    D3DDDI_MAPGPUVIRTUALADDRESS MapGPUVA = {0};
    D3DDDIGPUVIRTUALADDRESS_PROTECTION_TYPE protectionType = {{{0}}};
    protectionType.Write = TRUE;

    uint64_t size = gmm->gmmResourceInfo->getSizeAllocation();

    MapGPUVA.hPagingQueue = pagingQueue;
    MapGPUVA.hAllocation = handle;
    MapGPUVA.Protection = protectionType;
    MapGPUVA.SizeInPages = size / MemoryConstants::pageSize;
    MapGPUVA.OffsetInPages = 0;

    MapGPUVA.BaseAddress = preferredAddress;
    MapGPUVA.MinimumAddress = minimumAddress;
    MapGPUVA.MaximumAddress = maximumAddress;

    NTSTATUS status = getGdi()->mapGpuVirtualAddress(&MapGPUVA);
    gpuPtr = GmmHelper::canonize(MapGPUVA.VirtualAddress);

    if (status == STATUS_PENDING) {
        updatePagingFenceValue(MapGPUVA.PagingFenceValue);
        status = STATUS_SUCCESS;
    }

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    kmDafListener->notifyMapGpuVA(featureTable->ftrKmdDaf, getAdapter(), device, handle, MapGPUVA.VirtualAddress, getGdi()->escape);

    if (gmm->isRenderCompressed && rootDeviceEnvironment.pageTableManager.get()) {
        return rootDeviceEnvironment.pageTableManager->updateAuxTable(gpuPtr, gmm, true);
    }

    return true;
}

D3DGPU_VIRTUAL_ADDRESS Wddm::reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS minimumAddress,
                                                      D3DGPU_VIRTUAL_ADDRESS maximumAddress,
                                                      D3DGPU_SIZE_T size) {
    UNRECOVERABLE_IF(size % MemoryConstants::pageSize64k);
    D3DDDI_RESERVEGPUVIRTUALADDRESS reserveGpuVirtualAddress = {};
    reserveGpuVirtualAddress.MinimumAddress = minimumAddress;
    reserveGpuVirtualAddress.MaximumAddress = maximumAddress;
    reserveGpuVirtualAddress.hPagingQueue = this->pagingQueue;
    reserveGpuVirtualAddress.Size = size;

    NTSTATUS status = getGdi()->reserveGpuVirtualAddress(&reserveGpuVirtualAddress);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    return reserveGpuVirtualAddress.VirtualAddress;
}

bool Wddm::freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_FREEGPUVIRTUALADDRESS FreeGPUVA = {0};
    FreeGPUVA.hAdapter = getAdapter();
    FreeGPUVA.BaseAddress = GmmHelper::decanonize(gpuPtr);
    FreeGPUVA.Size = size;

    status = getGdi()->freeGpuVirtualAddress(&FreeGPUVA);
    gpuPtr = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0);

    kmDafListener->notifyUnmapGpuVA(featureTable->ftrKmdDaf, getAdapter(), device, FreeGPUVA.BaseAddress, getGdi()->escape);

    return status == STATUS_SUCCESS;
}

NTSTATUS Wddm::createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, D3DKMT_HANDLE *outSharedHandle) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DDDI_ALLOCATIONINFO AllocationInfo = {0};
    D3DKMT_CREATEALLOCATION CreateAllocation = {0};

    if (gmm == nullptr)
        return false;

    AllocationInfo.pSystemMem = alignedCpuPtr;
    AllocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
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
    CreateAllocation.Flags.CreateShared = outSharedHandle ? TRUE : FALSE;
    CreateAllocation.Flags.RestrictSharedAccess = FALSE;
    CreateAllocation.Flags.CreateResource = outSharedHandle || alignedCpuPtr ? TRUE : FALSE;
    CreateAllocation.pAllocationInfo = &AllocationInfo;
    CreateAllocation.hDevice = device;

    status = getGdi()->createAllocation(&CreateAllocation);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return status;
    }

    outHandle = AllocationInfo.hAllocation;
    outResourceHandle = CreateAllocation.hResource;
    if (outSharedHandle) {
        *outSharedHandle = CreateAllocation.hGlobalShare;
    }
    kmDafListener->notifyWriteTarget(featureTable->ftrKmdDaf, getAdapter(), device, outHandle, getGdi()->escape);

    return status;
}

bool Wddm::createAllocation64k(const Gmm *gmm, D3DKMT_HANDLE &outHandle) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_ALLOCATIONINFO AllocationInfo = {0};
    D3DKMT_CREATEALLOCATION CreateAllocation = {0};

    AllocationInfo.pSystemMem = 0;
    AllocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    AllocationInfo.PrivateDriverDataSize = static_cast<unsigned int>(sizeof(GMM_RESOURCE_INFO));
    AllocationInfo.Flags.Primary = 0;

    CreateAllocation.NumAllocations = 1;
    CreateAllocation.pPrivateRuntimeData = NULL;
    CreateAllocation.pPrivateDriverData = NULL;
    CreateAllocation.Flags.CreateResource = TRUE;
    CreateAllocation.pAllocationInfo = &AllocationInfo;
    CreateAllocation.hDevice = device;

    status = getGdi()->createAllocation(&CreateAllocation);

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    outHandle = AllocationInfo.hAllocation;
    kmDafListener->notifyWriteTarget(featureTable->ftrKmdDaf, getAdapter(), device, outHandle, getGdi()->escape);

    return true;
}

NTSTATUS Wddm::createAllocationsAndMapGpuVa(OsHandleStorage &osHandles) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DDDI_ALLOCATIONINFO AllocationInfo[maxFragmentsCount] = {{0}};
    D3DKMT_CREATEALLOCATION CreateAllocation = {0};

    auto allocationCount = 0;
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (!osHandles.fragmentStorageData[i].osHandleStorage) {
            break;
        }

        if (osHandles.fragmentStorageData[i].osHandleStorage->handle == (D3DKMT_HANDLE) nullptr && osHandles.fragmentStorageData[i].fragmentSize) {
            AllocationInfo[allocationCount].pPrivateDriverData = osHandles.fragmentStorageData[i].osHandleStorage->gmm->gmmResourceInfo->peekHandle();
            auto pSysMem = osHandles.fragmentStorageData[i].cpuPtr;
            auto PSysMemFromGmm = osHandles.fragmentStorageData[i].osHandleStorage->gmm->gmmResourceInfo->getSystemMemPointer();
            DEBUG_BREAK_IF(PSysMemFromGmm != pSysMem);
            AllocationInfo[allocationCount].pSystemMem = osHandles.fragmentStorageData[i].cpuPtr;
            AllocationInfo[allocationCount].PrivateDriverDataSize = static_cast<unsigned int>(sizeof(GMM_RESOURCE_INFO));
            allocationCount++;
        }
    }
    if (allocationCount == 0) {
        return STATUS_SUCCESS;
    }

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

    while (status == STATUS_UNSUCCESSFUL) {
        status = getGdi()->createAllocation(&CreateAllocation);

        if (status != STATUS_SUCCESS) {
            DBG_LOG(PrintDebugMessages, __FUNCTION__, "status: ", status);
            DEBUG_BREAK_IF(status != STATUS_GRAPHICS_NO_VIDEO_MEMORY);
            break;
        }
        auto allocationIndex = 0;
        for (int i = 0; i < allocationCount; i++) {
            while (osHandles.fragmentStorageData[allocationIndex].osHandleStorage->handle) {
                allocationIndex++;
            }
            osHandles.fragmentStorageData[allocationIndex].osHandleStorage->handle = AllocationInfo[i].hAllocation;
            bool success = mapGpuVirtualAddress(&osHandles.fragmentStorageData[allocationIndex]);

            if (!success) {
                osHandles.fragmentStorageData[allocationIndex].freeTheFragment = true;
                DBG_LOG(PrintDebugMessages, __FUNCTION__, "mapGpuVirtualAddress: ", success);
                DEBUG_BREAK_IF(true);
                return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
            }

            allocationIndex++;

            kmDafListener->notifyWriteTarget(featureTable->ftrKmdDaf, getAdapter(), device, AllocationInfo[i].hAllocation, getGdi()->escape);
        }

        status = STATUS_SUCCESS;
    }
    return status;
}

bool Wddm::destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_DESTROYALLOCATION2 DestroyAllocation = {0};
    DEBUG_BREAK_IF(!(allocationCount <= 1 || resourceHandle == 0));

    DestroyAllocation.hDevice = device;
    DestroyAllocation.hResource = resourceHandle;
    DestroyAllocation.phAllocationList = handles;
    DestroyAllocation.AllocationCount = allocationCount;

    DestroyAllocation.Flags.AssumeNotInUse = 1;

    status = getGdi()->destroyAllocation2(&DestroyAllocation);

    return status == STATUS_SUCCESS;
}

bool Wddm::openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFO QueryResourceInfo = {0};
    QueryResourceInfo.hDevice = device;
    QueryResourceInfo.hGlobalShare = handle;
    auto status = getGdi()->queryResourceInfo(&QueryResourceInfo);
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

    status = getGdi()->openResource(&OpenResource);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    alloc->setDefaultHandle(allocationInfo[0].hAllocation);
    alloc->resourceHandle = OpenResource.hResource;

    auto resourceInfo = const_cast<void *>(allocationInfo[0].pPrivateDriverData);
    alloc->setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmClientContext(), static_cast<GMM_RESOURCE_INFO *>(resourceInfo)));

    return true;
}

bool Wddm::openNTHandle(HANDLE handle, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE queryResourceInfoFromNtHandle = {};
    queryResourceInfoFromNtHandle.hDevice = device;
    queryResourceInfoFromNtHandle.hNtHandle = handle;
    auto status = getGdi()->queryResourceInfoFromNtHandle(&queryResourceInfoFromNtHandle);
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

    status = getGdi()->openResourceFromNtHandle(&openResourceFromNtHandle);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    alloc->setDefaultHandle(allocationInfo2[0].hAllocation);
    alloc->resourceHandle = openResourceFromNtHandle.hResource;

    auto resourceInfo = const_cast<void *>(allocationInfo2[0].pPrivateDriverData);
    alloc->setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmClientContext(), static_cast<GMM_RESOURCE_INFO *>(resourceInfo)));

    return true;
}

void *Wddm::lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) {

    if (applyMakeResidentPriorToLock) {
        temporaryResources->makeResidentResource(handle, size);
    }

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_LOCK2 lock2 = {};

    lock2.hAllocation = handle;
    lock2.hDevice = this->device;

    status = getGdi()->lock2(&lock2);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    kmDafLock(handle);
    return lock2.pData;
}

void Wddm::unlockResource(const D3DKMT_HANDLE &handle) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_UNLOCK2 unlock2 = {};

    unlock2.hAllocation = handle;
    unlock2.hDevice = this->device;

    status = getGdi()->unlock2(&unlock2);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    kmDafListener->notifyUnlock(featureTable->ftrKmdDaf, getAdapter(), device, &handle, 1, getGdi()->escape);
}

void Wddm::kmDafLock(D3DKMT_HANDLE handle) {
    kmDafListener->notifyLock(featureTable->ftrKmdDaf, getAdapter(), device, handle, 0, getGdi()->escape);
}

bool Wddm::createContext(OsContextWin &osContext) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATECONTEXTVIRTUAL CreateContext = {0};
    CREATECONTEXT_PVTDATA PrivateData = {{0}};

    PrivateData.IsProtectedProcess = FALSE;
    PrivateData.IsDwm = FALSE;
    PrivateData.ProcessID = GetCurrentProcessId();
    PrivateData.GpuVAContext = TRUE;
    PrivateData.pHwContextId = &hwContextId;
    PrivateData.IsMediaUsage = false;
    PrivateData.NoRingFlushes = DebugManager.flags.UseNoRingFlushesKmdMode.get();
    applyAdditionalContextFlags(PrivateData, osContext);

    CreateContext.EngineAffinity = 0;
    CreateContext.Flags.NullRendering = static_cast<UINT>(DebugManager.flags.EnableNullHardware.get());
    CreateContext.Flags.HwQueueSupported = wddmInterface->hwQueuesSupported();

    if (osContext.getPreemptionMode() >= PreemptionMode::MidBatch) {
        CreateContext.Flags.DisableGpuTimeout = readEnablePreemptionRegKey();
    }

    CreateContext.PrivateDriverDataSize = sizeof(PrivateData);
    CreateContext.NodeOrdinal = WddmEngineMapper::engineNodeMap(osContext.getEngineType());
    CreateContext.pPrivateDriverData = &PrivateData;
    CreateContext.ClientHint = D3DKMT_CLIENTHINT_OPENCL;
    CreateContext.hDevice = device;

    status = getGdi()->createContext(&CreateContext);
    osContext.setWddmContextHandle(CreateContext.hContext);

    return status == STATUS_SUCCESS;
}

bool Wddm::destroyContext(D3DKMT_HANDLE context) {
    D3DKMT_DESTROYCONTEXT DestroyContext = {0};
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (context != static_cast<D3DKMT_HANDLE>(0)) {
        DestroyContext.hContext = context;
        status = getGdi()->destroyContext(&DestroyContext);
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    bool status = false;
    if (currentPagingFenceValue > *pagingFenceAddress && !waitOnGPU(submitArguments.contextHandle)) {
        return false;
    }
    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", submitArguments.monitorFence->currentFenceValue);

    status = wddmInterface->submit(commandBuffer, size, commandHeader, submitArguments);
    if (status) {
        submitArguments.monitorFence->lastSubmittedFence = submitArguments.monitorFence->currentFenceValue;
        submitArguments.monitorFence->currentFenceValue++;
    }
    getDeviceState();

    return status;
}

void Wddm::getDeviceState() {
#ifdef _DEBUG
    D3DKMT_GETDEVICESTATE GetDevState;
    memset(&GetDevState, 0, sizeof(GetDevState));
    NTSTATUS status = STATUS_SUCCESS;

    GetDevState.hDevice = device;
    GetDevState.StateType = D3DKMT_DEVICESTATE_EXECUTION;

    status = getGdi()->getDeviceState(&GetDevState);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    if (status == STATUS_SUCCESS) {
        DEBUG_BREAK_IF(GetDevState.ExecutionState != D3DKMT_DEVICEEXECUTION_ACTIVE);
    }
#endif
}

void Wddm::handleCompletion(OsContextWin &osContext) {
    auto &monitoredFence = osContext.getResidencyController().getMonitoredFence();
    if (monitoredFence.cpuAddress) {
        auto *currentTag = monitoredFence.cpuAddress;
        while (*currentTag < monitoredFence.currentFenceValue - 1)
            ;
    }
}

unsigned int Wddm::readEnablePreemptionRegKey() {
    return static_cast<unsigned int>(registryReader->getSetting("EnablePreemption", 1));
}

bool Wddm::waitOnGPU(D3DKMT_HANDLE context) {
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU WaitOnGPU = {0};

    WaitOnGPU.hContext = context;
    WaitOnGPU.ObjectCount = 1;
    WaitOnGPU.ObjectHandleArray = &pagingQueueSyncObject;
    uint64_t localPagingFenceValue = currentPagingFenceValue;

    WaitOnGPU.MonitoredFenceValueArray = &localPagingFenceValue;
    NTSTATUS status = getGdi()->waitForSynchronizationObjectFromGpu(&WaitOnGPU);

    return status == STATUS_SUCCESS;
}

bool Wddm::waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence) {
    NTSTATUS status = STATUS_SUCCESS;

    if (lastFenceValue > *monitoredFence.cpuAddress) {
        D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu = {0};
        waitFromCpu.ObjectCount = 1;
        waitFromCpu.ObjectHandleArray = &monitoredFence.fenceHandle;
        waitFromCpu.FenceValueArray = &lastFenceValue;
        waitFromCpu.hDevice = device;
        waitFromCpu.hAsyncEvent = NULL;

        status = getGdi()->waitForSynchronizationObjectFromCpu(&waitFromCpu);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }

    return status == STATUS_SUCCESS;
}

void Wddm::initGfxPartition(GfxPartition &outGfxPartition, uint32_t rootDeviceIndex, size_t numRootDevices) const {
    if (gfxPartition.SVM.Limit != 0) {
        outGfxPartition.heapInit(HeapIndex::HEAP_SVM, gfxPartition.SVM.Base, gfxPartition.SVM.Limit - gfxPartition.SVM.Base + 1);
    } else if (is32bit) {
        outGfxPartition.heapInit(HeapIndex::HEAP_SVM, 0x0ull, 4 * MemoryConstants::gigaByte);
    }

    outGfxPartition.heapInit(HeapIndex::HEAP_STANDARD, gfxPartition.Standard.Base, gfxPartition.Standard.Limit - gfxPartition.Standard.Base + 1);

    // Split HEAP_STANDARD64K among root devices
    auto gfxStandard64KBSize = alignDown((gfxPartition.Standard64KB.Limit - gfxPartition.Standard64KB.Base + 1) / numRootDevices, GfxPartition::heapGranularity);
    outGfxPartition.heapInit(HeapIndex::HEAP_STANDARD64KB, gfxPartition.Standard64KB.Base + rootDeviceIndex * gfxStandard64KBSize, gfxStandard64KBSize);

    for (auto heap : GfxPartition::heap32Names) {
        outGfxPartition.heapInit(heap, gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base,
                                 gfxPartition.Heap32[static_cast<uint32_t>(heap)].Limit - gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base + 1);
    }
}

uint64_t Wddm::getSystemSharedMemory() const {
    return systemSharedMemory;
}

uint64_t Wddm::getDedicatedVideoMemory() const {
    return dedicatedVideoMemory;
}

uint64_t Wddm::getMaxApplicationAddress() const {
    return maximumApplicationAddress;
}

NTSTATUS Wddm::escape(D3DKMT_ESCAPE &escapeCommand) {
    escapeCommand.hAdapter = getAdapter();
    return getGdi()->escape(&escapeCommand);
};

PFND3DKMT_ESCAPE Wddm::getEscapeHandle() const {
    return getGdi()->escape;
}

VOID *Wddm::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) {
    if (DebugManager.flags.DoNotRegisterTrimCallback.get()) {
        return nullptr;
    }
    D3DKMT_REGISTERTRIMNOTIFICATION registerTrimNotification;
    registerTrimNotification.Callback = callback;
    registerTrimNotification.AdapterLuid = hwDeviceId->getAdapterLuid();
    registerTrimNotification.Context = &residencyController;
    registerTrimNotification.hDevice = device;

    NTSTATUS status = getGdi()->registerTrimNotification(&registerTrimNotification);
    if (status == STATUS_SUCCESS) {
        return registerTrimNotification.Handle;
    }
    return nullptr;
}

void Wddm::unregisterTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, VOID *trimCallbackHandle) {
    DEBUG_BREAK_IF(callback == nullptr);
    if (trimCallbackHandle == nullptr) {
        return;
    }
    D3DKMT_UNREGISTERTRIMNOTIFICATION unregisterTrimNotification;
    unregisterTrimNotification.Callback = callback;
    unregisterTrimNotification.Handle = trimCallbackHandle;

    NTSTATUS status = getGdi()->unregisterTrimNotification(&unregisterTrimNotification);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
}

void Wddm::releaseReservedAddress(void *reservedAddress) {
    if (reservedAddress) {
        auto status = virtualFree(reservedAddress, 0, MEM_RELEASE);
        DEBUG_BREAK_IF(!status);
    }
}

bool Wddm::reserveValidAddressRange(size_t size, void *&reservedMem) {
    reservedMem = virtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
    if (reservedMem == nullptr) {
        return false;
    } else if (minAddress > reinterpret_cast<uintptr_t>(reservedMem)) {
        StackVec<void *, 100> invalidAddrVector;
        invalidAddrVector.push_back(reservedMem);
        do {
            reservedMem = virtualAlloc(nullptr, size, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
            if (minAddress > reinterpret_cast<uintptr_t>(reservedMem) && reservedMem != nullptr) {
                invalidAddrVector.push_back(reservedMem);
            } else {
                break;
            }
        } while (1);
        for (auto &it : invalidAddrVector) {
            auto status = virtualFree(it, 0, MEM_RELEASE);
            DEBUG_BREAK_IF(!status);
        }
        if (reservedMem == nullptr) {
            return false;
        }
    }
    return true;
}

void *Wddm::virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type) {
    return virtualAllocFnc(inPtr, size, flags, type);
}

int Wddm::virtualFree(void *ptr, size_t size, unsigned long flags) {
    return virtualFreeFnc(ptr, size, flags);
}

long __stdcall notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    return notifyAubCaptureImpl(csrHandle, gfxAddress, gfxSize, allocate);
}
bool Wddm::configureDeviceAddressSpace() {
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
    deviceCallbacks.Adapter.KmtHandle = getAdapter();
    deviceCallbacks.hCsr = nullptr;
    deviceCallbacks.hDevice.KmtHandle = device;
    deviceCallbacks.PagingQueue = pagingQueue;
    deviceCallbacks.PagingFence = pagingQueueSyncObject;

    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnAllocate = getGdi()->createAllocation;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate = getGdi()->destroyAllocation;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMapGPUVA = getGdi()->mapGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMakeResident = getGdi()->makeResident;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEvict = getGdi()->evict;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnReserveGPUVA = getGdi()->reserveGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUpdateGPUVA = getGdi()->updateGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnWaitFromCpu = getGdi()->waitForSynchronizationObjectFromCpu;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnLock = getGdi()->lock2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUnLock = getGdi()->unlock2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEscape = getGdi()->escape;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnFreeGPUVA = getGdi()->freeGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture = notifyAubCapture;

    GMM_DEVICE_INFO deviceInfo{};
    deviceInfo.pGfxPartition = &gfxPartition;
    deviceInfo.pDeviceCb = &deviceCallbacks;
    if (!gmmMemory->setDeviceInfo(&deviceInfo)) {
        return false;
    }
    SYSTEM_INFO sysInfo;
    Wddm::getSystemInfo(&sysInfo);
    maximumApplicationAddress = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);
    auto productFamily = gfxPlatform->eProductFamily;
    if (!hardwareInfoTable[productFamily]) {
        return false;
    }
    auto svmSize = hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                       ? maximumApplicationAddress + 1u
                       : 0u;

    bool obtainMinAddress = gfxPlatform->eRenderCoreFamily == IGFX_GEN12LP_CORE;
    return gmmMemory->configureDevice(getAdapter(), device, getGdi()->escape, svmSize, featureTable->ftrL3IACoherency, minAddress, obtainMinAddress);
}

void Wddm::waitOnPagingFenceFromCpu() {
    perfLogStartWaitTime(residencyLogger.get(), currentPagingFenceValue);
    while (currentPagingFenceValue > *getPagingFenceAddress())
        perfLogResidencyEnteredWait(residencyLogger.get());

    perfLogResidencyWaitPagingeFenceLog(residencyLogger.get(), *getPagingFenceAddress());
}

void Wddm::setGmmInputArg(void *args) {
    reinterpret_cast<GMM_INIT_IN_ARGS *>(args)->stAdapterBDF = this->adapterBDF;
}

void Wddm::updatePagingFenceValue(uint64_t newPagingFenceValue) {
    interlockedMax(currentPagingFenceValue, newPagingFenceValue);
}

WddmVersion Wddm::getWddmVersion() {
    if (featureTable->ftrWddmHwQueues) {
        return WddmVersion::WDDM_2_3;
    } else {
        return WddmVersion::WDDM_2_0;
    }
}

uint32_t Wddm::getRequestedEUCount() const {
    DEBUG_BREAK_IF(!gtSystemInfo);
    // Always request even number od EUs
    return (gtSystemInfo.get()->EUCount / gtSystemInfo.get()->SubSliceCount) & (~1u);
};

void Wddm::createPagingFenceLogger() {
    if (DebugManager.flags.WddmResidencyLogger.get()) {
        residencyLogger = std::make_unique<WddmResidencyLogger>(device, pagingFenceAddress);
    }
}

} // namespace NEO
