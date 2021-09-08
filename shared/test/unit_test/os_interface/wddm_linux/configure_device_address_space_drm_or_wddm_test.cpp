/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"

#include "test.h"

#include "gmm_memory.h"

struct MockWddmLinux : NEO::Wddm {
    MockWddmLinux(std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceId, NEO::RootDeviceEnvironment &rootDeviceEnvironment)
        : NEO::Wddm(std::move(hwDeviceId), rootDeviceEnvironment) {
    }

    using Wddm::gfxPlatform;
    using Wddm::gmmMemory;
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
    using WddmMemoryManager::WddmMemoryManager;
};

NTSTATUS __stdcall closeAdapterMock(CONST D3DKMT_CLOSEADAPTER *arg) {
    return 0;
}

template <typename T>
struct RestorePoint {
    RestorePoint(T &obj) : obj(obj), prev(obj) {
    }

    ~RestorePoint() {
        std::swap(obj, prev);
    }

    T &obj;
    T prev;
};

template <typename... Ts>
struct MultiRestorePoint {
    MultiRestorePoint(Ts &...objs) : restorePoints(objs...) {}
    std::tuple<RestorePoint<Ts>...> restorePoints;
};

D3DDDI_RESERVEGPUVIRTUALADDRESS receivedReserveGpuVaArgs = {};
std::pair<D3DKMT_CREATEALLOCATION, std::vector<D3DDDI_ALLOCATIONINFO2>> receivedCreateAllocationArgs = {};
D3DDDI_MAPGPUVIRTUALADDRESS receivedMapGpuVirtualAddressArgs = {};
D3DKMT_LOCK2 receivedLock2Args = {};
D3DKMT_DESTROYALLOCATION2 receivedDestroyAllocation2Args = {};
static constexpr auto mockAllocationHandle = 7U;

NTSTATUS __stdcall reserveDeviceAddressSpaceMock(D3DDDI_RESERVEGPUVIRTUALADDRESS *arg) {
    receivedReserveGpuVaArgs = *arg;
    return 0;
}

NTSTATUS __stdcall createAllocation2Mock(D3DKMT_CREATEALLOCATION *arg) {
    receivedCreateAllocationArgs.first = *arg;
    receivedCreateAllocationArgs.second.resize(0);
    if (arg->NumAllocations) {
        receivedCreateAllocationArgs.second.assign(arg->pAllocationInfo2, arg->pAllocationInfo2 + arg->NumAllocations);
        arg->pAllocationInfo2[0].hAllocation = mockAllocationHandle;
    }
    return 0;
}

NTSTATUS __stdcall mapGpuVirtualAddressMock(D3DDDI_MAPGPUVIRTUALADDRESS *arg) {
    receivedMapGpuVirtualAddressArgs = *arg;
    return 0;
}

NTSTATUS __stdcall lock2Mock(D3DKMT_LOCK2 *arg) {
    receivedLock2Args = *arg;
    return 0;
}

NTSTATUS __stdcall destroyAllocations2Mock(const D3DKMT_DESTROYALLOCATION2 *arg) {
    receivedDestroyAllocation2Args = *arg;
    return 0;
}

TEST(WddmLinux, givenSvmAddressSpaceWhenConfiguringDeviceAddressSpaceThenReserveGpuVAForUSM) {

    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace < MemoryConstants::max64BitAppAddress) {
        GTEST_SKIP();
    }

    RestorePoint receivedReserveGpuVaArgsGlobalsRestorer{receivedReserveGpuVaArgs};

    NEO::MockExecutionEnvironment mockExecEnv;
    NEO::MockRootDeviceEnvironment mockRootDeviceEnvironment{mockExecEnv};
    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceIdIn;
    auto osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
    osEnvironment->gdi->closeAdapter = closeAdapterMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    hwDeviceIdIn.reset(new NEO::HwDeviceIdWddm(NULL_HANDLE, LUID{}, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>()));

    MockWddmLinux wddm{std::move(hwDeviceIdIn), mockRootDeviceEnvironment};
    auto mockGmmClientContext = NEO::GmmClientContext::create<NEO::MockGmmClientContext>(nullptr, NEO::defaultHwInfo.get());
    wddm.gmmMemory = std::make_unique<MockGmmMemoryWddmLinux>(mockGmmClientContext.get());
    *wddm.gfxPlatform = NEO::defaultHwInfo->platform;
    wddm.configureDeviceAddressSpace();

    auto maximumApplicationAddress = MemoryConstants::max64BitAppAddress;
    auto productFamily = wddm.gfxPlatform->eProductFamily;
    auto svmSize = NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                       ? maximumApplicationAddress + 1u
                       : 0u;

    EXPECT_EQ(MemoryConstants::pageSize64k, receivedReserveGpuVaArgs.BaseAddress);
    EXPECT_EQ(0U, receivedReserveGpuVaArgs.MinimumAddress);
    EXPECT_EQ(svmSize, receivedReserveGpuVaArgs.MaximumAddress);
    EXPECT_EQ(svmSize - receivedReserveGpuVaArgs.BaseAddress, receivedReserveGpuVaArgs.Size);
    EXPECT_EQ(wddm.getAdapter(), receivedReserveGpuVaArgs.hAdapter);
}

TEST(WddmLinux, givenNonSvmAddressSpaceWhenConfiguringDeviceAddressSpaceThenReserveGpuVAForUSMIsNotCalled) {

    if (NEO::hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress) {
        GTEST_SKIP();
    }

    RestorePoint receivedReserveGpuVaArgsGlobalsRestorer{receivedReserveGpuVaArgs};

    NEO::MockExecutionEnvironment mockExecEnv;
    NEO::MockRootDeviceEnvironment mockRootDeviceEnvironment{mockExecEnv};
    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceIdIn;
    auto osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
    osEnvironment->gdi->closeAdapter = closeAdapterMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    hwDeviceIdIn.reset(new NEO::HwDeviceIdWddm(NULL_HANDLE, LUID{}, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>()));

    MockWddmLinux wddm{std::move(hwDeviceIdIn), mockRootDeviceEnvironment};
    auto mockGmmClientContext = NEO::GmmClientContext::create<NEO::MockGmmClientContext>(nullptr, NEO::defaultHwInfo.get());
    wddm.gmmMemory = std::make_unique<MockGmmMemoryWddmLinux>(mockGmmClientContext.get());
    *wddm.gfxPlatform = NEO::defaultHwInfo->platform;
    wddm.configureDeviceAddressSpace();

    EXPECT_EQ(0U, receivedReserveGpuVaArgs.BaseAddress);
    EXPECT_EQ(0U, receivedReserveGpuVaArgs.MinimumAddress);
    EXPECT_EQ(0U, receivedReserveGpuVaArgs.MaximumAddress);
    EXPECT_EQ(0U, receivedReserveGpuVaArgs.Size);
    EXPECT_EQ(0U, receivedReserveGpuVaArgs.hAdapter);
}

TEST(WddmLinux, givenRequestFor32bitAllocationWithoutPreexistingHostPtrWhenAllocatingThroughKmdIsPreferredThenAllocateThroughKmdAndLockAllocation) {
    MultiRestorePoint gdiReceivedArgsRestorePoint(receivedReserveGpuVaArgs, receivedCreateAllocationArgs, receivedMapGpuVirtualAddressArgs, receivedLock2Args, receivedDestroyAllocation2Args);

    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceIdIn;
    auto osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
    NEO::MockExecutionEnvironment mockExecEnv;
    NEO::MockRootDeviceEnvironment mockRootDeviceEnvironment{mockExecEnv};
    osEnvironment->gdi->closeAdapter = closeAdapterMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    osEnvironment->gdi->createAllocation2 = createAllocation2Mock;
    osEnvironment->gdi->mapGpuVirtualAddress = mapGpuVirtualAddressMock;
    osEnvironment->gdi->lock2 = lock2Mock;
    osEnvironment->gdi->destroyAllocation2 = destroyAllocations2Mock;
    hwDeviceIdIn.reset(new NEO::HwDeviceIdWddm(NULL_HANDLE, LUID{}, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>()));

    std::unique_ptr<MockWddmLinux> wddm = std::make_unique<MockWddmLinux>(std::move(hwDeviceIdIn), mockRootDeviceEnvironment);
    auto mockGmmClientContext = NEO::GmmClientContext::create<NEO::MockGmmClientContext>(nullptr, NEO::defaultHwInfo.get());
    wddm->gmmMemory = std::make_unique<MockGmmMemoryWddmLinux>(mockGmmClientContext.get());
    *wddm->gfxPlatform = NEO::defaultHwInfo->platform;
    mockExecEnv.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    mockExecEnv.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(wddm));
    mockExecEnv.rootDeviceEnvironments[0]->gmmHelper.reset(new NEO::GmmHelper(mockExecEnv.rootDeviceEnvironments[0]->osInterface.get(), mockExecEnv.rootDeviceEnvironments[0]->getHardwareInfo()));

    MockWddmLinuxMemoryManager memoryManager{mockExecEnv};

    NEO::AllocationData allocData = {};
    allocData.size = 64U;

    auto alloc = memoryManager.allocate32BitGraphicsMemoryImpl(allocData, false);
    EXPECT_NE(nullptr, alloc);
    memoryManager.freeGraphicsMemoryImpl(alloc);

    ASSERT_EQ(1U, receivedCreateAllocationArgs.first.NumAllocations);
    ASSERT_EQ(1U, receivedCreateAllocationArgs.second.size());
    EXPECT_EQ(nullptr, receivedCreateAllocationArgs.second[0].pSystemMem);
    EXPECT_EQ(0U, receivedMapGpuVirtualAddressArgs.BaseAddress);
    EXPECT_EQ(mockAllocationHandle, receivedLock2Args.hAllocation);
}
class MockOsTimeLinux : public NEO::OSTimeLinux {
  public:
    MockOsTimeLinux(NEO::OSInterface *osInterface, std::unique_ptr<NEO::DeviceTime> deviceTime) : NEO::OSTimeLinux(osInterface, std::move(deviceTime)) {}
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

TEST(OSTimeWinLinuxTests, givenOSInterfaceWhenGetCpuGpuTimeThenGetCpuTimeFromOsTimeWasCalled) {

    NEO::TimeStampData CPUGPUTime01 = {0};

    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceIdIn;
    auto osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
    osEnvironment->gdi->closeAdapter = closeAdapterMock;
    osEnvironment->gdi->reserveGpuVirtualAddress = reserveDeviceAddressSpaceMock;
    NEO::MockExecutionEnvironment mockExecEnv;
    NEO::MockRootDeviceEnvironment mockRootDeviceEnvironment{mockExecEnv};
    hwDeviceIdIn.reset(new NEO::HwDeviceIdWddm(NULL_HANDLE, LUID{}, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>()));

    std::unique_ptr<NEO::OSInterface> osInterface(new NEO::OSInterface());

    std::unique_ptr<MockWddmLinux> wddm = std::make_unique<MockWddmLinux>(std::move(hwDeviceIdIn), mockRootDeviceEnvironment);
    *wddm->gfxPlatform = NEO::defaultHwInfo->platform;
    mockRootDeviceEnvironment.setHwInfo(NEO::defaultHwInfo.get());
    auto mockDeviceTimeWddm = std::make_unique<MockDeviceTimeWddm>(wddm.get());
    osInterface->setDriverModel(std::move(wddm));
    std::unique_ptr<NEO::DeviceTime> deviceTime = std::unique_ptr<NEO::DeviceTime>(mockDeviceTimeWddm.release());
    auto osTime = std::unique_ptr<MockOsTimeLinux>(new MockOsTimeLinux(osInterface.get(), std::move(deviceTime)));
    osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_TRUE(osTime->osTimeGetCpuTimeWasCalled);
}
