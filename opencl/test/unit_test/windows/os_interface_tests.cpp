/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/windows/os_interface.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "test.h"

using namespace NEO;

TEST(osInterfaceTests, osInterfaceLocalMemoryEnabledByDefault) {
    EXPECT_TRUE(OSInterface::osEnableLocalMemory);
}

TEST(osInterfaceTests, whenOsInterfaceSetupGmmInputArgsThenProperAdapterBDFIsSet) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMock(rootDeviceEnvironment);
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    wddm->init();

    auto &adapterBDF = wddm->adapterBDF;
    adapterBDF.Bus = 0x12;
    adapterBDF.Device = 0x34;
    adapterBDF.Function = 0x56;

    GMM_INIT_IN_ARGS gmmInputArgs = {};
    EXPECT_NE(0, memcmp(&adapterBDF, &gmmInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
    osInterface.setGmmInputArgs(&gmmInputArgs);
    EXPECT_EQ(0, memcmp(&adapterBDF, &gmmInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
}
