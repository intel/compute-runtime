/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <typeinfo>

using namespace NEO;

TEST(wddmCreateTests, givenInputVersionWhenCreatingThenCreateRequestedObject) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(std::unique_ptr<HwDeviceIdWddm>(hwDeviceIds[0].release()->as<HwDeviceIdWddm>()), rootDeviceEnvironment));
    EXPECT_NE(nullptr, wddm);
}
