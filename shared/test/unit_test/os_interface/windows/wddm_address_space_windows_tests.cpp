/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using WddmTests = WddmTestWithMockGdiDll;

TEST_F(WddmTests, whenInitializingWddmThenSetMinAddressToCorrectValue) {
    constexpr static uintptr_t mockedInternalGpuVaRange = 0x9876u;
    auto gmmMemory = new MockGmmMemoryBase(wddm->rootDeviceEnvironment.getGmmClientContext());
    gmmMemory->overrideInternalGpuVaRangeLimit(mockedInternalGpuVaRange);
    wddm->gmmMemory.reset(gmmMemory);

    ASSERT_EQ(0u, wddm->getWddmMinAddress());
    wddm->init();

    const bool obtainFromGmm = defaultHwInfo->platform.eRenderCoreFamily == IGFX_GEN12LP_CORE;
    const auto expectedMinAddress = obtainFromGmm ? mockedInternalGpuVaRange : windowsMinAddress;
    ASSERT_EQ(expectedMinAddress, wddm->getWddmMinAddress());
}
