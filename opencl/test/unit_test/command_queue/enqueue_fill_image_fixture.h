/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

namespace NEO {

struct EnqueueFillImageTestFixture : public CommandEnqueueFixture {
    void setUp() {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();
        context = new MockContext(pClDevice);
        image = Image2dHelperUlt<>::create(context);
    }

    void tearDown() {
        if (testing::Test::IsSkipped()) {
            return;
        }
        delete image;
        delete context;
        CommandEnqueueFixture::tearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueFillImage() {
        auto retVal = EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ,
                                                                 image);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext *context = nullptr;
    Image *image = nullptr;
};
} // namespace NEO
