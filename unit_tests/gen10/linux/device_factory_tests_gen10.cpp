/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/linux/device_factory_tests.h"

typedef DeviceFactoryLinuxTest DeviceFactoryLinuxTestCnl;

GEN10TEST_F(DeviceFactoryLinuxTestCnl, queryWhitelistedPreemptionRegister) {
    pDrm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;

    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(numDevices, executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_TRUE(executionEnvironment.getHardwareInfo()->capabilityTable.whitelistedRegisters.csChicken1_0x2580);

    DeviceFactory::releaseDevices();
}

GEN10TEST_F(DeviceFactoryLinuxTestCnl, queryNotWhitelistedPreemptionRegister) {
    pDrm->StoredPreemptionSupport = 0;

    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(numDevices, executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_FALSE(executionEnvironment.getHardwareInfo()->capabilityTable.whitelistedRegisters.csChicken1_0x2580);

    DeviceFactory::releaseDevices();
}
