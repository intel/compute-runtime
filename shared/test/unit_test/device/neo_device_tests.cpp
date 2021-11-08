/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "test.h"

using namespace NEO;

TEST(DeviceTest, whenBlitterOperationsSupportIsDisabledThenNoInternalCopyEngineIsReturned) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = false;

    UltDeviceFactory factory{1, 0};
    EXPECT_EQ(nullptr, factory.rootDevices[0]->getInternalCopyEngine());
}

TEST(DeviceTest, givenBlitterOperationsDisabledWhenCreatingBlitterEngineThenAbort) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = false;

    UltDeviceFactory factory{1, 0};
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::Cooperative}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::Internal}), std::runtime_error);
    EXPECT_THROW(factory.rootDevices[0]->createEngine(0, {aub_stream::EngineType::ENGINE_BCS, EngineUsage::LowPriority}), std::runtime_error);
}
