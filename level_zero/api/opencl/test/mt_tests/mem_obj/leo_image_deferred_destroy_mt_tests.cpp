/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/opencl/test/common/fixtures/image_destroy_fixture.h"

#include "CL/cl.h"

#include <atomic>
#include <memory>
#include <thread>

namespace NEO {
namespace LEO {
namespace ult {

using ImageDeferredDestroyMtTests = ImageDeferredDestroyTest;

HWTEST_F(ImageDeferredDestroyMtTests, givenImageFromBufferInUseWhenImageAndLastContextReferenceAreReleasedConcurrentlyThenL0ImageIsDestroyedAndParentIsReleased) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.callBaseWaitForCompletionWithTimeout = false;
    cl_device_id clDeviceId = clDevice;
    constexpr uint32_t iterations = 50u;

    for (uint32_t iteration = 0u; iteration < iterations; ++iteration) {
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        auto buffer = createBuffer(&dummyStorage);
        const auto refCountWithoutImage = buffer->getRefInternalCount();
        auto image = createImage(buffer);
        cl_context context = leoContext.release();

        std::atomic<bool> start = false;
        std::thread imageReleaseThread([&] {
            while (!start) {
                std::this_thread::yield();
            }
            image->decRefApi();
        });
        std::thread contextReleaseThread([&] {
            while (!start) {
                std::this_thread::yield();
            }
            clReleaseContext(context);
        });
        start = true;
        imageReleaseThread.join();
        contextReleaseThread.join();

        EXPECT_EQ(iteration + 1, destroyCount);
        EXPECT_EQ(refCountWithoutImage, buffer->getRefInternalCount());
        buffer->decRefApi();
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
