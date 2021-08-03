/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
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

NTSTATUS __stdcall closeAdapterMock(CONST D3DKMT_CLOSEADAPTER *arg) {
    return 0;
}

template <typename T>
struct RestorePoint {
    RestorePoint(T &obj) : obj(obj), prev(obj) {
    }

    ~RestorePoint() {
        obj = prev;
    }

    T &obj;
    T prev;
};

D3DDDI_RESERVEGPUVIRTUALADDRESS receivedReserveGpuVaArgs = {};

NTSTATUS __stdcall reserveDeviceAddressSpaceMock(D3DDDI_RESERVEGPUVIRTUALADDRESS *arg) {
    receivedReserveGpuVaArgs = *arg;
    return 0;
}

TEST(WddmLinux, whenConfiguringDeviceAddressSpaceThenReserveGpuVAForUSM) {
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
