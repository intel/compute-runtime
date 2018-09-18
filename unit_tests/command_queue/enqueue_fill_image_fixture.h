/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"

namespace OCLRT {

struct EnqueueFillImageTestFixture : public CommandEnqueueFixture {

    EnqueueFillImageTestFixture() : image(nullptr) {
    }

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        context = new MockContext(pDevice);
        image = Image2dHelper<>::create(context);
    }

    virtual void TearDown(void) override {
        delete image;
        delete context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueFillImage() {
        auto retVal = EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ,
                                                                 image);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext *context;
    Image *image;
};
} // namespace OCLRT
