/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/interlocked_max.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/dxcore_wrapper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/kmdaf_listener.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/sku_info/operations/windows/sku_info_receiver.h"
#include "shared/source/utilities/stackvec.h"

#include "gmm_memory.h"

namespace NEO {
extern Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory();
extern Wddm::DXCoreCreateAdapterFactoryFcn getDXCoreCreateAdapterFactory();
extern Wddm::GetSystemInfoFcn getGetSystemInfo();

Wddm::DXCoreCreateAdapterFactoryFcn Wddm::dXCoreCreateAdapterFactory = getDXCoreCreateAdapterFactory();
Wddm::CreateDXGIFactoryFcn Wddm::createDxgiFactory = getCreateDxgiFactory();
Wddm::GetSystemInfoFcn Wddm::getSystemInfo = getGetSystemInfo();

Wddm::Wddm(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment)
    : DriverModel(DriverModelType::WDDM), hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {
    UNRECOVERABLE_IF(!hwDeviceId);
    featureTable.reset(new FeatureTable());
    workaroundTable.reset(new WorkaroundTable());
    gtSystemInfo.reset(new GT_SYSTEM_INFO);
    gfxPlatform.reset(new PLATFORM);
    memset(gtSystemInfo.get(), 0, sizeof(*gtSystemInfo));
    memset(gfxPlatform.get(), 0, sizeof(*gfxPlatform));
    this->enablePreemptionRegValue = NEO::readEnablePreemptionRegKey();
    kmDafListener = std::unique_ptr<KmDafListener>(new KmDafListener);
    temporaryResources = std::make_unique<WddmResidentAllocationsContainer>(this);
    osMemory = OSMemory::create();
}

Wddm::~Wddm() {
    temporaryResources.reset();
    destroyPagingQueue();
    destroyDevice();
    UNRECOVERABLE_IF(temporaryResources.get())
}

bool Wddm::init() {
    if (!rootDeviceEnvironment.osInterface) {
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(this));
    }
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
    if (hwConfig->configureHwInfoWddm(hardwareInfo.get(), hardwareInfo.get(), nullptr)) {
        return false;
    }

    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*hardwareInfo);
    rootDeviceEnvironment.setHwInfo(hardwareInfo.get());
    rootDeviceEnvironment.initGmm();
    this->rootDeviceEnvironment.getGmmClientContext()->setHandleAllocator(this->hwDeviceId->getUmKmDataTranslator()->createGmmHandleAllocator());

    [[maybe_unused]] bool result = rootDeviceEnvironment.initAilConfiguration();
    DEBUG_BREAK_IF(!result);

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
    ADAPTER_INFO_KMD adapterInfo = {};
    D3DKMT_QUERYADAPTERINFO queryAdapterInfo = {};
    queryAdapterInfo.hAdapter = getAdapter();
    queryAdapterInfo.Type = KMTQAITYPE_UMDRIVERPRIVATE;

    if (hwDeviceId->getUmKmDataTranslator()->enabled()) {
        UmKmDataTempStorage<ADAPTER_INFO_KMD, 1> internalRepresentation(hwDeviceId->getUmKmDataTranslator()->getSizeForAdapterInfoInternalRepresentation());
        queryAdapterInfo.pPrivateDriverData = internalRepresentation.data();
        queryAdapterInfo.PrivateDriverDataSize = static_cast<uint32_t>(internalRepresentation.size());

        status = getGdi()->queryAdapterInfo(&queryAdapterInfo);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);

        if (status == STATUS_SUCCESS) {
            bool translated = hwDeviceId->getUmKmDataTranslator()->translateAdapterInfoFromInternalRepresentation(adapterInfo, internalRepresentation.data(), internalRepresentation.size());
            status = translated ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        }
    } else {
        queryAdapterInfo.pPrivateDriverData = &adapterInfo;
        queryAdapterInfo.PrivateDriverDataSize = sizeof(ADAPTER_INFO_KMD);

        status = getGdi()->queryAdapterInfo(&queryAdapterInfo);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }

    // translate
    if (status == STATUS_SUCCESS) {
        memcpy_s(gtSystemInfo.get(), sizeof(GT_SYSTEM_INFO), &adapterInfo.SystemInfo, sizeof(GT_SYSTEM_INFO));
        memcpy_s(gfxPlatform.get(), sizeof(PLATFORM), &adapterInfo.GfxPlatform, sizeof(PLATFORM));

        SkuInfoReceiver::receiveFtrTableFromAdapterInfo(featureTable.get(), &adapterInfo);
        SkuInfoReceiver::receiveWaTableFromAdapterInfo(workaroundTable.get(), &adapterInfo);

        memcpy_s(&gfxPartition, sizeof(gfxPartition), &adapterInfo.GfxPartition, sizeof(GMM_GFX_PARTITIONING));
        memcpy_s(&adapterBDF, sizeof(adapterBDF), &adapterInfo.stAdapterBDF, sizeof(ADAPTER_BDF));

        deviceRegistryPath = std::string(adapterInfo.DeviceRegistryPath, sizeof(adapterInfo.DeviceRegistryPath)).c_str();

        systemSharedMemory = adapterInfo.SystemSharedMemory;
        dedicatedVideoMemory = adapterInfo.DedicatedVideoMemory;
        maxRenderFrequency = adapterInfo.MaxRenderFreq;
        timestampFrequency = adapterInfo.GfxTimeStampFreq;
        instrumentationEnabled = adapterInfo.Caps.InstrumentationIsEnabled != 0;
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::createPagingQueue() {
    D3DKMT_CREATEPAGINGQUEUE createPagingQueue = {};
    createPagingQueue.hDevice = device;
    createPagingQueue.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;

    NTSTATUS status = getGdi()->createPagingQueue(&createPagingQueue);

    if (status == STATUS_SUCCESS) {
        pagingQueue = createPagingQueue.hPagingQueue;
        pagingQueueSyncObject = createPagingQueue.hSyncObject;
        pagingFenceAddress = reinterpret_cast<UINT64 *>(createPagingQueue.FenceValueCPUVirtualAddress);
        createPagingFenceLogger();
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::destroyPagingQueue() {
    D3DDDI_DESTROYPAGINGQUEUE destroyPagingQueue = {};
    if (pagingQueue) {
        destroyPagingQueue.hPagingQueue = pagingQueue;

        [[maybe_unused]] NTSTATUS status = getGdi()->destroyPagingQueue(&destroyPagingQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        pagingQueue = 0;
    }
    return true;
}

bool Wddm::createDevice(PreemptionMode preemptionMode) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATEDEVICE createDevice = {};
    if (hwDeviceId) {
        createDevice.hAdapter = getAdapter();
        createDevice.Flags.LegacyMode = FALSE;
        if (preemptionMode >= PreemptionMode::MidBatch) {
            createDevice.Flags.DisableGpuTimeout = getEnablePreemptionRegValue();
        }

        status = getGdi()->createDevice(&createDevice);
        if (status == STATUS_SUCCESS) {
            device = createDevice.hDevice;
        }
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::destroyDevice() {
    D3DKMT_DESTROYDEVICE destroyDevice = {};
    if (device) {
        destroyDevice.hDevice = device;

        [[maybe_unused]] NTSTATUS status = getGdi()->destroyDevice(&destroyDevice);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        device = 0;
    }
    return true;
}

bool validDriverStorePath(OsEnvironmentWin &osEnvironment, D3DKMT_HANDLE adapter) {
    D3DKMT_QUERYADAPTERINFO queryAdapterInfo = {};
    ADAPTER_INFO_KMD adapterInfo = {};
    queryAdapterInfo.hAdapter = adapter;
    queryAdapterInfo.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    queryAdapterInfo.pPrivateDriverData = &adapterInfo;
    queryAdapterInfo.PrivateDriverDataSize = sizeof(ADAPTER_INFO_KMD);

    auto status = osEnvironment.gdi->queryAdapterInfo(&queryAdapterInfo);

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF("queryAdapterInfo failed");
        return false;
    }

    std::string deviceRegistryPath = adapterInfo.DeviceRegistryPath;
    return isCompatibleDriverStore(std::move(deviceRegistryPath));
}

std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid) {
    D3DKMT_OPENADAPTERFROMLUID openAdapterData = {};
    openAdapterData.AdapterLuid = adapterLuid;
    auto status = osEnvironment.gdi->openAdapterFromLuid(&openAdapterData);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF("openAdapterFromLuid failed");
        return nullptr;
    }

    std::unique_ptr<UmKmDataTranslator> umKmDataTranslator = createUmKmDataTranslator(*osEnvironment.gdi, openAdapterData.hAdapter);
    if (false == umKmDataTranslator->enabled()) {
        if (false == validDriverStorePath(osEnvironment, openAdapterData.hAdapter)) {
            return nullptr;
        }
    }

    D3DKMT_QUERYADAPTERINFO queryAdapterInfo = {};
    D3DKMT_ADAPTERTYPE queryAdapterType = {};
    queryAdapterInfo.hAdapter = openAdapterData.hAdapter;
    queryAdapterInfo.Type = KMTQAITYPE_ADAPTERTYPE;
    queryAdapterInfo.pPrivateDriverData = &queryAdapterType;
    queryAdapterInfo.PrivateDriverDataSize = sizeof(queryAdapterType);
    status = osEnvironment.gdi->queryAdapterInfo(&queryAdapterInfo);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF("queryAdapterInfo failed");
        return nullptr;
    }
    if (0 == queryAdapterType.RenderSupported) {
        return nullptr;
    }

    return std::make_unique<HwDeviceIdWddm>(openAdapterData.hAdapter, adapterLuid, &osEnvironment, std::move(umKmDataTranslator));
}

std::vector<std::unique_ptr<HwDeviceId>> Wddm::discoverDevices(ExecutionEnvironment &executionEnvironment) {

    auto osEnvironment = new OsEnvironmentWin();
    auto gdi = osEnvironment->gdi.get();
    executionEnvironment.osEnvironment.reset(osEnvironment);

    if (!gdi->isInitialized()) {
        return {};
    }

    auto adapterFactory = AdapterFactory::create(Wddm::dXCoreCreateAdapterFactory, Wddm::createDxgiFactory);

    if (false == adapterFactory->isSupported()) {
        return {};
    }

    size_t numRootDevices = 0u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }

    std::vector<std::unique_ptr<HwDeviceId>> hwDeviceIds;
    do {
        if (false == adapterFactory->createSnapshotOfAvailableAdapters()) {
            return hwDeviceIds;
        }

        auto adapterCount = adapterFactory->getNumAdaptersInSnapshot();
        for (uint32_t i = 0; i < adapterCount; ++i) {
            AdapterFactory::AdapterDesc adapterDesc;
            if (false == adapterFactory->getAdapterDesc(i, adapterDesc)) {
                DEBUG_BREAK_IF(true);
                continue;
            }

            if (adapterDesc.type == AdapterFactory::AdapterDesc::Type::NotHardware) {
                continue;
            }

            if (false == canUseAdapterBasedOnDriverDesc(adapterDesc.driverDescription.c_str())) {
                continue;
            }

            if (false == isAllowedDeviceId(adapterDesc.deviceId)) {
                continue;
            }

            auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterDesc.luid);
            if (hwDeviceId) {
                hwDeviceIds.push_back(std::unique_ptr<HwDeviceId>(hwDeviceId.release()));
            }

            if (!hwDeviceIds.empty() && hwDeviceIds.size() == numRootDevices) {
                break;
            }
        }
        if (hwDeviceIds.empty()) {
            break;
        }
    } while (hwDeviceIds.size() < numRootDevices);

    return hwDeviceIds;
}

bool Wddm::evict(const D3DKMT_HANDLE *handleList, uint32_t numOfHandles, uint64_t &sizeToTrim, bool evictNeeded) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_EVICT evict = {};
    evict.AllocationList = handleList;
    evict.hDevice = device;
    evict.NumAllocations = numOfHandles;
    evict.NumBytesToTrim = 0;
    evict.Flags.EvictOnlyIfNecessary = evictNeeded ? 0 : 1;

    status = getGdi()->evict(&evict);

    sizeToTrim = evict.NumBytesToTrim;

    kmDafListener->notifyEvict(featureTable->flags.ftrKmdDaf, getAdapter(), device, handleList, numOfHandles, getGdi()->escape);

    return status == STATUS_SUCCESS;
}

bool Wddm::makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DDDI_MAKERESIDENT makeResident = {};
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

    kmDafListener->notifyMakeResident(featureTable->flags.ftrKmdDaf, getAdapter(), device, handles, count, getGdi()->escape);

    return success;
}

bool Wddm::mapGpuVirtualAddress(AllocationStorageData *allocationStorageData) {
    auto osHandle = static_cast<OsHandleWin *>(allocationStorageData->osHandleStorage);
    return mapGpuVirtualAddress(osHandle->gmm,
                                osHandle->handle,
                                0u, MemoryConstants::maxSvmAddress, castToUint64(allocationStorageData->cpuPtr),
                                osHandle->gpuPtr);
}

bool Wddm::mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr) {
    D3DDDI_MAPGPUVIRTUALADDRESS mapGPUVA = {};
    D3DDDIGPUVIRTUALADDRESS_PROTECTION_TYPE protectionType = {};
    protectionType.Write = TRUE;

    uint64_t size = gmm->gmmResourceInfo->getSizeAllocation();

    mapGPUVA.hPagingQueue = pagingQueue;
    mapGPUVA.hAllocation = handle;
    mapGPUVA.Protection = protectionType;

    mapGPUVA.SizeInPages = size / MemoryConstants::pageSize;
    mapGPUVA.OffsetInPages = 0;

    mapGPUVA.BaseAddress = preferredAddress;
    mapGPUVA.MinimumAddress = minimumAddress;
    mapGPUVA.MaximumAddress = maximumAddress;

    applyAdditionalMapGPUVAFields(mapGPUVA, gmm);

    NTSTATUS status = getGdi()->mapGpuVirtualAddress(&mapGPUVA);

    auto gmmHelper = gmm->getGmmHelper();
    gpuPtr = gmmHelper->canonize(mapGPUVA.VirtualAddress);

    if (status == STATUS_PENDING) {
        updatePagingFenceValue(mapGPUVA.PagingFenceValue);
        status = STATUS_SUCCESS;
    }

    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    kmDafListener->notifyMapGpuVA(featureTable->flags.ftrKmdDaf, getAdapter(), device, handle, mapGPUVA.VirtualAddress, getGdi()->escape);
    bool ret = true;
    if (gmm->isCompressionEnabled && HwInfoConfig::get(gfxPlatform->eProductFamily)->isPageTableManagerSupported(*rootDeviceEnvironment.getHardwareInfo())) {
        for (auto engine : rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines()) {
            if (engine.commandStreamReceiver->pageTableManager.get()) {
                ret &= engine.commandStreamReceiver->pageTableManager->updateAuxTable(gpuPtr, gmm, true);
            }
        }
    }

    return ret;
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
    D3DKMT_FREEGPUVIRTUALADDRESS freeGpuva = {};
    freeGpuva.hAdapter = getAdapter();
    freeGpuva.BaseAddress = rootDeviceEnvironment.getGmmHelper()->decanonize(gpuPtr);
    freeGpuva.Size = size;
    status = getGdi()->freeGpuVirtualAddress(&freeGpuva);
    gpuPtr = static_cast<D3DGPU_VIRTUAL_ADDRESS>(0);

    kmDafListener->notifyUnmapGpuVA(featureTable->flags.ftrKmdDaf, getAdapter(), device, freeGpuva.BaseAddress, getGdi()->escape);

    return status == STATUS_SUCCESS;
}

NTSTATUS Wddm::createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, uint64_t *outSharedHandle) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DDDI_ALLOCATIONINFO2 allocationInfo = {};
    D3DKMT_CREATEALLOCATION createAllocation = {};

    if (gmm == nullptr)
        return false;

    allocationInfo.pSystemMem = alignedCpuPtr;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.PrivateDriverDataSize = static_cast<uint32_t>(gmm->gmmResourceInfo->peekHandleSize());
    createAllocation.NumAllocations = 1;
    createAllocation.Flags.CreateShared = outSharedHandle ? TRUE : FALSE;
    createAllocation.Flags.NtSecuritySharing = outSharedHandle ? TRUE : FALSE;
    createAllocation.Flags.CreateResource = outSharedHandle || alignedCpuPtr ? TRUE : FALSE;
    createAllocation.pAllocationInfo2 = &allocationInfo;
    createAllocation.hDevice = device;

    status = getGdi()->createAllocation2(&createAllocation);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return status;
    }

    gmm->gmmResourceInfo->refreshHandle();

    outHandle = allocationInfo.hAllocation;
    outResourceHandle = createAllocation.hResource;
    if (outSharedHandle) {
        HANDLE ntSharedHandle = NULL;
        status = this->createNTHandle(&outResourceHandle, &ntSharedHandle);
        if (status != STATUS_SUCCESS) {
            DEBUG_BREAK_IF(true);
            [[maybe_unused]] auto destroyStatus = this->destroyAllocations(&outHandle, 1, outResourceHandle);
            outHandle = NULL_HANDLE;
            outResourceHandle = NULL_HANDLE;
            DEBUG_BREAK_IF(destroyStatus != STATUS_SUCCESS);
            return status;
        }
        *outSharedHandle = castToUint64(ntSharedHandle);
    }
    kmDafListener->notifyWriteTarget(featureTable->flags.ftrKmdDaf, getAdapter(), device, outHandle, getGdi()->escape);

    return status;
}

bool Wddm::createAllocation(const Gmm *gmm, D3DKMT_HANDLE &outHandle) {
    D3DKMT_HANDLE outResourceHandle = NULL_HANDLE;
    uint64_t *outSharedHandle = nullptr;
    auto result = this->createAllocation(nullptr, gmm, outHandle, outResourceHandle, outSharedHandle);
    return STATUS_SUCCESS == result;
}

bool Wddm::setAllocationPriority(const D3DKMT_HANDLE *handles, uint32_t allocationCount, uint32_t priority) {
    D3DKMT_SETALLOCATIONPRIORITY setAllocationPriority = {};

    StackVec<UINT, 4> priorities{};

    priorities.resize(allocationCount);
    for (auto i = 0u; i < allocationCount; i++) {
        priorities[i] = priority;
    }

    setAllocationPriority.hDevice = device;
    setAllocationPriority.AllocationCount = allocationCount;
    setAllocationPriority.hResource = NULL_HANDLE;
    setAllocationPriority.phAllocationList = handles;
    setAllocationPriority.pPriorities = priorities.data();

    auto status = getGdi()->setAllocationPriority(&setAllocationPriority);

    DEBUG_BREAK_IF(STATUS_SUCCESS != status);

    return STATUS_SUCCESS == status;
}

NTSTATUS Wddm::createAllocationsAndMapGpuVa(OsHandleStorage &osHandles) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DDDI_ALLOCATIONINFO2 allocationInfo[maxFragmentsCount] = {};
    D3DKMT_CREATEALLOCATION createAllocation = {};

    auto allocationCount = 0;
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (!osHandles.fragmentStorageData[i].osHandleStorage) {
            break;
        }

        auto osHandle = static_cast<OsHandleWin *>(osHandles.fragmentStorageData[i].osHandleStorage);
        if ((osHandle->handle == (D3DKMT_HANDLE)0) && (osHandles.fragmentStorageData[i].fragmentSize)) {
            allocationInfo[allocationCount].pPrivateDriverData = osHandle->gmm->gmmResourceInfo->peekHandle();
            [[maybe_unused]] auto pSysMem = osHandles.fragmentStorageData[i].cpuPtr;
            [[maybe_unused]] auto pSysMemFromGmm = osHandle->gmm->gmmResourceInfo->getSystemMemPointer();
            DEBUG_BREAK_IF(pSysMemFromGmm != pSysMem);
            allocationInfo[allocationCount].pSystemMem = osHandles.fragmentStorageData[i].cpuPtr;
            allocationInfo[allocationCount].PrivateDriverDataSize = static_cast<unsigned int>(osHandle->gmm->gmmResourceInfo->peekHandleSize());
            allocationCount++;
        }
    }
    if (allocationCount == 0) {
        return STATUS_SUCCESS;
    }

    createAllocation.hGlobalShare = 0;
    createAllocation.PrivateRuntimeDataSize = 0;
    createAllocation.PrivateDriverDataSize = 0;
    createAllocation.Flags.Reserved = 0;
    createAllocation.NumAllocations = allocationCount;
    createAllocation.pPrivateRuntimeData = nullptr;
    createAllocation.pPrivateDriverData = nullptr;
    createAllocation.Flags.NonSecure = FALSE;
    createAllocation.Flags.CreateShared = FALSE;
    createAllocation.Flags.RestrictSharedAccess = FALSE;
    createAllocation.Flags.CreateResource = FALSE;
    createAllocation.pAllocationInfo2 = allocationInfo;
    createAllocation.hDevice = device;

    while (status == STATUS_UNSUCCESSFUL) {
        status = getGdi()->createAllocation2(&createAllocation);

        if (status != STATUS_SUCCESS) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s status: %d", __FUNCTION__, status);
            DEBUG_BREAK_IF(status != STATUS_GRAPHICS_NO_VIDEO_MEMORY);
            break;
        }
        auto allocationIndex = 0;
        for (int i = 0; i < allocationCount; i++) {
            while (static_cast<OsHandleWin *>(osHandles.fragmentStorageData[allocationIndex].osHandleStorage)->handle) {
                allocationIndex++;
            }
            static_cast<OsHandleWin *>(osHandles.fragmentStorageData[allocationIndex].osHandleStorage)->handle = allocationInfo[i].hAllocation;
            bool success = mapGpuVirtualAddress(&osHandles.fragmentStorageData[allocationIndex]);

            if (!success) {
                osHandles.fragmentStorageData[allocationIndex].freeTheFragment = true;
                PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s mapGpuVirtualAddress: %d", __FUNCTION__, success);
                DEBUG_BREAK_IF(true);
                return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
            }

            allocationIndex++;

            kmDafListener->notifyWriteTarget(featureTable->flags.ftrKmdDaf, getAdapter(), device, allocationInfo[i].hAllocation, getGdi()->escape);
        }

        status = STATUS_SUCCESS;
    }
    return status;
}

bool Wddm::destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
    if ((0U == allocationCount) && (0U == resourceHandle)) {
        return true;
    }
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_DESTROYALLOCATION2 destroyAllocation = {};
    DEBUG_BREAK_IF(!(allocationCount <= 1 || resourceHandle == 0));

    destroyAllocation.hDevice = device;
    destroyAllocation.hResource = resourceHandle;
    destroyAllocation.phAllocationList = handles;
    destroyAllocation.AllocationCount = allocationCount;

    destroyAllocation.Flags.AssumeNotInUse = 1;

    status = getGdi()->destroyAllocation2(&destroyAllocation);

    return status == STATUS_SUCCESS;
}
bool Wddm::verifySharedHandle(D3DKMT_HANDLE osHandle) {
    D3DKMT_QUERYRESOURCEINFO queryResourceInfo = {};
    queryResourceInfo.hDevice = device;
    queryResourceInfo.hGlobalShare = osHandle;
    auto status = getGdi()->queryResourceInfo(&queryResourceInfo);
    return status == STATUS_SUCCESS;
}

bool Wddm::openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFO queryResourceInfo = {};
    queryResourceInfo.hDevice = device;
    queryResourceInfo.hGlobalShare = handle;
    [[maybe_unused]] auto status = getGdi()->queryResourceInfo(&queryResourceInfo);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    if (queryResourceInfo.NumAllocations == 0) {
        return false;
    }

    std::unique_ptr<char[]> allocPrivateData(new char[queryResourceInfo.TotalPrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateData(new char[queryResourceInfo.ResourcePrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateRuntimeData(new char[queryResourceInfo.PrivateRuntimeDataSize]);
    std::unique_ptr<D3DDDI_OPENALLOCATIONINFO2[]> allocationInfo(new D3DDDI_OPENALLOCATIONINFO2[queryResourceInfo.NumAllocations]);

    D3DKMT_OPENRESOURCE openResource = {};

    openResource.hDevice = device;
    openResource.hGlobalShare = handle;
    openResource.NumAllocations = queryResourceInfo.NumAllocations;
    openResource.pOpenAllocationInfo2 = allocationInfo.get();
    openResource.pTotalPrivateDriverDataBuffer = allocPrivateData.get();
    openResource.TotalPrivateDriverDataBufferSize = queryResourceInfo.TotalPrivateDriverDataSize;
    openResource.pResourcePrivateDriverData = resPrivateData.get();
    openResource.ResourcePrivateDriverDataSize = queryResourceInfo.ResourcePrivateDriverDataSize;
    openResource.pPrivateRuntimeData = resPrivateRuntimeData.get();
    openResource.PrivateRuntimeDataSize = queryResourceInfo.PrivateRuntimeDataSize;

    status = getGdi()->openResource(&openResource);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    alloc->setDefaultHandle(allocationInfo[0].hAllocation);
    alloc->resourceHandle = openResource.hResource;

    auto resourceInfo = const_cast<void *>(allocationInfo[0].pPrivateDriverData);
    alloc->setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), static_cast<GMM_RESOURCE_INFO *>(resourceInfo)));

    return true;
}

bool Wddm::verifyNTHandle(HANDLE handle) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE queryResourceInfoFromNtHandle = {};
    queryResourceInfoFromNtHandle.hDevice = device;
    queryResourceInfoFromNtHandle.hNtHandle = handle;
    auto status = getGdi()->queryResourceInfoFromNtHandle(&queryResourceInfoFromNtHandle);
    return status == STATUS_SUCCESS;
}

bool Wddm::openNTHandle(HANDLE handle, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE queryResourceInfoFromNtHandle = {};
    queryResourceInfoFromNtHandle.hDevice = device;
    queryResourceInfoFromNtHandle.hNtHandle = handle;
    [[maybe_unused]] auto status = getGdi()->queryResourceInfoFromNtHandle(&queryResourceInfoFromNtHandle);
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

    auto resourceInfo = const_cast<void *>(allocationInfo2[0].pPrivateDriverData);

    alloc->setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), static_cast<GMM_RESOURCE_INFO *>(resourceInfo), hwDeviceId->getUmKmDataTranslator()->enabled()));

    alloc->setDefaultHandle(allocationInfo2[0].hAllocation);
    alloc->resourceHandle = openResourceFromNtHandle.hResource;

    return true;
}

void *Wddm::lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) {

    if (applyMakeResidentPriorToLock) {
        temporaryResources->makeResidentResource(handle, size);
    }

    D3DKMT_LOCK2 lock2 = {};

    lock2.hAllocation = handle;
    lock2.hDevice = this->device;

    [[maybe_unused]] NTSTATUS status = getGdi()->lock2(&lock2);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    kmDafLock(handle);
    return lock2.pData;
}

void Wddm::unlockResource(const D3DKMT_HANDLE &handle) {
    D3DKMT_UNLOCK2 unlock2 = {};

    unlock2.hAllocation = handle;
    unlock2.hDevice = this->device;

    [[maybe_unused]] NTSTATUS status = getGdi()->unlock2(&unlock2);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    kmDafListener->notifyUnlock(featureTable->flags.ftrKmdDaf, getAdapter(), device, &handle, 1, getGdi()->escape);
}

void Wddm::kmDafLock(D3DKMT_HANDLE handle) {
    kmDafListener->notifyLock(featureTable->flags.ftrKmdDaf, getAdapter(), device, handle, 0, getGdi()->escape);
}

bool Wddm::setLowPriorityContextParam(D3DKMT_HANDLE contextHandle) {
    D3DKMT_SETCONTEXTSCHEDULINGPRIORITY contextPriority = {};

    contextPriority.hContext = contextHandle;
    contextPriority.Priority = 1;

    if (DebugManager.flags.ForceWddmLowPriorityContextValue.get() != -1) {
        contextPriority.Priority = static_cast<INT>(DebugManager.flags.ForceWddmLowPriorityContextValue.get());
    }

    auto status = getGdi()->setSchedulingPriority(&contextPriority);

    PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stdout,
                       "\nSet scheduling priority for Wddm context. Status: :%lu, context handle: %u, priority: %d \n",
                       status, contextHandle, contextPriority.Priority);

    return (status == STATUS_SUCCESS);
}

bool Wddm::createContext(OsContextWin &osContext) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATECONTEXTVIRTUAL createContext = {};

    CREATECONTEXT_PVTDATA privateData = initPrivateData(osContext);

    privateData.ProcessID = NEO::getPid();
    privateData.pHwContextId = &hwContextId;
    privateData.NoRingFlushes = DebugManager.flags.UseNoRingFlushesKmdMode.get();

    auto &rootDeviceEnvironment = this->getRootDeviceEnvironment();
    applyAdditionalContextFlags(privateData, osContext, *rootDeviceEnvironment.getHardwareInfo());

    createContext.EngineAffinity = 0;
    createContext.Flags.NullRendering = static_cast<UINT>(DebugManager.flags.EnableNullHardware.get());
    createContext.Flags.HwQueueSupported = wddmInterface->hwQueuesSupported();

    if (osContext.getPreemptionMode() >= PreemptionMode::MidBatch) {
        createContext.Flags.DisableGpuTimeout = getEnablePreemptionRegValue();
    }

    UmKmDataTempStorage<CREATECONTEXT_PVTDATA> internalRepresentation;
    if (hwDeviceId->getUmKmDataTranslator()->enabled()) {
        internalRepresentation.resize(hwDeviceId->getUmKmDataTranslator()->getSizeForCreateContextDataInternalRepresentation());
        hwDeviceId->getUmKmDataTranslator()->translateCreateContextDataToInternalRepresentation(internalRepresentation.data(), internalRepresentation.size(), privateData);
        createContext.pPrivateDriverData = internalRepresentation.data();
        createContext.PrivateDriverDataSize = static_cast<uint32_t>(internalRepresentation.size());
    } else {
        createContext.PrivateDriverDataSize = sizeof(privateData);
        createContext.pPrivateDriverData = &privateData;
    }
    createContext.NodeOrdinal = WddmEngineMapper::engineNodeMap(osContext.getEngineType());
    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::L0) {
        createContext.ClientHint = D3DKMT_CLIENTHINT_ONEAPI_LEVEL0;
    } else {
        createContext.ClientHint = D3DKMT_CLIENTHINT_OPENCL;
    }
    createContext.hDevice = device;

    status = getGdi()->createContext(&createContext);
    osContext.setWddmContextHandle(createContext.hContext);

    PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stdout,
                       "\nCreated Wddm context. Status: :%lu, engine: %u, contextId: %u, deviceBitfield: %lu \n",
                       status, osContext.getEngineType(), osContext.getContextId(), osContext.getDeviceBitfield().to_ulong());

    if (status != STATUS_SUCCESS) {
        return false;
    }

    if (osContext.isLowPriority()) {
        return setLowPriorityContextParam(osContext.getWddmContextHandle());
    }

    return true;
}

bool Wddm::destroyContext(D3DKMT_HANDLE context) {
    D3DKMT_DESTROYCONTEXT destroyContext = {};
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (context != static_cast<D3DKMT_HANDLE>(0)) {
        destroyContext.hContext = context;
        status = getGdi()->destroyContext(&destroyContext);
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    bool status = false;
    if (currentPagingFenceValue > *pagingFenceAddress && !waitOnGPU(submitArguments.contextHandle)) {
        return false;
    }
    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", submitArguments.monitorFence->currentFenceValue);

    if (DebugManager.flags.PrintDeviceAndEngineIdOnSubmission.get()) {
        printf("%u: Wddm Submission with context handle %u and HwQueue handle %u\n", SysCalls::getProcessId(), submitArguments.contextHandle, submitArguments.hwQueueHandle);
    }

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
    D3DKMT_GETDEVICESTATE GetDevState = {};
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

unsigned int Wddm::getEnablePreemptionRegValue() {
    return enablePreemptionRegValue;
}

bool Wddm::waitOnGPU(D3DKMT_HANDLE context) {
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU waitOnGpu = {};

    waitOnGpu.hContext = context;
    waitOnGpu.ObjectCount = 1;
    waitOnGpu.ObjectHandleArray = &pagingQueueSyncObject;
    uint64_t localPagingFenceValue = currentPagingFenceValue;

    waitOnGpu.MonitoredFenceValueArray = &localPagingFenceValue;
    NTSTATUS status = getGdi()->waitForSynchronizationObjectFromGpu(&waitOnGpu);

    return status == STATUS_SUCCESS;
}

bool Wddm::waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence) {
    NTSTATUS status = STATUS_SUCCESS;

    if (!skipResourceCleanup() && lastFenceValue > *monitoredFence.cpuAddress) {
        D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu = {};
        waitFromCpu.ObjectCount = 1;
        waitFromCpu.ObjectHandleArray = &monitoredFence.fenceHandle;
        waitFromCpu.FenceValueArray = &lastFenceValue;
        waitFromCpu.hDevice = device;
        waitFromCpu.hAsyncEvent = NULL_HANDLE;
        status = getGdi()->waitForSynchronizationObjectFromCpu(&waitFromCpu);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }

    return status == STATUS_SUCCESS;
}

bool Wddm::isGpuHangDetected(OsContext &osContext) {
    const auto osContextWin = static_cast<OsContextWin *>(&osContext);
    const auto &monitoredFence = osContextWin->getResidencyController().getMonitoredFence();
    bool hangDetected = monitoredFence.cpuAddress && *monitoredFence.cpuAddress == gpuHangIndication;

    PRINT_DEBUG_STRING(hangDetected && DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "ERROR: GPU HANG detected!\n");

    return hangDetected;
}

void Wddm::initGfxPartition(GfxPartition &outGfxPartition, uint32_t rootDeviceIndex, size_t numRootDevices, bool useExternalFrontWindowPool) const {
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
        if (useExternalFrontWindowPool && HeapAssigner::heapTypeExternalWithFrontWindowPool(heap)) {
            outGfxPartition.heapInitExternalWithFrontWindow(heap, gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base,
                                                            gfxPartition.Heap32[static_cast<uint32_t>(heap)].Limit - gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base + 1);
            size_t externalFrontWindowSize = GfxPartition::externalFrontWindowPoolSize;
            outGfxPartition.heapInitExternalWithFrontWindow(HeapAssigner::mapExternalWindowIndex(heap), outGfxPartition.heapAllocate(heap, externalFrontWindowSize),
                                                            externalFrontWindowSize);
        } else if (HeapAssigner::isInternalHeap(heap)) {
            auto baseAddress = gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base >= minAddress ? gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base : minAddress;

            outGfxPartition.heapInitWithFrontWindow(heap, baseAddress,
                                                    gfxPartition.Heap32[static_cast<uint32_t>(heap)].Limit - baseAddress + 1,
                                                    GfxPartition::internalFrontWindowPoolSize);
            outGfxPartition.heapInitFrontWindow(HeapAssigner::mapInternalWindowIndex(heap), baseAddress, GfxPartition::internalFrontWindowPoolSize);
        } else {
            outGfxPartition.heapInit(heap, gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base,
                                     gfxPartition.Heap32[static_cast<uint32_t>(heap)].Limit - gfxPartition.Heap32[static_cast<uint32_t>(heap)].Base + 1);
        }
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

bool Wddm::verifyAdapterLuid(LUID adapterLuid) const {
    return adapterLuid.HighPart == hwDeviceId->getAdapterLuid().HighPart && adapterLuid.LowPart == hwDeviceId->getAdapterLuid().LowPart;
}

LUID Wddm::getAdapterLuid() const {
    return hwDeviceId->getAdapterLuid();
}

bool Wddm::isShutdownInProgress() {
    return NEO::isShutdownInProgress();
}

void Wddm::releaseReservedAddress(void *reservedAddress) {
    if (reservedAddress) {
        this->virtualFree(reservedAddress, 0);
    }
}

bool Wddm::reserveValidAddressRange(size_t size, void *&reservedMem) {
    reservedMem = this->virtualAlloc(nullptr, size, false);
    if (reservedMem == nullptr) {
        return false;
    } else if (minAddress > reinterpret_cast<uintptr_t>(reservedMem)) {
        StackVec<void *, 100> invalidAddrVector;
        invalidAddrVector.push_back(reservedMem);
        do {
            reservedMem = this->virtualAlloc(nullptr, size, true);
            if (minAddress > reinterpret_cast<uintptr_t>(reservedMem) && reservedMem != nullptr) {
                invalidAddrVector.push_back(reservedMem);
            } else {
                break;
            }
        } while (1);
        for (auto &it : invalidAddrVector) {
            this->virtualFree(it, 0);
        }
        if (reservedMem == nullptr) {
            return false;
        }
    }
    return true;
}

void *Wddm::virtualAlloc(void *inPtr, size_t size, bool topDownHint) {
    return osMemory->osReserveCpuAddressRange(inPtr, size, topDownHint);
}

void Wddm::virtualFree(void *ptr, size_t size) {
    osMemory->osReleaseCpuAddressRange(ptr, size);
}

void Wddm::waitOnPagingFenceFromCpu() {
    perfLogStartWaitTime(residencyLogger.get(), currentPagingFenceValue);
    while (currentPagingFenceValue > *getPagingFenceAddress())
        perfLogResidencyEnteredWait(residencyLogger.get());

    perfLogResidencyWaitPagingeFenceLog(residencyLogger.get(), *getPagingFenceAddress());
}

void Wddm::updatePagingFenceValue(uint64_t newPagingFenceValue) {
    interlockedMax(currentPagingFenceValue, newPagingFenceValue);
}

WddmVersion Wddm::getWddmVersion() {
    if (featureTable->flags.ftrWddmHwQueues) {
        return WddmVersion::WDDM_2_3;
    } else {
        return WddmVersion::WDDM_2_0;
    }
}

uint32_t Wddm::getRequestedEUCount() const {
    DEBUG_BREAK_IF(!gtSystemInfo);
    // Always request even number od EUs
    return (gtSystemInfo->EUCount / gtSystemInfo->SubSliceCount) & (~1u);
};

void Wddm::createPagingFenceLogger() {
    if (DebugManager.flags.WddmResidencyLogger.get()) {
        residencyLogger = std::make_unique<WddmResidencyLogger>(device, pagingFenceAddress);
    }
}

PhysicalDevicePciBusInfo Wddm::getPciBusInfo() const {
    if (adapterBDF.Data == std::numeric_limits<uint32_t>::max()) {
        return PhysicalDevicePciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue);
    }
    return PhysicalDevicePciBusInfo(0, adapterBDF.Bus, adapterBDF.Device, adapterBDF.Function);
}

PhyicalDevicePciSpeedInfo Wddm::getPciSpeedInfo() const {
    PhyicalDevicePciSpeedInfo speedInfo{};
    return speedInfo;
}

} // namespace NEO
