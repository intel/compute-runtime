/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "test_traits_common.h"

struct ImagesSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::imagesSupported;
    }
};

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
