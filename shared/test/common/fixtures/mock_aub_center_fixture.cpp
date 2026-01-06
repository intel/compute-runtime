/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/mock_aub_center_fixture.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/tests_configuration.h"

namespace NEO {

MockAubCenterFixture::MockAubCenterFixture(CommandStreamReceiverType commandStreamReceiverType) : commandStreamReceiverType(commandStreamReceiverType){};

void MockAubCenterFixture::setMockAubCenter(RootDeviceEnvironment &rootDeviceEnvironment) {
    setMockAubCenter(rootDeviceEnvironment, CommandStreamReceiverType::aub, true);
}
void MockAubCenterFixture::setMockAubCenter(RootDeviceEnvironment &rootDeviceEnvironment, CommandStreamReceiverType commandStreamReceiverType, bool createMockAubManager = true) {
    if (testMode != TestMode::aubTests && testMode != TestMode::aubTestsWithTbx) {
        rootDeviceEnvironment.initGmm();
        auto mockAubCenter = std::make_unique<MockAubCenter>(rootDeviceEnvironment, false, "", commandStreamReceiverType);
        if (createMockAubManager && !mockAubCenter->aubManager) {
            mockAubCenter->aubManager = std::make_unique<MockAubManager>();
        }
        rootDeviceEnvironment.aubCenter.reset(mockAubCenter.release());
    }
}
} // namespace NEO
