/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gmm_memory_base.h"
#include "shared/test/common/os_interface/windows/gdi_dll_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct Gen12LpWddmTest : public GdiDllFixture, ::testing::Test {
    void SetUp() override {
        GdiDllFixture::SetUp();

        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->initGmm();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        gmmMemory = new MockGmmMemoryBase(rootDeviceEnvironment->getGmmClientContext());
        wddm->gmmMemory.reset(gmmMemory);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    WddmMock *wddm = nullptr;
    MockGmmMemoryBase *gmmMemory = nullptr;
};

GEN12LPTEST_F(Gen12LpWddmTest, whenConfigureDeviceAddressSpaceThenObtainMinAddress) {
    uintptr_t minAddress = 0x12345u;

    EXPECT_NE(NEO::windowsMinAddress, minAddress);

    gmmMemory->getInternalGpuVaRangeLimitResult = minAddress;

    wddm->init();

    EXPECT_EQ(minAddress, wddm->getWddmMinAddress());
    EXPECT_EQ(1u, gmmMemory->getInternalGpuVaRangeLimitCalled);
}

using Gen12LpWddmHwInfoTest = ::testing::Test;

GEN12LPTEST_F(Gen12LpWddmHwInfoTest, givenIncorrectProductFamiliyWhenInitCalledThenOverride) {
    HardwareInfo localHwInfo = *defaultHwInfo;

    localHwInfo.platform.eRenderCoreFamily = GFXCORE_FAMILY::IGFX_UNKNOWN_CORE;
    localHwInfo.platform.eDisplayCoreFamily = GFXCORE_FAMILY::IGFX_UNKNOWN_CORE;

    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&localHwInfo.platform,
                                                         &localHwInfo.gtSystemInfo,
                                                         localHwInfo.capabilityTable.gpuAddressSpace));

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();

    auto localWddm = std::unique_ptr<Wddm>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    localWddm->init();

    auto newHwInfo = rootDeviceEnvironment->getHardwareInfo();

    EXPECT_EQ(GFXCORE_FAMILY::IGFX_GEN12LP_CORE, newHwInfo->platform.eRenderCoreFamily);
    EXPECT_EQ(GFXCORE_FAMILY::IGFX_GEN12LP_CORE, newHwInfo->platform.eDisplayCoreFamily);

    // reset mock gdi globals
    localHwInfo = *defaultHwInfo;
    mockGdiDll.reset(setAdapterInfo(&localHwInfo.platform,
                                    &localHwInfo.gtSystemInfo,
                                    localHwInfo.capabilityTable.gpuAddressSpace));
}
