/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_info.h"
#include "core/os_interface/windows/wddm/wddm.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"

#include <typeinfo>

using namespace NEO;

TEST(wddmCreateTests, givenInputVersionWhenCreatingThenCreateRequestedObject) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto hwDeviceId = Wddm::discoverDevices();
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(std::move(hwDeviceId), rootDeviceEnvironment));
    EXPECT_EQ(typeid(*wddm.get()), typeid(Wddm));
}
