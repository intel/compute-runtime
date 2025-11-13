/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"

#include "aub_fixture.h"

namespace NEO {
struct ImageAubFixture : public AUBCommandStreamFixture {
    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::setUp;

    void setUp(bool enableBlitter) {
        if (enableBlitter) {

            MockExecutionEnvironment mockExecutionEnvironment{};
            auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();

            if (!productHelper.isBlitterForImagesSupported() || !productHelper.blitEnqueuePreferred(false)) {
                GTEST_SKIP();
            }

            auto hardwareInfo = *defaultHwInfo;
            hardwareInfo.capabilityTable.blitterOperationsSupported = true;
            CommandStreamFixture::setUp(&hardwareInfo);
        } else {
            CommandStreamFixture::setUp(nullptr);
        }
    }

    void tearDown() {
        if (pCmdQ) {
            auto blocked = pCmdQ->isQueueBlocked();
            UNRECOVERABLE_IF(blocked);
        }

        CommandStreamFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
};
} // namespace NEO
