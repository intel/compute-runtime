/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gmm_memory.h"

struct MockWddmLinux : NEO::Wddm {
    MockWddmLinux(std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceId, NEO::RootDeviceEnvironment &rootDeviceEnvironment)
        : NEO::Wddm(std::move(hwDeviceId), rootDeviceEnvironment) {
        constexpr uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte; // 4GB
        gfxPartition.SVM.Base = 0;
        gfxPartition.SVM.Limit = MemoryConstants::max64BitAppAddress + 1;
        decltype(gfxPartition.SVM) *precHeap = &gfxPartition.SVM;
        for (auto heap : {&gfxPartition.Heap32[0], &gfxPartition.Heap32[1], &gfxPartition.Heap32[2], &gfxPartition.Heap32[3], &gfxPartition.Standard, &gfxPartition.Standard64KB}) {
            heap->Base = precHeap->Limit;
            heap->Limit = heap->Base + gfxHeap32Size;
            precHeap = heap;
        }
        UNRECOVERABLE_IF(!isAligned<NEO::GfxPartition::heapGranularity>(precHeap->Limit));
    }

    bool reserveValidAddressRange(size_t size, void *&reservedMem) override {
        if (validAddressBase + size > MemoryConstants::max64BitAppAddress) {
            validAddressRangeReservations.push_back(std::make_tuple(size, reservedMem, false));
            return false;
        }
        reservedMem = reinterpret_cast<void *>(validAddressBase);
        validAddressBase += size;
        validAddressRangeReservations.push_back(std::make_tuple(size, reservedMem, true));
        return true;
    }

    void releaseReservedAddress(void *reservedAddress) override {
        Wddm::releaseReservedAddress(reservedAddress);
        validAddressRangeReleases.push_back(reservedAddress);
    }

    uintptr_t validAddressBase = 128U * MemoryConstants::megaByte + 4U; // intentionally only DWORD alignment to improve alignment testing
    std::vector<std::tuple<size_t, void *, bool>> validAddressRangeReservations;
    std::vector<void *> validAddressRangeReleases;

    using Wddm::featureTable;
    using Wddm::getReadOnlyFlagValue;
    using Wddm::gfxPartition;
    using Wddm::gfxPlatform;
    using Wddm::gmmMemory;
    using Wddm::isReadOnlyFlagFallbackSupported;
};

struct MockGmmMemoryWddmLinux : NEO::GmmMemory {
    MockGmmMemoryWddmLinux(NEO::GmmClientContext *gmmClientContext) : NEO::GmmMemory(gmmClientContext) {
    }

    bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override {
        return true;
    }
};

struct MockWddmLinuxMemoryManager : NEO::WddmMemoryManager {
    using WddmMemoryManager::allocate32BitGraphicsMemoryImpl;
    using WddmMemoryManager::allocatePhysicalDeviceMemory;
    using WddmMemoryManager::allocatePhysicalLocalDeviceMemory;
    using WddmMemoryManager::createPhysicalAllocation;
    using WddmMemoryManager::localMemorySupported;
    using WddmMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory;
    using WddmMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory;
    using WddmMemoryManager::WddmMemoryManager;
    NTSTATUS createInternalNTHandle(D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle, uint32_t rootDeviceIndex) override {
        if (failCreateInternalNTHandle) {
            return 1;
        } else {
            return WddmMemoryManager::createInternalNTHandle(resourceHandle, ntHandle, rootDeviceIndex);
        }
    }
    bool failCreateInternalNTHandle = false;
};

struct WddmLinuxMockHwDeviceIdWddm : public NEO::HwDeviceIdWddm {
    using HwDeviceIdWddm::HwDeviceIdWddm;
    using HwDeviceIdWddm::umKmDataTranslator;
};

NTSTATUS __stdcall closeAdapterMock(CONST D3DKMT_CLOSEADAPTER *arg) {
    return 0;
}

template <typename T>
struct RestorePoint {
    RestorePoint(T &obj) : obj(obj), prev(std::move(obj)) {
    }

    ~RestorePoint() {
        obj = std::move(prev);
    }

    T &obj;
    T prev;
};

struct GdiMockConfig {
    struct GdiMockCallbackWithReturn {
        NTSTATUS returnValue = 0U;
        void (*callback)() = nullptr;
        int callCount = 0U;
        void *context = nullptr;
    };

    D3DDDI_RESERVEGPUVIRTUALADDRESS receivedReserveGpuVaArgs = {};
    GdiMockCallbackWithReturn reserveGpuVaClb = {};

    std::pair<D3DKMT_ESCAPE, std::unique_ptr<std::vector<uint8_t>>> receivedEscapeArgs = {};
    GdiMockCallbackWithReturn escapeClb = {};

    std::pair<D3DKMT_CREATEALLOCATION, std::unique_ptr<std::vector<D3DDDI_ALLOCATIONINFO2>>> receivedCreateAllocationArgs = {};
    D3DDDI_MAPGPUVIRTUALADDRESS receivedMapGpuVirtualAddressArgs = {};
    D3DKMT_LOCK2 receivedLock2Args = {};
    D3DKMT_DESTROYALLOCATION2 receivedDestroyAllocation2Args = {};
    uint32_t mockAllocationHandle = 7U;

    D3DKMT_GETDEVICESTATE receivedGetDeviceStateArgs = {};
    GdiMockCallbackWithReturn getDeviceStateClb = {};
} gdiMockConfig;

NTSTATUS __stdcall reserveDeviceAddressSpaceMock(D3DDDI_RESERVEGPUVIRTUALADDRESS *arg) {
    gdiMockConfig.receivedReserveGpuVaArgs = *arg;
    gdiMockConfig.reserveGpuVaClb.callCount += 1;
    if (gdiMockConfig.reserveGpuVaClb.callback) {
        gdiMockConfig.reserveGpuVaClb.callback();
    }
    if (0 == gdiMockConfig.reserveGpuVaClb.returnValue) {
        bool validArgs = true;
        if (arg->BaseAddress) {
            validArgs = validArgs && isAligned<MemoryConstants::pageSize64k>(arg->BaseAddress);
            validArgs = validArgs && isAligned<MemoryConstants::pageSize64k>(arg->Size);
            validArgs = validArgs && (0U == arg->MinimumAddress);
            validArgs = validArgs && (0U == arg->MaximumAddress);
        } else {
            validArgs = validArgs && (0U == arg->BaseAddress);
            validArgs = validArgs && isAligned<MemoryConstants::pageSize64k>(arg->MinimumAddress);
            validArgs = validArgs && isAligned<MemoryConstants::pageSize64k>(arg->MaximumAddress);
        }
        validArgs = validArgs && isAligned<MemoryConstants::pageSize64k>(arg->Size);
        if (false == validArgs) {
            return -1;
        }
    }
    return gdiMockConfig.reserveGpuVaClb.returnValue;
}

NTSTATUS __stdcall createAllocation2Mock(D3DKMT_CREATEALLOCATION *arg) {
    gdiMockConfig.receivedCreateAllocationArgs.first = *arg;
    gdiMockConfig.receivedCreateAllocationArgs.second.reset();
    if (arg->NumAllocations) {
        gdiMockConfig.receivedCreateAllocationArgs.second.reset(new std::vector<D3DDDI_ALLOCATIONINFO2>(arg->pAllocationInfo2, arg->pAllocationInfo2 + arg->NumAllocations));
    }
    arg->pAllocationInfo2[0].hAllocation = gdiMockConfig.mockAllocationHandle;
    return 0;
}

NTSTATUS __stdcall mapGpuVirtualAddressMock(D3DDDI_MAPGPUVIRTUALADDRESS *arg) {
    gdiMockConfig.receivedMapGpuVirtualAddressArgs = *arg;
    return 0;
}

NTSTATUS __stdcall lock2Mock(D3DKMT_LOCK2 *arg) {
    gdiMockConfig.receivedLock2Args = *arg;
    return 0;
}

NTSTATUS __stdcall destroyAllocations2Mock(const D3DKMT_DESTROYALLOCATION2 *arg) {
    gdiMockConfig.receivedDestroyAllocation2Args = *arg;
    return 0;
}

NTSTATUS __stdcall escapeMock(const D3DKMT_ESCAPE *arg) {
    gdiMockConfig.receivedEscapeArgs.first = *arg;
    gdiMockConfig.receivedEscapeArgs.second.reset();
    gdiMockConfig.escapeClb.callCount += 1;
    if (arg->PrivateDriverDataSize) {
        auto privateData = static_cast<uint8_t *>(arg->pPrivateDriverData);
        gdiMockConfig.receivedEscapeArgs.second.reset(new std::vector<uint8_t>(privateData, privateData + arg->PrivateDriverDataSize));
    }
    if (gdiMockConfig.escapeClb.callback) {
        gdiMockConfig.escapeClb.callback();
    }
    return gdiMockConfig.escapeClb.returnValue;
}

NTSTATUS __stdcall getDeviceStateMock(D3DKMT_GETDEVICESTATE *arg) {
    gdiMockConfig.receivedGetDeviceStateArgs = *arg;
    gdiMockConfig.getDeviceStateClb.callCount += 1;
    if (gdiMockConfig.getDeviceStateClb.callback) {
        gdiMockConfig.getDeviceStateClb.callback();
    }
    return gdiMockConfig.getDeviceStateClb.returnValue;
}

struct WddmLinuxTest : public ::testing::Test {
    void SetUp() override {
        osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
        osEnvironment->gdi->closeAdapter = closeAdapterMock;
        auto hwDeviceIdIn = std::make_unique<WddmLinuxMockHwDeviceIdWddm>(NULL_HANDLE, LUID{}, 1u, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>());
        this->hwDeviceId = hwDeviceIdIn.get();
        auto &rootDeviceEnvironment = *mockExecEnv.rootDeviceEnvironments[0];
        auto wddm = std::make_unique<MockWddmLinux>(std::move(hwDeviceIdIn), rootDeviceEnvironment);
        this->wddm = wddm.get();
        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

        wddm->gmmMemory = std::make_unique<MockGmmMemoryWddmLinux>(gmmHelper->getClientContext());
        static_cast<PLATFORM &>(*wddm->gfxPlatform) = NEO::defaultHwInfo->platform;

        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        rootDeviceEnvironment.osInterface->setDriverModel(std::move(wddm));
    }

    size_t getMaxSvmSize() const {
        auto maximumApplicationAddress = MemoryConstants::max64BitAppAddress;
        auto productFamily = wddm->gfxPlatform->eProductFamily;
        auto svmSize = NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                           ? maximumApplicationAddress + 1u
                           : 0u;
        return svmSize;
    }

    RestorePoint<GdiMockConfig> gdiRestorePoint{gdiMockConfig};

    std::unique_ptr<NEO::OsEnvironmentWin> osEnvironment;
    NEO::MockExecutionEnvironment mockExecEnv;
    MockWddmLinux *wddm = nullptr;
    WddmLinuxMockHwDeviceIdWddm *hwDeviceId = nullptr;
};

using GmmTestsDG2 = WddmLinuxTest;

HWTEST2_F(GmmTestsDG2, givenGmmForImageWithForceLocalMemThenNonLocalIsSetToFalseAndoLocalOnlyIsSetToTrue, IsDG2) {
    const_cast<NEO::HardwareInfo *>(mockExecEnv.rootDeviceEnvironments[0]->getHardwareInfo())->featureTable.flags.ftrLocalMemory = 1u;

    NEO::ImageDescriptor imgDesc = {};
    imgDesc.imageType = NEO::ImageType::image2DArray;
    imgDesc.imageWidth = 60;
    imgDesc.imageHeight = 1;
    imgDesc.imageDepth = 1;
    imgDesc.imageArraySize = 10;

    NEO::ImageInfo imgInfo = NEO::MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.useLocalMemory = true;

    NEO::StorageInfo storageInfo = {};
    storageInfo.localOnlyRequired = true;
    storageInfo.memoryBanks = 2;
    storageInfo.systemMemoryPlacement = false;

    std::unique_ptr<NEO::Gmm> gmm(new NEO::Gmm(mockExecEnv.rootDeviceEnvironments[0]->getGmmHelper(), imgInfo, storageInfo, false));

    EXPECT_EQ(gmm->resourceParams.Flags.Info.NonLocalOnly, 0u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.LocalOnly, 1u);
}

using WddmLinuxConfigureDeviceAddressSpaceTest = WddmLinuxTest;

TEST_F(WddmLinuxConfigureDeviceAddressSpaceTest, givenSvmAddressSpaceThenReserveGpuVAForUSM) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace < MemoryConstants::max64BitAppAddress || productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);

    auto svmSize = this->getMaxSvmSize();
    EXPECT_EQ(NEO::windowsMinAddress, gdiMockConfig.receivedReserveGpuVaArgs.BaseAddress);
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.MinimumAddress);
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.MaximumAddress);
    EXPECT_EQ(svmSize - gdiMockConfig.receivedReserveGpuVaArgs.BaseAddress, gdiMockConfig.receivedReserveGpuVaArgs.Size);
    EXPECT_EQ(this->wddm->getAdapter(), gdiMockConfig.receivedReserveGpuVaArgs.hAdapter);
}

TEST_F(WddmLinuxConfigureDeviceAddressSpaceTest, givenPreReservedSvmAddressSpaceThenMakeSureWholeGpuVAForUSMIsReservedAndProperlyAligned) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace < MemoryConstants::max64BitAppAddress || productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    auto cantReserveWholeGpuVAButCanReservePortion = []() {
        gdiMockConfig.reserveGpuVaClb.returnValue = (gdiMockConfig.reserveGpuVaClb.callCount == 1) ? -1 : 0;
    };
    gdiMockConfig.reserveGpuVaClb.callback = cantReserveWholeGpuVAButCanReservePortion;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);

    auto cantReserveWholeGpuVAAndCantReservePortion = []() { gdiMockConfig.reserveGpuVaClb.returnValue = -1; };
    gdiMockConfig.reserveGpuVaClb.callback = cantReserveWholeGpuVAAndCantReservePortion;
    success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);

    auto svmSize = this->getMaxSvmSize();
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.BaseAddress);
    EXPECT_EQ(NEO::windowsMinAddress, gdiMockConfig.receivedReserveGpuVaArgs.MinimumAddress);
    EXPECT_EQ(svmSize, gdiMockConfig.receivedReserveGpuVaArgs.MaximumAddress);
    EXPECT_EQ(MemoryConstants::pageSize64k, gdiMockConfig.receivedReserveGpuVaArgs.Size);
    EXPECT_EQ(wddm->getAdapter(), gdiMockConfig.receivedReserveGpuVaArgs.hAdapter);
}

TEST_F(WddmLinuxConfigureDeviceAddressSpaceTest, givenNonSvmAddressSpaceThenReserveGpuVAForUSMIsNotCalled) {
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress) {
        GTEST_SKIP();
    }

    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);

    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.BaseAddress);
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.MinimumAddress);
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.MaximumAddress);
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.Size);
    EXPECT_EQ(0U, gdiMockConfig.receivedReserveGpuVaArgs.hAdapter);
}

using WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest = WddmLinuxConfigureDeviceAddressSpaceTest;

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, whenFailedToReadProceesPartitionLayoutThenFail) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    gdiMockConfig.escapeClb.returnValue = -1;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);
    EXPECT_EQ(1, gdiMockConfig.escapeClb.callCount);
    EXPECT_EQ(0U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, whenFailedToTranslateProceesPartitionLayoutThenFail) {
    struct MockTranslator : NEO::UmKmDataTranslator {
        bool &wasCalled;
        MockTranslator(bool &wasCalled) : wasCalled(wasCalled) { wasCalled = false; }

        bool translateGmmGfxPartitioningFromInternalRepresentation(GMM_GFX_PARTITIONING &dst, const void *src, size_t srcSize) override {
            wasCalled = true;
            return false;
        }
    };

    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    bool translatorWasCalled = false;
    this->hwDeviceId->umKmDataTranslator.reset(new MockTranslator(translatorWasCalled));
    osEnvironment->gdi->escape = escapeMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);
    EXPECT_TRUE(translatorWasCalled);
    EXPECT_EQ(1, gdiMockConfig.escapeClb.callCount);
    EXPECT_EQ(0U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenPreconfiguredAddressSpaceFromKmdThenUseIt) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    gdiMockConfig.escapeClb.callback = []() {
        memset(gdiMockConfig.receivedEscapeArgs.first.pPrivateDriverData, 0xFF, gdiMockConfig.receivedEscapeArgs.first.PrivateDriverDataSize);
    };
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFULL, this->wddm->gfxPartition.Standard.Base);
    EXPECT_EQ(1, gdiMockConfig.escapeClb.callCount);
    EXPECT_EQ(0U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenNoPreconfiguredAddressSpaceFromKmdThenConfigureOneAndUpdateKmdItInKmd) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);
    EXPECT_NE(0xFFFFFFFFFFFFFFFFULL, this->wddm->gfxPartition.Standard.Base);
    EXPECT_EQ(2, gdiMockConfig.escapeClb.callCount);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenNoPreconfiguredAddressSpaceFromKmdWhenFailedToReserveValidCpuAddressRangeThenFail) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    void *addr = nullptr;
    auto bloatedAddressSpaceSuccesfully = this->wddm->reserveValidAddressRange(MemoryConstants::max64BitAppAddress - this->wddm->validAddressBase, addr);
    ASSERT_TRUE(bloatedAddressSpaceSuccesfully);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);
    EXPECT_EQ(1, gdiMockConfig.escapeClb.callCount);
    ASSERT_EQ(2U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenNoPreconfiguredAddressSpaceFromKmdWhenUpdatingItInKmdAndFailedToTranslateThenFail) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    struct MockTranslator : NEO::UmKmDataTranslator {
        bool &wasCalled;
        MockTranslator(bool &wasCalled) : wasCalled(wasCalled) { wasCalled = false; }

        bool translateGmmGfxPartitioningToInternalRepresentation(void *dst, size_t dstSize, const GMM_GFX_PARTITIONING &src) override {
            wasCalled = true;
            return false;
        }
    };

    bool translatorWasCalled = false;
    this->hwDeviceId->umKmDataTranslator.reset(new MockTranslator(translatorWasCalled));
    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);
    EXPECT_TRUE(translatorWasCalled);
    EXPECT_EQ(1, gdiMockConfig.escapeClb.callCount);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(1U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenNoPreconfiguredAddressSpaceFromKmdWhenKmdGotJustUpdatedByDifferentClientThenUseItsAddressSpaceConfiguration) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    gdiMockConfig.escapeClb.callback = []() {
        if (gdiMockConfig.escapeClb.callCount == 2) {
            memset(gdiMockConfig.receivedEscapeArgs.first.pPrivateDriverData, 0xFF, gdiMockConfig.receivedEscapeArgs.first.PrivateDriverDataSize);
        }
    };
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFULL, this->wddm->gfxPartition.Standard.Base);
    EXPECT_EQ(2, gdiMockConfig.escapeClb.callCount);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(1U, this->wddm->validAddressRangeReleases.size());
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenNoPreconfiguredAddressSpaceFromKmdThenConfigureOneBasedOnAligmentAndSizeRequirements) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);
    ASSERT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());

    auto reservedRange = this->wddm->validAddressRangeReservations[0];
    auto reservedBase = reinterpret_cast<uintptr_t>(std::get<1>(reservedRange));
    auto reservedSize = std::get<0>(reservedRange);
    auto reservedEnd = reservedBase + reservedSize;

    constexpr uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte; // 4GB
    const auto &gfxPartition = this->wddm->gfxPartition;
    decltype(gfxPartition.SVM) precMem = {0, reservedBase};
    const decltype(gfxPartition.SVM) *precHeap = &precMem;
    for (auto heap : {&gfxPartition.Heap32[0], &gfxPartition.Heap32[1], &gfxPartition.Heap32[2], &gfxPartition.Heap32[3]}) {
        EXPECT_LE(precHeap->Limit, heap->Base);
        EXPECT_EQ(heap->Base + gfxHeap32Size, heap->Limit);
        EXPECT_TRUE(isAligned<NEO::GfxPartition::heapGranularity>(precHeap->Base));
        precHeap = heap;
    }

    for (auto heap : {&gfxPartition.Standard, &gfxPartition.Standard64KB}) {
        EXPECT_LE(precHeap->Limit, heap->Base);
        EXPECT_GE(heap->Limit + gfxHeap32Size, heap->Base);
        EXPECT_TRUE(isAligned<NEO::GfxPartition::heapGranularity>(precHeap->Base));
        precHeap = heap;
    }
    EXPECT_GE(reservedEnd, gfxPartition.Standard64KB.Limit);
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenTwoSvmAddressSpacesThenReserveGpuVAForBoth) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    std::vector<D3DDDI_RESERVEGPUVIRTUALADDRESS> allReserveGpuVaArgs;
    gdiMockConfig.reserveGpuVaClb.context = &allReserveGpuVaArgs;
    gdiMockConfig.reserveGpuVaClb.callback = []() {
        auto &allArgs = *static_cast<decltype(allReserveGpuVaArgs) *>(gdiMockConfig.reserveGpuVaClb.context);
        allArgs.push_back(gdiMockConfig.receivedReserveGpuVaArgs);
    };

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_TRUE(success);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());

    ASSERT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    auto reservedCpuRange = this->wddm->validAddressRangeReservations[0];
    auto reservedCpuBase = reinterpret_cast<uintptr_t>(std::get<1>(reservedCpuRange));
    auto reservedCpuSize = std::get<0>(reservedCpuRange);
    auto reservedCpuEnd = reservedCpuBase + reservedCpuSize;

    ASSERT_EQ(2U, allReserveGpuVaArgs.size());
    auto svmSize = this->getMaxSvmSize();
    EXPECT_EQ(NEO::windowsMinAddress, allReserveGpuVaArgs[0].BaseAddress);
    EXPECT_EQ(0U, allReserveGpuVaArgs[0].MinimumAddress);
    EXPECT_EQ(0U, allReserveGpuVaArgs[0].MaximumAddress);
    EXPECT_EQ(alignUp(reservedCpuBase - allReserveGpuVaArgs[0].BaseAddress, 2 * MemoryConstants::megaByte), allReserveGpuVaArgs[0].Size);
    EXPECT_EQ(this->wddm->getAdapter(), allReserveGpuVaArgs[0].hAdapter);

    EXPECT_EQ(alignDown(reservedCpuEnd, MemoryConstants::pageSize64k), allReserveGpuVaArgs[1].BaseAddress);
    EXPECT_EQ(0U, allReserveGpuVaArgs[1].MinimumAddress);
    EXPECT_EQ(0U, allReserveGpuVaArgs[1].MaximumAddress);
    EXPECT_EQ(alignUp(svmSize - reservedCpuEnd, MemoryConstants::pageSize64k), allReserveGpuVaArgs[1].Size);
    EXPECT_EQ(this->wddm->getAdapter(), allReserveGpuVaArgs[1].hAdapter);
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenTwoSvmAddressSpacesWhenReservGpuVAForFirstOneFailsThenFail) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;

    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    auto cantReserveWholeGpuVAButCanReservePortion = []() {
        gdiMockConfig.reserveGpuVaClb.returnValue = (gdiMockConfig.reserveGpuVaClb.callCount == 1) ? -1 : 0;
    };
    gdiMockConfig.reserveGpuVaClb.callback = cantReserveWholeGpuVAButCanReservePortion;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());

    EXPECT_EQ(2, gdiMockConfig.reserveGpuVaClb.callCount);
}

TEST_F(WddmLinuxConfigureReduced48bitDeviceAddressSpaceTest, givenTwoSvmAddressSpacesWhenReservGpuVAForSecondOneFailsThenFail) {
    auto &productHelper = mockExecEnv.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace != MemoryConstants::max64BitAppAddress && !productHelper.overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }

    this->wddm->featureTable->flags.ftrCCSRing = 1;
    osEnvironment->gdi->escape = escapeMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    auto cantReserveWholeGpuVAOfSecondButCanReservePortionOfSecont = []() {
        gdiMockConfig.reserveGpuVaClb.returnValue = (gdiMockConfig.reserveGpuVaClb.callCount == 2) ? -1 : 0;
    };
    gdiMockConfig.reserveGpuVaClb.callback = cantReserveWholeGpuVAOfSecondButCanReservePortionOfSecont;
    bool success = this->wddm->configureDeviceAddressSpace();
    EXPECT_FALSE(success);
    EXPECT_EQ(1U, this->wddm->validAddressRangeReservations.size());
    EXPECT_EQ(0U, this->wddm->validAddressRangeReleases.size());

    EXPECT_EQ(3, gdiMockConfig.reserveGpuVaClb.callCount);
}

TEST_F(WddmLinuxTest, givenRequestFor32bitAllocationWithoutPreexistingHostPtrWhenAllocatingThroughKmdIsPreferredThenAllocateThroughKmdAndLockAllocation) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    allocData.size = 64U;

    auto alloc = memoryManager.allocate32BitGraphicsMemoryImpl(allocData);
    EXPECT_NE(nullptr, alloc);
    memoryManager.freeGraphicsMemoryImpl(alloc);

    ASSERT_EQ(1U, gdiMockConfig.receivedCreateAllocationArgs.first.NumAllocations);
    EXPECT_EQ(nullptr, gdiMockConfig.receivedCreateAllocationArgs.second->operator[](0).pSystemMem);
    EXPECT_EQ(0U, gdiMockConfig.receivedMapGpuVirtualAddressArgs.BaseAddress);
    EXPECT_EQ(gdiMockConfig.mockAllocationHandle, gdiMockConfig.receivedLock2Args.hAllocation);
}

TEST_F(WddmLinuxTest, givenRequestFor32bitAllocationWithoutPreexistingHostPtrWhenAllocatingThroughKmdIsPreferredThenEnforceProperAlignment) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    allocData.size = 3U;

    auto alloc = memoryManager.allocate32BitGraphicsMemoryImpl(allocData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_TRUE(isAligned<MemoryConstants::allocationAlignment>(alloc->getUnderlyingBufferSize()));
    memoryManager.freeGraphicsMemoryImpl(alloc);
}

TEST_F(WddmLinuxTest, givenAllocatePhysicalDeviceMemoryThenAllocationReturned) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    NEO::MemoryManager::AllocationStatus status = NEO::MemoryManager::AllocationStatus::Error;
    allocData.size = 3U;

    auto alloc = memoryManager.allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, alloc);
    memoryManager.freeGraphicsMemoryImpl(alloc);
}

TEST_F(WddmLinuxTest, givenAllocatedMemoryAndCloseInternalHandleThenSharedHandleClosed) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    NEO::MemoryManager::AllocationStatus status = NEO::MemoryManager::AllocationStatus::Error;
    allocData.size = 3U;

    auto alloc = memoryManager.allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, alloc);
    uint64_t handle = 0;
    EXPECT_EQ(0, alloc->createInternalHandle(&memoryManager, 0u, handle));

    memoryManager.closeInternalHandle(handle, 0u, alloc);

    EXPECT_EQ(0, alloc->createInternalHandle(&memoryManager, 0u, handle));

    memoryManager.freeGraphicsMemoryImpl(alloc);
}

TEST_F(WddmLinuxTest, givenAllocatedMemoryAndCloseInternalHandleWithoutAllocationThenSharedHandleStillClosed) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    NEO::MemoryManager::AllocationStatus status = NEO::MemoryManager::AllocationStatus::Error;
    allocData.size = 3U;

    auto alloc = memoryManager.allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, alloc);
    uint64_t handle = 0;
    EXPECT_EQ(0, alloc->createInternalHandle(&memoryManager, 0u, handle));

    memoryManager.closeInternalHandle(handle, 0u, nullptr);

    memoryManager.freeGraphicsMemoryImpl(alloc);
}

TEST_F(WddmLinuxTest, givenAllocatedMemoryAndCreateInternalHandleFailedThenEmpyHandleReturned) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    NEO::MemoryManager::AllocationStatus status = NEO::MemoryManager::AllocationStatus::Error;
    allocData.size = 3U;

    auto alloc = memoryManager.allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, alloc);

    uint64_t handle = 0;
    EXPECT_EQ(0, alloc->createInternalHandle(&memoryManager, 0u, handle));

    memoryManager.closeInternalHandle(handle, 0u, alloc);

    memoryManager.failCreateInternalNTHandle = true;
    EXPECT_EQ(1, alloc->createInternalHandle(&memoryManager, 0u, handle));

    memoryManager.freeGraphicsMemoryImpl(alloc);
}

TEST_F(WddmLinuxTest, givenAllocatePhysicalLocalDeviceMemoryThenErrorReturned) {
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    NEO::MemoryManager::AllocationStatus status = NEO::MemoryManager::AllocationStatus::Error;
    allocData.size = 3U;

    auto alloc = memoryManager.allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_EQ(nullptr, alloc);
}

TEST_F(WddmLinuxTest, whenCheckedIfResourcesCleanupCanBeSkippedAndDeviceIsAliveThenReturnsFalse) {
    osEnvironment->gdi->getDeviceState = getDeviceStateMock;
    gdiMockConfig.getDeviceStateClb.returnValue = STATUS_SUCCESS;
    EXPECT_TRUE(this->wddm->isDriverAvailable());
    EXPECT_EQ(1, gdiMockConfig.getDeviceStateClb.callCount);
}

TEST_F(WddmLinuxTest, whenCheckedIfResourcesCleanupCanBeSkippedAndDeviceIsLostThenReturnsTrue) {
    osEnvironment->gdi->getDeviceState = getDeviceStateMock;
    gdiMockConfig.getDeviceStateClb.returnValue = -1;
    EXPECT_FALSE(this->wddm->isDriverAvailable());
    EXPECT_EQ(0, this->wddm->getGdi()->destroyAllocation2(nullptr));
    EXPECT_EQ(0, this->wddm->getGdi()->waitForSynchronizationObjectFromCpu(nullptr));
    EXPECT_EQ(0, this->wddm->getGdi()->destroyPagingQueue(nullptr));
    EXPECT_EQ(0, this->wddm->getGdi()->destroyDevice(nullptr));
    EXPECT_EQ(0, this->wddm->getGdi()->closeAdapter(nullptr));
    EXPECT_EQ(1, gdiMockConfig.getDeviceStateClb.callCount);
}

TEST_F(WddmLinuxTest, whenGettingReadOnlyFlagThenAlwaysReturnFalse) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    EXPECT_FALSE(wddm->getReadOnlyFlagValue(ptr));

    EXPECT_FALSE(wddm->getReadOnlyFlagValue(nullptr));
}

TEST_F(WddmLinuxTest, whenGettingReadOnlyFlagFallbackSupportThenFalseIsReturned) {
    EXPECT_FALSE(wddm->isReadOnlyFlagFallbackSupported());
}
class MockOsTimeLinux : public NEO::OSTimeLinux {
  public:
    MockOsTimeLinux(NEO::OSInterface &osInterface, std::unique_ptr<NEO::DeviceTime> deviceTime) : NEO::OSTimeLinux(osInterface, std::move(deviceTime)) {}
    bool getCpuTime(uint64_t *timeStamp) override {
        osTimeGetCpuTimeWasCalled = true;
        *timeStamp = 0x1234;
        return true;
    }
    bool osTimeGetCpuTimeWasCalled = false;
};

class MockDeviceTimeWddm : public NEO::DeviceTimeWddm {
  public:
    MockDeviceTimeWddm(NEO::Wddm *wddm) : NEO::DeviceTimeWddm(wddm) {}
    bool runEscape(NEO::Wddm *wddm, NEO::TimeStampDataHeader &escapeInfo) override {
        return true;
    }
};

TEST(OSTimeWinLinuxTests, givenOSInterfaceWhenGetGpuCpuTimeThenGetCpuTimeFromOsTimeWasCalled) {

    NEO::TimeStampData gpuCpuTime01 = {0};

    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceIdIn;
    auto osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
    osEnvironment->gdi->closeAdapter = closeAdapterMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    NEO::MockExecutionEnvironment mockExecEnv;
    auto &rootDeviceEnvironment = *mockExecEnv.rootDeviceEnvironments[0];
    hwDeviceIdIn.reset(new NEO::HwDeviceIdWddm(NULL_HANDLE, LUID{}, 1u, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>()));

    rootDeviceEnvironment.osInterface = std::make_unique<NEO::OSInterface>();

    std::unique_ptr<MockWddmLinux> wddm = std::make_unique<MockWddmLinux>(std::move(hwDeviceIdIn), rootDeviceEnvironment);
    static_cast<PLATFORM &>(*wddm->gfxPlatform) = NEO::defaultHwInfo->platform;
    rootDeviceEnvironment.setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    auto mockDeviceTimeWddm = std::make_unique<MockDeviceTimeWddm>(wddm.get());
    rootDeviceEnvironment.osInterface->setDriverModel(std::move(wddm));
    std::unique_ptr<NEO::DeviceTime> deviceTime = std::unique_ptr<NEO::DeviceTime>(mockDeviceTimeWddm.release());
    auto osTime = std::unique_ptr<MockOsTimeLinux>(new MockOsTimeLinux(*rootDeviceEnvironment.osInterface, std::move(deviceTime)));
    osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_TRUE(osTime->osTimeGetCpuTimeWasCalled);
}
