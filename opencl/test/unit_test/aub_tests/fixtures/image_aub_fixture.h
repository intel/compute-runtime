/*
 * Copyright (C) 2022 Intel Corporation
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
struct ImageAubFixture : public ClDeviceFixture, public AUBCommandStreamFixture {
    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::setUp;

    void setUp(bool enableBlitter) {
        if (enableBlitter) {
            if (!(ProductHelper::get(defaultHwInfo->platform.eProductFamily)->isBlitterForImagesSupported())) {
                GTEST_SKIP();
            }

            hardwareInfo = *defaultHwInfo;
            hardwareInfo.capabilityTable.blitterOperationsSupported = true;
            ClDeviceFixture::setUpImpl(&hardwareInfo);
        } else {
            ClDeviceFixture::setUp();
        }

        context = new MockContext(pClDevice);

        cl_int retVal = CL_SUCCESS;
        auto clQueue = clCreateCommandQueueWithProperties(context, pClDevice, nullptr, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        pCmdQ = castToObject<CommandQueue>(clQueue);

        CommandStreamFixture::setUp(pCmdQ);
    }

    void tearDown() {
        if (pCmdQ) {
            auto blocked = pCmdQ->isQueueBlocked();
            UNRECOVERABLE_IF(blocked);
            pCmdQ->release();
        }
        if (context) {
            context->release();
        }

        CommandStreamFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
    CommandQueue *pCmdQ = nullptr;
    MockContext *context = nullptr;
};
} // namespace NEO
