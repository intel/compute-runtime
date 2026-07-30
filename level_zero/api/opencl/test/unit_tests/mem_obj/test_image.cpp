/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

struct ImageDestructorTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createBuffer(void *usmPtr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, nullptr, sizeof(dummyStorage), false);
    }

    Image *createImageFrom(MemObj *parent) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        cl_image_format format{CL_R, CL_UNORM_INT8};
        // Null handle - the destructor then skips zeImageDestroy.
        return new Image(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, nullptr, nullptr, nullptr, false,
                         format, static_cast<cl_mem>(parent));
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

TEST_F(ImageDestructorTest, givenImageCreatedFromBufferWhenImageIsDestroyedThenParentBufferIsReleased) {
    auto buffer = createBuffer(&dummyStorage);
    const auto refCountWithoutImage = buffer->getRefInternalCount();

    auto image = createImageFrom(buffer);
    EXPECT_EQ(refCountWithoutImage + 1, buffer->getRefInternalCount());

    delete image;
    EXPECT_EQ(refCountWithoutImage, buffer->getRefInternalCount());

    delete buffer;
}

TEST_F(ImageDestructorTest, givenBufferReleasedBeforeImageCreatedFromItThenBufferStorageIsFreedOnlyAfterImageIsReleased) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    auto image = createImageFrom(buffer);

    buffer->decRefApi();
    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());

    image->decRefApi();

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    EXPECT_EQ(usmPtr, capturingContext->freeMemExtArgs[0].ptr);
}

TEST_F(ImageDestructorTest, givenParentBufferWithSharingHandlerWhenImageIsCreatedFromItThenHandlerIsInherited) {
    auto buffer = createBuffer(&dummyStorage);
    buffer->setSharingHandler(new SharingHandler());

    auto image = createImageFrom(buffer);
    EXPECT_EQ(buffer->peekSharingHandler(), image->peekSharingHandler());

    delete image;
    // Keeps ~Buffer on the plain USM free branch instead of alloc-data teardown.
    buffer->setSharingHandler(nullptr);
    delete buffer;
}

} // namespace ult
} // namespace LEO
} // namespace NEO
