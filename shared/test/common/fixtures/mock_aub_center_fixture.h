/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/tests_configuration.h"

namespace NEO {
struct MockAubCenterFixture {

    MockAubCenterFixture() = default;
    MockAubCenterFixture(CommandStreamReceiverType commandStreamReceiverType) : commandStreamReceiverType(commandStreamReceiverType){};

    void setUp() {
    }
    void tearDown() {
    }

    static void setMockAubCenter(RootDeviceEnvironment &rootDeviceEnvironment) {
        setMockAubCenter(rootDeviceEnvironment, CommandStreamReceiverType::CSR_AUB);
    }
    static void setMockAubCenter(RootDeviceEnvironment &rootDeviceEnvironment, CommandStreamReceiverType commandStreamReceiverType) {
        if (testMode != TestMode::AubTests && testMode != TestMode::AubTestsWithTbx) {
            rootDeviceEnvironment.initGmm();
            auto mockAubCenter = std::make_unique<MockAubCenter>(defaultHwInfo.get(), *rootDeviceEnvironment.getGmmHelper(), false, "", commandStreamReceiverType);
            mockAubCenter->aubManager = std::make_unique<MockAubManager>();
            rootDeviceEnvironment.aubCenter.reset(mockAubCenter.release());
        }
    }

  protected:
    CommandStreamReceiverType commandStreamReceiverType = CommandStreamReceiverType::CSR_AUB;
};
} // namespace NEO
