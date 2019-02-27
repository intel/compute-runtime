/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "test.h"
#include "unit_tests/os_interface/windows/os_interface_win_tests.h"

typedef OsInterfaceTest OsInterfaceTestSkl;

GEN9TEST_F(OsInterfaceTestSkl, askKmdIfPreemptionRegisterWhitelisted) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;

    ExecutionEnvironment executionEnvironment;
    bool success = DeviceFactory::getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_TRUE(success);

    for (size_t i = 0u; i < numDevices; i++) {
        if (hwInfo[i].pWaTable->waEnablePreemptionGranularityControlByUMD) {
            EXPECT_TRUE(hwInfo[i].capabilityTable.whitelistedRegisters.csChicken1_0x2580);
        } else {
            EXPECT_FALSE(hwInfo[i].capabilityTable.whitelistedRegisters.csChicken1_0x2580);
        }
    }

    DeviceFactory::releaseDevices();
}
