/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "execution_environment/root_device_environment.h"
#include "helpers/hw_info.h"
#include "os_interface/os_interface.h"
#include "os_interface/windows/wddm/wddm.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "test.h"

#include <typeinfo>

using namespace NEO;

TEST(wddmCreateTests, givenInputVersionWhenCreatingThenCreateRequestedObject) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto hwDeviceIds = OSInterface::discoverDevices();
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(std::move(hwDeviceIds[0]), rootDeviceEnvironment));
    EXPECT_EQ(typeid(*wddm.get()), typeid(Wddm));
}
