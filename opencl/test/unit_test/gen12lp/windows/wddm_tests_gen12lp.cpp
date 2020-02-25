/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/gdi_interface.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/os_interface/windows/gdi_dll_fixture.h"
#include "test.h"

#include "mock_gmm_memory.h"

using namespace NEO;

struct Gen12LpWddmTest : public GdiDllFixture, ::testing::Test {
    void SetUp() override {
        GdiDllFixture::SetUp();

        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->initGmm();
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment)));
        gmmMemory = new ::testing::NiceMock<GmockGmmMemory>(rootDeviceEnvironment->getGmmClientContext());
        wddm->gmmMemory.reset(gmmMemory);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment;
    std::unique_ptr<WddmMock> wddm;
    GmockGmmMemory *gmmMemory = nullptr;
};

GEN12LPTEST_F(Gen12LpWddmTest, whenConfigureDeviceAddressSpaceThenObtainMinAddress) {
    ON_CALL(*gmmMemory, configureDeviceAddressSpace(::testing::_,
                                                    ::testing::_,
                                                    ::testing::_,
                                                    ::testing::_,
                                                    ::testing::_))
        .WillByDefault(::testing::Return(true));

    uintptr_t minAddress = 0x12345u;

    EXPECT_NE(NEO::windowsMinAddress, minAddress);

    EXPECT_CALL(*gmmMemory,
                getInternalGpuVaRangeLimit())
        .Times(1)
        .WillRepeatedly(::testing::Return(minAddress));

    wddm->init();

    EXPECT_EQ(minAddress, wddm->getWddmMinAddress());
}
