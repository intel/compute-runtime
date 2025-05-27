/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/mt_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/dxcore_wrapper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/kmdaf_listener.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_temp_storage.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/sku_info/operations/windows/sku_info_receiver.h"

#include "gmm_memory.h"

namespace NEO {
extern Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory();
extern Wddm::DXCoreCreateAdapterFactoryFcn getDXCoreCreateAdapterFactory();
extern Wddm::GetSystemInfoFcn getGetSystemInfo();

Wddm::DXCoreCreateAdapterFactoryFcn Wddm::dXCoreCreateAdapterFactory = getDXCoreCreateAdapterFactory();
Wddm::CreateDXGIFactoryFcn Wddm::createDxgiFactory = getCreateDxgiFactory();
Wddm::GetSystemInfoFcn Wddm::getSystemInfo = getGetSystemInfo();

Wddm::Wddm(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment)
    : DriverModel(DriverModelType::wddm), hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {
    UNRECOVERABLE_IF(!hwDeviceId);
    featureTable.reset(new FeatureTable());
    workaroundTable.reset(new WorkaroundTable());
    gtSystemInfo.reset(new GT_SYSTEM_INFO);
    gfxPlatform.reset(new PLATFORM_KMD);
    gfxFeatureTable.reset(new SKU_FEATURE_TABLE_KMD);
    gfxWorkaroundTable.reset(new WA_TABLE_KMD);
    memset(gtSystemInfo.get(), 0, sizeof(*gtSystemInfo));
    memset(gfxPlatform.get(), 0, sizeof(*gfxPlatform));
    this->enablePreemptionRegValue = NEO::readEnablePreemptionRegKey();
    kmDafListener = std::unique_ptr<KmDafListener>(new KmDafListener);
    temporaryResources = std::make_unique<WddmResidentAllocationsContainer>(this);
    osMemory = OSMemory::create();
    bool forceCheck = false;
#if _DEBUG
    forceCheck = true;
#endif
    checkDeviceState = (debugManager.flags.EnableDeviceStateVerification.get() != -1) ? debugManager.flags.EnableDeviceStateVerification.get() : forceCheck;
    pagingFenceDelayTime = debugManager.flags.WddmPagingFenceCpuWaitDelayTime.get();
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

    auto hardwareInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    hardwareInfo->platform = *gfxPlatform;
    hardwareInfo->featureTable = *featureTable;
    hardwareInfo->workaroundTable = *workaroundTable;
    hardwareInfo->gtSystemInfo = *gtSystemInfo;
    hardwareInfo->capabilityTable = hardwareInfoTable[productFamily]->capabilityTable;
    hardwareInfo->capabilityTable.maxRenderFrequency = maxRenderFrequency;
    hardwareInfo->capabilityTable.instrumentationEnabled =
        (hardwareInfo->capabilityTable.instrumentationEnabled && instrumentationEnabled);

    rootDeviceEnvironment.initProductHelper();
    rootDeviceEnvironment.initCompilerProductHelper();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    productHelper.adjustPlatformForProductFamily(hardwareInfo);
    rootDeviceEnvironment.initApiGfxCoreHelper();
    rootDeviceEnvironment.initGfxCoreHelper();
    rootDeviceEnvironment.initializeGfxCoreHelperFromProductHelper();
    rootDeviceEnvironment.initializeGfxCoreHelperFromHwInfo();
    rootDeviceEnvironment.initAilConfigurationHelper();
    if (false == rootDeviceEnvironment.initAilConfiguration()) {
        return false;
    }

    populateIpVersion(*hardwareInfo);
    rootDeviceEnvironment.initReleaseHelper();
    rootDeviceEnvironment.setRcsExposure();

    if (productHelper.configureHwInfoWddm(hardwareInfo, hardwareInfo, rootDeviceEnvironment)) {
        return false;
    }
    rootDeviceEnvironment.initWaitUtils();
    setPlatformSupportEvictIfNecessaryFlag(productHelper);

    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*hardwareInfo);

    rootDeviceEnvironment.initGmm();
    this->rootDeviceEnvironment.getGmmClientContext()->setHandleAllocator(this->hwDeviceId->getUmKmDataTranslator()->createGmmHandleAllocator());

    auto wddmVersion = getWddmVersion();
    if (WddmVersion::wddm32 == wddmVersion) {
        wddmInterface = std::make_unique<WddmInterface32>(*this);
    } else if (WddmVersion::wddm23 == wddmVersion) {
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

    if (!buildTopologyMapping()) {
        return false;
    }

    setProcessPowerThrottling();
    setThreadPriority();

    return configureDeviceAddressSpace();
}

void Wddm::setPlatformSupportEvictIfNecessaryFlag(const ProductHelper &productHelper) {
    platformSupportsEvictIfNecessary = productHelper.isEvictionIfNecessaryFlagSupported();
    int32_t overridePlatformSupportsEvictIfNecessary =
        debugManager.flags.PlaformSupportEvictIfNecessaryFlag.get();
    if (overridePlatformSupportsEvictIfNecessary != -1) {
        platformSupportsEvictIfNecessary = !!overridePlatformSupportsEvictIfNecessary;
    }
    forceEvictOnlyIfNecessary = debugManager.flags.ForceEvictOnlyIfNecessaryFlag.get();
}

bool Wddm::buildTopologyMapping() {

    TopologyMapping mapping;
    if (!translateTopologyInfo(mapping)) {
        PRINT_DEBUGGER_ERROR_LOG("translateTopologyInfo Failed\n", "");
        return false;
    }
    this->topologyMap[0] = mapping;

    return true;
}

bool Wddm::translateTopologyInfo(TopologyMapping &mapping) {
    int sliceCount = 0;
    int subSliceCount = 0;
    int euCount = 0;
    std::vector<int> sliceIndices;
    auto gtSystemInfo = rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo;
    sliceIndices.reserve(gtSystemInfo.SliceCount);
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(*hwInfo);

    for (uint32_t x = 0; x < std::max(highestEnabledSlice, hwInfo->gtSystemInfo.MaxSlicesSupported); x++) {
        if (!gtSystemInfo.SliceInfo[x].Enabled) {
            continue;
        }
        sliceIndices.push_back(x);
        sliceCount++;

        std::vector<int> subSliceIndices;
        subSliceIndices.reserve((gtSystemInfo.SliceInfo[x].DualSubSliceEnabledCount) * GT_MAX_SUBSLICE_PER_DSS);

        // subSliceIndex is used to track the index number of subslices from all SS or DSS in this slice
        int subSliceIndex = -1;
        bool dssEnabled = false;
        for (uint32_t dss = 0; dss < GT_MAX_DUALSUBSLICE_PER_SLICE; dss++) {
            if (!gtSystemInfo.SliceInfo[x].DSSInfo[dss].Enabled) {
                subSliceIndex += 2;
                continue;
            }

            for (uint32_t y = 0; y < GT_MAX_SUBSLICE_PER_DSS; y++) {
                subSliceIndex++;
                if (!gtSystemInfo.SliceInfo[x].DSSInfo[dss].SubSlice[y].Enabled) {
                    continue;
                }
                dssEnabled = true;
                subSliceCount++;
                subSliceIndices.push_back(subSliceIndex);

                euCount += gtSystemInfo.SliceInfo[x].DSSInfo[dss].SubSlice[y].EuEnabledCount;
            }
        }

        if (!dssEnabled) {
            subSliceIndex = -1;
            for (uint32_t sss = 0; sss < GT_MAX_SUBSLICE_PER_SLICE; sss++) {
                subSliceIndex++;
                if (!gtSystemInfo.SliceInfo[x].SubSliceInfo[sss].Enabled) {
                    continue;
                }

                subSliceCount++;
                subSliceIndices.push_back(subSliceIndex);

                euCount += gtSystemInfo.SliceInfo[x].SubSliceInfo[sss].EuEnabledCount;
            }
        }

        // single slice available
        if (sliceCount == 1) {
            mapping.subsliceIndices = std::move(subSliceIndices);
        }
    }

    if (sliceIndices.size()) {
        mapping.sliceIndices = std::move(sliceIndices);
    }

    if (sliceCount != 1) {
        mapping.subsliceIndices.clear();
    }
    PRINT_DEBUGGER_INFO_LOG("Topology Mapping: sliceCount=%d subSliceCount=%d euCount=%d\n", sliceCount, subSliceCount, euCount);
    return (sliceCount && subSliceCount && euCount);
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
        memcpy_s(gfxPlatform.get(), sizeof(PLATFORM_KMD), &adapterInfo.GfxPlatform, sizeof(PLATFORM_KMD));
        memcpy_s(gfxFeatureTable.get(), sizeof(SKU_FEATURE_TABLE_KMD), &adapterInfo.SkuTable, sizeof(SKU_FEATURE_TABLE_KMD));
        memcpy_s(gfxWorkaroundTable.get(), sizeof(WA_TABLE_KMD), &adapterInfo.WaTable, sizeof(WA_TABLE_KMD));

        if (debugManager.flags.ForceDeviceId.get() != "unk") {
            gfxPlatform->usDeviceID = static_cast<unsigned short>(std::stoi(debugManager.flags.ForceDeviceId.get(), nullptr, 16));
        }

        SkuInfoReceiver::receiveFtrTableFromAdapterInfo(featureTable.get(), &adapterInfo);
        SkuInfoReceiver::receiveWaTableFromAdapterInfo(workaroundTable.get(), &adapterInfo);

        memcpy_s(&gfxPartition, sizeof(gfxPartition), &adapterInfo.GfxPartition, sizeof(GMM_GFX_PARTITIONING));
        memcpy_s(&adapterBDF, sizeof(adapterBDF), &adapterInfo.stAdapterBDF, sizeof(ADAPTER_BDF));
        memcpy_s(segmentId, sizeof(segmentId), adapterInfo.SegmentId, sizeof(adapterInfo.SegmentId));

        deviceRegistryPath = std::string(adapterInfo.DeviceRegistryPath, sizeof(adapterInfo.DeviceRegistryPath)).c_str();

        systemSharedMemory = adapterInfo.SystemSharedMemory;
        dedicatedVideoMemory = adapterInfo.DedicatedVideoMemory;
        lmemBarSize = adapterInfo.LMemBarSize;
        maxRenderFrequency = adapterInfo.MaxRenderFreq;
        timestampFrequency = adapterInfo.GfxTimeStampFreq;
        instrumentationEnabled = adapterInfo.Caps.InstrumentationIsEnabled != 0;

        populateAdditionalAdapterInfoOptions(adapterInfo);
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

std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinal) {
    D3DKMT_OPENADAPTERFROMLUID openAdapterData = {};
    openAdapterData.AdapterLuid = adapterLuid;
    auto status = osEnvironment.gdi->openAdapterFromLuid(&openAdapterData);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF("openAdapterFromLuid failed");
        return nullptr;
    }

    std::unique_ptr<UmKmDataTranslator> umKmDataTranslator = createUmKmDataTranslator(*osEnvironment.gdi, openAdapterData.hAdapter);
    if (false == umKmDataTranslator->enabled() && !debugManager.flags.DoNotValidateDriverPath.get()) {
        if (false == validDriverStorePath(osEnvironment, openAdapterData.hAdapter)) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Driver path is not a valid DriverStore path. Try running with debug key: DoNotValidateDriverPath=1.\n");
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
    uint32_t adapterNodeMask = 1 << adapterNodeOrdinal;
    return std::make_unique<HwDeviceIdWddm>(openAdapterData.hAdapter, adapterLuid, adapterNodeMask, &osEnvironment, std::move(umKmDataTranslator));
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
    if (debugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = debugManager.flags.CreateMultipleRootDevices.get();
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

            if (adapterDesc.type == AdapterFactory::AdapterDesc::Type::notHardware) {
                continue;
            }

            if (false == canUseAdapterBasedOnDriverDesc(adapterDesc.driverDescription.c_str())) {
                continue;
            }

            if (false == isAllowedDeviceId(adapterDesc.deviceId)) {
                continue;
            }

            auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterDesc.luid, i);
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
    if (numOfHandles == 0) {
        return true;
    }
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_EVICT evict = {};
    evict.AllocationList = handleList;
    evict.hDevice = device;
    evict.NumAllocations = numOfHandles;
    evict.NumBytesToTrim = 0;
    evict.Flags.EvictOnlyIfNecessary = adjustEvictNeededParameter(evictNeeded) ? 0 : 1;

    status = getGdi()->evict(&evict);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

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
    makeResident.Flags.MustSucceed = 0;

    status = getGdi()->makeResident(&makeResident);
    if (status == STATUS_PENDING) {
        perfLogResidencyMakeResident(residencyLogger.get(), true, makeResident.PagingFenceValue);
        updatePagingFenceValue(makeResident.PagingFenceValue);
        success = true;
    } else if (status == STATUS_SUCCESS) {
        perfLogResidencyMakeResident(residencyLogger.get(), false, makeResident.PagingFenceValue);
        success = true;
    } else {
        DEBUG_BREAK_IF(cantTrimFurther);
        DEBUG_BREAK_IF(makeResident.NumAllocations != 0u && makeResident.NumAllocations != count);
        perfLogResidencyTrimRequired(residencyLogger.get(), makeResident.NumBytesToTrim);
        if (numberOfBytesToTrim != nullptr) {
            *numberOfBytesToTrim = makeResident.NumBytesToTrim;
        }
        return false;
    }

    kmDafListener->notifyMakeResident(featureTable->flags.ftrKmdDaf, getAdapter(), device, handles, count, getGdi()->escape);
    this->setNewResourceBoundToPageTable();

    return success;
}

bool Wddm::mapGpuVirtualAddress(AllocationStorageData *allocationStorageData) {
    auto osHandle = static_cast<OsHandleWin *>(allocationStorageData->osHandleStorage);
    return mapGpuVirtualAddress(osHandle->gmm,
                                osHandle->handle,
                                0u, MemoryConstants::maxSvmAddress, castToUint64(allocationStorageData->cpuPtr),
                                osHandle->gpuPtr, AllocationType::externalHostPtr);
}

bool Wddm::mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, AllocationType type) {
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

    applyAdditionalMapGPUVAFields(mapGPUVA, gmm, type);

    MapGpuVirtualAddressGmm gmmMapGpuVa = {&mapGPUVA, gmm->gmmResourceInfo.get(), &gpuPtr, getGdi()};
    auto status = gmm->getGmmHelper()->getClientContext()->mapGpuVirtualAddress(&gmmMapGpuVa);

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
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (gmm->isCompressionEnabled() && productHelper.isPageTableManagerSupported(*rootDeviceEnvironment.getHardwareInfo())) {
        this->forEachContextWithinWddm([&](const EngineControl &engine) {
            if (engine.commandStreamReceiver->pageTableManager.get()) {
                ret &= engine.commandStreamReceiver->pageTableManager->updateAuxTable(gpuPtr, gmm, true);
            }
        });
    }

    return ret;
}

NTSTATUS Wddm::reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS baseAddress,
                                        D3DGPU_VIRTUAL_ADDRESS minimumAddress,
                                        D3DGPU_VIRTUAL_ADDRESS maximumAddress,
                                        D3DGPU_SIZE_T size,
                                        D3DGPU_VIRTUAL_ADDRESS *reservedAddress) {
    UNRECOVERABLE_IF(size % MemoryConstants::pageSize64k);
    D3DDDI_RESERVEGPUVIRTUALADDRESS reserveGpuVirtualAddress = {};
    reserveGpuVirtualAddress.BaseAddress = baseAddress;
    reserveGpuVirtualAddress.MinimumAddress = minimumAddress;
    reserveGpuVirtualAddress.MaximumAddress = maximumAddress;
    reserveGpuVirtualAddress.hPagingQueue = this->pagingQueue;
    reserveGpuVirtualAddress.Size = size;

    NTSTATUS status = getGdi()->reserveGpuVirtualAddress(&reserveGpuVirtualAddress);
    *reservedAddress = reserveGpuVirtualAddress.VirtualAddress;
    return status;
}

uint64_t Wddm::freeGmmGpuVirtualAddress(Gmm *gmm, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) {
    uint64_t status = STATUS_SUCCESS;
    FreeGpuVirtualAddressGmm freeGpuva = {getAdapter(), rootDeviceEnvironment.getGmmHelper()->decanonize(gpuPtr), size, gmm->gmmResourceInfo.get(), getGdi()};
    status = gmm->getGmmHelper()->getClientContext()->freeGpuVirtualAddress(&freeGpuva);
    return status;
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

bool Wddm::isReadOnlyFlagFallbackAvailable(const D3DKMT_CREATEALLOCATION &createAllocation) const {
    return isReadOnlyFlagFallbackSupported() && createAllocation.pAllocationInfo2->pSystemMem && !createAllocation.Flags.ReadOnly;
}

NTSTATUS Wddm::createAllocation(const void *cpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, uint64_t *outSharedHandle) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DDDI_ALLOCATIONINFO2 allocationInfo = {};
    D3DKMT_CREATEALLOCATION createAllocation = {};

    if (gmm == nullptr)
        return false;

    allocationInfo.pSystemMem = gmm->gmmResourceInfo->getSystemMemPointer();
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.PrivateDriverDataSize = static_cast<uint32_t>(gmm->gmmResourceInfo->peekHandleSize());
    createAllocation.NumAllocations = 1;
    createAllocation.Flags.CreateShared = outSharedHandle ? TRUE : FALSE;
    createAllocation.Flags.NtSecuritySharing = outSharedHandle ? TRUE : FALSE;
    createAllocation.Flags.CreateResource = outSharedHandle ? TRUE : FALSE;
    createAllocation.Flags.ReadOnly = getReadOnlyFlagValue(cpuPtr);
    createAllocation.pAllocationInfo2 = &allocationInfo;
    createAllocation.hDevice = device;

    bool allowNotZeroForCompressed = false;
    if (NEO::debugManager.flags.AllowNotZeroForCompressedOnWddm.get() != -1) {
        allowNotZeroForCompressed = !!NEO::debugManager.flags.AllowNotZeroForCompressedOnWddm.get();
    }
    if (allowNotZeroForCompressed && gmm->isCompressionEnabled()) {
        createAllocation.Flags.AllowNotZeroed = 1;
    }

    status = getGdi()->createAllocation2(&createAllocation);
    if (status != STATUS_SUCCESS && isReadOnlyFlagFallbackAvailable(createAllocation)) {
        createAllocation.Flags.ReadOnly = TRUE;
        status = getGdi()->createAllocation2(&createAllocation);
    }
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
    createAllocation.Flags.ReadOnly = getReadOnlyFlagValue(allocationInfo[0].pSystemMem);
    createAllocation.pAllocationInfo2 = allocationInfo;
    createAllocation.hDevice = device;

    while (status == STATUS_UNSUCCESSFUL) {
        status = getGdi()->createAllocation2(&createAllocation);
        if (status != STATUS_SUCCESS && isReadOnlyFlagFallbackAvailable(createAllocation)) {
            createAllocation.Flags.ReadOnly = TRUE;
            status = getGdi()->createAllocation2(&createAllocation);
        }

        if (status != STATUS_SUCCESS) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s status: %d", __FUNCTION__, status);
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
                PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s mapGpuVirtualAddress: %d", __FUNCTION__, success);
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
    destroyAllocation.Flags.AssumeNotInUse = debugManager.flags.SetAssumeNotInUse.get();

    bool destroyViaGmm = true;

    if (debugManager.flags.DestroyAllocationsViaGmm.get() != -1) {
        destroyViaGmm = debugManager.flags.DestroyAllocationsViaGmm.get();
    }

    if (destroyViaGmm) {
        DeallocateGmm deallocateGmm{&destroyAllocation, getGdi()};
        status = static_cast<NTSTATUS>(this->rootDeviceEnvironment.getGmmClientContext()->deallocate2(&deallocateGmm));
    } else {
        status = getGdi()->destroyAllocation2(&destroyAllocation);
    }

    return status == STATUS_SUCCESS;
}
bool Wddm::verifySharedHandle(D3DKMT_HANDLE osHandle) {
    D3DKMT_QUERYRESOURCEINFO queryResourceInfo = {};
    queryResourceInfo.hDevice = device;
    queryResourceInfo.hGlobalShare = osHandle;
    auto status = getGdi()->queryResourceInfo(&queryResourceInfo);
    return status == STATUS_SUCCESS;
}

bool Wddm::openSharedHandle(const MemoryManager::OsHandleData &osHandleData, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFO queryResourceInfo = {};
    queryResourceInfo.hDevice = device;
    queryResourceInfo.hGlobalShare = osHandleData.handle;
    [[maybe_unused]] auto status = getGdi()->queryResourceInfo(&queryResourceInfo);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    if (queryResourceInfo.NumAllocations == 0) {
        return false;
    }

    std::unique_ptr<char[]> allocPrivateData(new char[queryResourceInfo.TotalPrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateData(new char[queryResourceInfo.ResourcePrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateRuntimeData(new char[queryResourceInfo.PrivateRuntimeDataSize]);
    std::unique_ptr<D3DDDI_OPENALLOCATIONINFO[]> allocationInfo(new D3DDDI_OPENALLOCATIONINFO[queryResourceInfo.NumAllocations]);

    D3DKMT_OPENRESOURCE openResource = {};

    openResource.hDevice = device;
    openResource.hGlobalShare = osHandleData.handle;
    openResource.NumAllocations = queryResourceInfo.NumAllocations;
    openResource.pOpenAllocationInfo = allocationInfo.get();
    openResource.pTotalPrivateDriverDataBuffer = allocPrivateData.get();
    openResource.TotalPrivateDriverDataBufferSize = queryResourceInfo.TotalPrivateDriverDataSize;
    openResource.pResourcePrivateDriverData = resPrivateData.get();
    openResource.ResourcePrivateDriverDataSize = queryResourceInfo.ResourcePrivateDriverDataSize;
    openResource.pPrivateRuntimeData = resPrivateRuntimeData.get();
    openResource.PrivateRuntimeDataSize = queryResourceInfo.PrivateRuntimeDataSize;

    status = getGdi()->openResource(&openResource);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    auto allocationInfoIndex = osHandleData.arrayIndex < queryResourceInfo.NumAllocations ? osHandleData.arrayIndex : 0;
    alloc->setDefaultHandle(allocationInfo[allocationInfoIndex].hAllocation);
    alloc->setResourceHandle(openResource.hResource);

    auto resourceInfo = const_cast<void *>(allocationInfo[allocationInfoIndex].pPrivateDriverData);
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

bool Wddm::openNTHandle(const MemoryManager::OsHandleData &osHandleData, WddmAllocation *alloc) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE queryResourceInfoFromNtHandle = {};
    queryResourceInfoFromNtHandle.hDevice = device;
    queryResourceInfoFromNtHandle.hNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle));
    [[maybe_unused]] auto status = getGdi()->queryResourceInfoFromNtHandle(&queryResourceInfoFromNtHandle);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);

    if (queryResourceInfoFromNtHandle.NumAllocations == 0) {
        return false;
    }

    std::unique_ptr<char[]> allocPrivateData(new char[queryResourceInfoFromNtHandle.TotalPrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateData(new char[queryResourceInfoFromNtHandle.ResourcePrivateDriverDataSize]);
    std::unique_ptr<char[]> resPrivateRuntimeData(new char[queryResourceInfoFromNtHandle.PrivateRuntimeDataSize]);
    std::unique_ptr<D3DDDI_OPENALLOCATIONINFO2[]> allocationInfo2(new D3DDDI_OPENALLOCATIONINFO2[queryResourceInfoFromNtHandle.NumAllocations]);

    D3DKMT_OPENRESOURCEFROMNTHANDLE openResourceFromNtHandle = {};

    openResourceFromNtHandle.hDevice = device;
    openResourceFromNtHandle.hNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle));
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

    auto allocationInfoIndex = osHandleData.arrayIndex < queryResourceInfoFromNtHandle.NumAllocations ? osHandleData.arrayIndex : 0;
    auto resourceInfo = const_cast<void *>(allocationInfo2[allocationInfoIndex].pPrivateDriverData);

    alloc->setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), static_cast<GMM_RESOURCE_INFO *>(resourceInfo), hwDeviceId->getUmKmDataTranslator()->enabled()));

    alloc->setDefaultHandle(allocationInfo2[allocationInfoIndex].hAllocation);
    alloc->setResourceHandle(openResourceFromNtHandle.hResource);

    return true;
}

void *Wddm::lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) {

    if (applyMakeResidentPriorToLock) {
        temporaryResources->makeResidentResource(handle, size);
    }

    D3DKMT_LOCK2 lock2 = {};

    lock2.hAllocation = handle;
    lock2.hDevice = this->device;

    NTSTATUS status = getGdi()->lock2(&lock2);
    if (status != STATUS_SUCCESS) {
        return nullptr;
    }

    kmDafLock(handle);
    return lock2.pData;
}

void Wddm::unlockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock) {
    D3DKMT_UNLOCK2 unlock2 = {};

    unlock2.hAllocation = handle;
    unlock2.hDevice = this->device;

    NTSTATUS status = getGdi()->unlock2(&unlock2);

    if (applyMakeResidentPriorToLock) {
        this->temporaryResources->evictResource(handle);
    }

    if (status != STATUS_SUCCESS) {
        return;
    }

    kmDafListener->notifyUnlock(featureTable->flags.ftrKmdDaf, getAdapter(), device, &handle, 1, getGdi()->escape);
}

void Wddm::kmDafLock(D3DKMT_HANDLE handle) {
    kmDafListener->notifyLock(featureTable->flags.ftrKmdDaf, getAdapter(), device, handle, 0, getGdi()->escape);
}

bool Wddm::isKmDafEnabled() const {
    return featureTable->flags.ftrKmdDaf;
}

bool Wddm::setLowPriorityContextParam(D3DKMT_HANDLE contextHandle) {
    D3DKMT_SETCONTEXTSCHEDULINGPRIORITY contextPriority = {};

    contextPriority.hContext = contextHandle;
    contextPriority.Priority = -7;

    if (debugManager.flags.ForceWddmLowPriorityContextValue.get() != -1) {
        contextPriority.Priority = static_cast<INT>(debugManager.flags.ForceWddmLowPriorityContextValue.get());
    }

    auto status = getGdi()->setSchedulingPriority(&contextPriority);

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stdout,
                       "\nSet scheduling priority for Wddm context. Status: :%lu, context handle: %u, priority: %d \n",
                       status, contextHandle, contextPriority.Priority);

    return (status == STATUS_SUCCESS);
}

bool Wddm::createContext(OsContextWin &osContext) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CREATECONTEXTVIRTUAL createContext = {};

    CREATECONTEXT_PVTDATA privateData = initPrivateData(osContext);

    privateData.ProcessID = NEO::SysCalls::getProcessId();
    privateData.pHwContextId = &hwContextId;
    privateData.NoRingFlushes = debugManager.flags.UseNoRingFlushesKmdMode.get();
    privateData.DummyPageBackingEnabled = debugManager.flags.DummyPageBackingEnabled.get();

    applyAdditionalContextFlags(privateData, osContext);

    createContext.EngineAffinity = 0;
    createContext.Flags.NullRendering = static_cast<UINT>(debugManager.flags.EnableNullHardware.get());
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

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stdout,
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

    if (debugManager.flags.PrintDeviceAndEngineIdOnSubmission.get()) {
        printf("%u: Wddm Submission with context handle %u and HwQueue handle %u\n", SysCalls::getProcessId(), submitArguments.contextHandle, submitArguments.hwQueueHandle);
    }

    status = getDeviceState();
    if (!status) {
        return false;
    }
    status = wddmInterface->submit(commandBuffer, size, commandHeader, submitArguments);
    if (status) {
        submitArguments.monitorFence->lastSubmittedFence = submitArguments.monitorFence->currentFenceValue;
        submitArguments.monitorFence->currentFenceValue++;
    } else if (debugManager.flags.EnableDeviceStateVerificationAfterFailedSubmission.get() == 1) {
        getDeviceState();
    }

    return status;
}

bool Wddm::getDeviceExecutionState(D3DKMT_DEVICESTATE_TYPE stateType, void *privateData) {
    D3DKMT_GETDEVICESTATE getDevState = {};
    NTSTATUS status = STATUS_SUCCESS;

    getDevState.hDevice = device;
    getDevState.StateType = stateType;

    status = getGdi()->getDeviceState(&getDevState);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    if (status != STATUS_SUCCESS) {
        return false;
    }

    if (stateType == D3DKMT_DEVICESTATE_PAGE_FAULT) {
        if (privateData != nullptr) {
            *reinterpret_cast<D3DKMT_DEVICEPAGEFAULT_STATE *>(privateData) = getDevState.PageFaultState;
        }
        return true;
    } else if (stateType == D3DKMT_DEVICESTATE_EXECUTION) {
        if (privateData != nullptr) {
            *reinterpret_cast<D3DKMT_DEVICEEXECUTION_STATE *>(privateData) = getDevState.ExecutionState;
        }
        return true;
    } else {
        return false;
    }
}

bool Wddm::getDeviceState() {
    if (checkDeviceState) {
        D3DKMT_DEVICEEXECUTION_STATE executionState = D3DKMT_DEVICEEXECUTION_ACTIVE;
        auto status = getDeviceExecutionState(D3DKMT_DEVICESTATE_EXECUTION, &executionState);
        if (status) {
            if (executionState == D3DKMT_DEVICEEXECUTION_ERROR_OUTOFMEMORY) {
                PRINT_DEBUG_STRING(true, stderr, "Device execution error, out of memory %d\n", executionState);
            } else if (executionState == D3DKMT_DEVICEEXECUTION_ERROR_DMAPAGEFAULT) {
                PRINT_DEBUG_STRING(true, stderr, "Device execution error, page fault\n", executionState);
                D3DKMT_DEVICEPAGEFAULT_STATE pageFaultState = {};
                status = getDeviceExecutionState(D3DKMT_DEVICESTATE_PAGE_FAULT, &pageFaultState);
                if (status) {
                    PRINT_DEBUG_STRING(true, stderr, "faulted gpuva 0x%" PRIx64 ", ", pageFaultState.FaultedVirtualAddress);
                    PRINT_DEBUG_STRING(true, stderr, "pipeline stage %d, bind table entry %u, flags 0x%x, error code(is device) %u, error code %u\n",
                                       pageFaultState.FaultedPipelineStage, pageFaultState.FaultedBindTableEntry, pageFaultState.PageFaultFlags,
                                       pageFaultState.FaultErrorCode.IsDeviceSpecificCode, pageFaultState.FaultErrorCode.IsDeviceSpecificCode ? pageFaultState.FaultErrorCode.DeviceSpecificCode : static_cast<UINT>(pageFaultState.FaultErrorCode.GeneralErrorCode));

                    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Page fault detected at address = ", std::hex, pageFaultState.FaultedVirtualAddress);
                }
            } else if (executionState != D3DKMT_DEVICEEXECUTION_ACTIVE) {
                PRINT_DEBUG_STRING(true, stderr, "Device execution error %d\n", executionState);
            }
            DEBUG_BREAK_IF(executionState != D3DKMT_DEVICEEXECUTION_ACTIVE);
            return executionState == D3DKMT_DEVICEEXECUTION_ACTIVE;
        }
        return false;
    }
    return true;
}

unsigned int Wddm::getEnablePreemptionRegValue() {
    return enablePreemptionRegValue;
}

bool Wddm::waitOnGPU(D3DKMT_HANDLE context) {
    perfLogStartWaitTime(residencyLogger.get(), currentPagingFenceValue);

    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU waitOnGpu = {};

    waitOnGpu.hContext = context;
    waitOnGpu.ObjectCount = 1;
    waitOnGpu.ObjectHandleArray = &pagingQueueSyncObject;
    uint64_t localPagingFenceValue = currentPagingFenceValue;

    waitOnGpu.MonitoredFenceValueArray = &localPagingFenceValue;
    NTSTATUS status = getGdi()->waitForSynchronizationObjectFromGpu(&waitOnGpu);

    perfLogResidencyWaitPagingeFenceLog(residencyLogger.get(), *getPagingFenceAddress(), true);
    return status == STATUS_SUCCESS;
}

bool Wddm::waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence, bool busyWait) {
    NTSTATUS status = STATUS_SUCCESS;

    if (!skipResourceCleanup() && lastFenceValue > *monitoredFence.cpuAddress) {
        CommandStreamReceiver *csr = nullptr;
        this->forEachContextWithinWddm([&monitoredFence, &csr](const EngineControl &engine) {
            auto &contextMonitoredFence = static_cast<OsContextWin *>(engine.osContext)->getResidencyController().getMonitoredFence();
            if (contextMonitoredFence.cpuAddress == monitoredFence.cpuAddress) {
                csr = engine.commandStreamReceiver;
            }
        });

        if (csr != nullptr && lastFenceValue > monitoredFence.lastSubmittedFence) {
            auto lock = csr->obtainUniqueOwnership();
            csr->flushMonitorFence(false);
        }

        if (busyWait) {
            constexpr int64_t timeout = 20;
            int64_t timeDiff = 0u;
            auto waitStartTime = std::chrono::high_resolution_clock::now();
            while (lastFenceValue > *monitoredFence.cpuAddress && timeDiff < timeout) {
                auto currentTime = std::chrono::high_resolution_clock::now();
                timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - waitStartTime).count();
            }
        }

        if (lastFenceValue > *monitoredFence.cpuAddress) {
            if (csr != nullptr) {
                // Flush monitor fence to emit KMD interrupt.
                auto lock = csr->obtainUniqueOwnership();
                csr->flushMonitorFence(true);
            }
            D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu = {};
            waitFromCpu.ObjectCount = 1;
            waitFromCpu.ObjectHandleArray = &monitoredFence.fenceHandle;
            waitFromCpu.FenceValueArray = &lastFenceValue;
            waitFromCpu.hDevice = device;
            waitFromCpu.hAsyncEvent = NULL_HANDLE;
            status = getGdi()->waitForSynchronizationObjectFromCpu(&waitFromCpu);
            DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        }
    }
    return status == STATUS_SUCCESS;
}

bool Wddm::isGpuHangDetected(OsContext &osContext) {
    const auto osContextWin = static_cast<OsContextWin *>(&osContext);
    const auto &monitoredFence = osContextWin->getResidencyController().getMonitoredFence();
    bool hangDetected = monitoredFence.cpuAddress && *monitoredFence.cpuAddress == gpuHangIndication;

    PRINT_DEBUG_STRING(hangDetected && debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "ERROR: GPU HANG detected!\n");

    return hangDetected;
}

void Wddm::initGfxPartition(GfxPartition &outGfxPartition, uint32_t rootDeviceIndex, size_t numRootDevices, bool useExternalFrontWindowPool) const {
    if (gfxPartition.SVM.Limit != 0) {
        outGfxPartition.heapInit(HeapIndex::heapSvm, gfxPartition.SVM.Base, gfxPartition.SVM.Limit - gfxPartition.SVM.Base + 1);
    } else if (is32bit) {
        outGfxPartition.heapInit(HeapIndex::heapSvm, 0x0ull, 4 * MemoryConstants::gigaByte);
    }

    outGfxPartition.heapInit(HeapIndex::heapStandard, gfxPartition.Standard.Base, gfxPartition.Standard.Limit - gfxPartition.Standard.Base + 1);

    // Split HEAP_STANDARD64K among root devices
    auto gfxStandard64KBSize = alignDown((gfxPartition.Standard64KB.Limit - gfxPartition.Standard64KB.Base + 1) / numRootDevices, GfxPartition::heapGranularity);
    outGfxPartition.heapInit(HeapIndex::heapStandard64KB, gfxPartition.Standard64KB.Base + rootDeviceIndex * gfxStandard64KBSize, gfxStandard64KBSize);

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

void Wddm::waitOnPagingFenceFromCpu(uint64_t pagingFenceValueToWait, bool isKmdWaitNeeded) {
    perfLogStartWaitTime(residencyLogger.get(), pagingFenceValueToWait);
    if (pagingFenceValueToWait > *getPagingFenceAddress()) {
        if (isKmdWaitNeeded) {
            perfLogResidencyEnteredWait(residencyLogger.get());
            D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu = {};
            waitFromCpu.ObjectCount = 1;
            waitFromCpu.ObjectHandleArray = &pagingQueueSyncObject;
            waitFromCpu.FenceValueArray = &pagingFenceValueToWait;
            waitFromCpu.hDevice = device;
            waitFromCpu.hAsyncEvent = NULL_HANDLE;
            [[maybe_unused]] auto status = getGdi()->waitForSynchronizationObjectFromCpu(&waitFromCpu);
            DEBUG_BREAK_IF(status != STATUS_SUCCESS);
        } else {
            while (pagingFenceValueToWait > *getPagingFenceAddress()) {
                perfLogResidencyEnteredWait(residencyLogger.get());
            }
        }
        if (pagingFenceDelayTime > 0) {
            delayPagingFenceFromCpu(pagingFenceDelayTime);
        }
    }

    perfLogResidencyWaitPagingeFenceLog(residencyLogger.get(), *getPagingFenceAddress(), false);
}

void Wddm::delayPagingFenceFromCpu(int64_t delayTime) {
    int64_t timeDiff = 0u;
    auto waitStartTime = std::chrono::high_resolution_clock::now();
    while (timeDiff < delayTime) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - waitStartTime).count();
    }
}

void Wddm::updatePagingFenceValue(uint64_t newPagingFenceValue) {
    NEO::MultiThreadHelpers::interlockedMax(currentPagingFenceValue, newPagingFenceValue);
}

WddmVersion Wddm::getWddmVersion() {
    if (featureTable->flags.ftrWddmHwQueues) {
        if (isNativeFenceAvailable()) {
            return WddmVersion::wddm32;
        }
        return WddmVersion::wddm23;
    } else {
        return WddmVersion::wddm20;
    }
}

uint32_t Wddm::getRequestedEUCount() const {
    DEBUG_BREAK_IF(!gtSystemInfo);
    // Always request even number od EUs
    return (gtSystemInfo->EUCount / gtSystemInfo->SubSliceCount) & (~1u);
};

void Wddm::createPagingFenceLogger() {
    if (debugManager.flags.WddmResidencyLogger.get()) {
        residencyLogger = std::make_unique<WddmResidencyLogger>(device, pagingFenceAddress, debugManager.flags.WddmResidencyLoggerOutputDirectory.get());
    }
}

PhysicalDevicePciBusInfo Wddm::getPciBusInfo() const {
    if (adapterBDF.Data == std::numeric_limits<uint32_t>::max()) {
        return PhysicalDevicePciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue);
    }
    return PhysicalDevicePciBusInfo(0, adapterBDF.Bus, adapterBDF.Device, adapterBDF.Function);
}

PhysicalDevicePciSpeedInfo Wddm::getPciSpeedInfo() const {
    PhysicalDevicePciSpeedInfo speedInfo{};
    return speedInfo;
}

void Wddm::populateIpVersion(HardwareInfo &hwInfo) {
    hwInfo.ipVersion.value = gfxPlatform->sRenderBlockID.Value;
    if (hwInfo.ipVersion.value == 0) {
        auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
        hwInfo.ipVersion.value = compilerProductHelper.getHwIpVersion(hwInfo);
    }
}

void Wddm::setNewResourceBoundToPageTable() {
    if (!this->rootDeviceEnvironment.getProductHelper().isTlbFlushRequired()) {
        return;
    }
    this->forEachContextWithinWddm([](const EngineControl &engine) { engine.osContext->setNewResourceBound(); });
}
} // namespace NEO
