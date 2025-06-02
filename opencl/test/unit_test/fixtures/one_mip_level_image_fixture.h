/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct OneMipLevelImageFixture {
    template <typename GfxFamily>
    struct CommandQueueHwMock : CommandQueueHw<GfxFamily> {
        OneMipLevelImageFixture &fixture;
        CommandQueueHwMock(OneMipLevelImageFixture &fixture)
            : CommandQueueHw<GfxFamily>(&fixture.context, fixture.context.getDevice(0), nullptr, false),
              fixture(fixture) {}

        void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &multiDispatchInfo) override {
            fixture.builtinOpsParamsCaptured = true;
            fixture.usedBuiltinOpsParams = multiDispatchInfo.peekBuiltinOpParams();
        }
    };

    void setUp() {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

        cl_image_desc imageDesc = Image3dDefaults::imageDesc;
        this->region[0] = imageDesc.image_width;
        this->region[1] = imageDesc.image_height;
        this->region[2] = imageDesc.image_depth;

        this->image.reset(createImage());
    }

    void tearDown() {
    }

    Image *createImage() {
        cl_image_desc imageDesc = Image3dDefaults::imageDesc;
        imageDesc.num_mip_levels = 1;
        return ImageHelperUlt<Image3dDefaults>::create(&context, &imageDesc);
    }

    Buffer *createBuffer() {
        return BufferHelper<>::create(&context);
    }

    template <typename GfxFamily>
    std::unique_ptr<CommandQueue> createQueue() {
        return std::unique_ptr<CommandQueue>(new CommandQueueHwMock<GfxFamily>(*this));
    }

    MockContext context;
    std::unique_ptr<Image> image;
    size_t origin[4] = {0, 0, 0, 0xdeadbeef};
    size_t region[4] = {0, 0, 0, 0};
    void *cpuPtr = Image3dDefaults::hostPtr;

    BuiltinOpParams usedBuiltinOpsParams;
    bool builtinOpsParamsCaptured;
};

} // namespace NEO
