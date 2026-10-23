/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using WddmTests = WddmTestWithMockGdiDll;

TEST_F(WddmTests, whenInitializingWddmThenSetMinAddressToCorrectValue) {
    auto gmmMemory = new MockGmmMemory(wddm->rootDeviceEnvironment.getGmmClientContext());
    wddm->gmmMemory.reset(gmmMemory);

    ASSERT_EQ(0u, wddm->getWddmMinAddress());
    wddm->init();

    ASSERT_EQ(windowsMinAddress, wddm->getWddmMinAddress());
}
