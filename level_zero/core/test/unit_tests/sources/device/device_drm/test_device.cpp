/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using LuidDeviceTest = Test<DeviceFixture>;

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureAndDRMDriverTypeThenUnsupportedReturned) {
    DebugManager.flags.EnableL0ReadLUIDExtension.set(true);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_luid_ext_properties_t deviceLuidProperties = {ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES};
    deviceProperties.pNext = &deviceLuidProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    DebugManager.flags.EnableL0ReadLUIDExtension.set(false);
}

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureAndDebugVariableFalseThenSuccessReturned) {
    DebugManager.flags.EnableL0ReadLUIDExtension.set(false);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_luid_ext_properties_t deviceLuidProperties = {ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES};
    deviceProperties.pNext = &deviceLuidProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0