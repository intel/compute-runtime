/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/preemption.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/windows/gdi_interface.h"
#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"

#include "mock_gmm_memory.h"

using namespace NEO;

struct Gen12LpWddmTest : public GdiDllFixture, ::testing::Test {
    void SetUp() override {
        GdiDllFixture::SetUp();

        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->initGmm();
        rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm(*rootDeviceEnvironment)));
        gmmMemory = new ::testing::NiceMock<GmockGmmMemory>(executionEnvironment->getGmmClientContext());
        wddm->gmmMemory.reset(gmmMemory);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
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

    auto hwInfoMock = *platformDevices[0];
    wddm->init(hwInfoMock);

    EXPECT_EQ(minAddress, wddm->getWddmMinAddress());
}
