/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_aub_fixture.h"

#include <array>

struct MultiTileBuffersXeHPAndLater : public MulticontextAubFixture, public ::testing::Test {
    static constexpr uint32_t numTiles = 2;

    void SetUp() override {
        MulticontextAubFixture::setUp(numTiles, EnabledCommandStreamers::Single, false);
    }
    void TearDown() override {
        MulticontextAubFixture::tearDown();
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiTileBuffersXeHPAndLater, givenTwoBuffersAllocatedOnDifferentTilesWhenCopiedThenDataValidates) {
    if constexpr (is64bit) {

        constexpr size_t bufferSize = 64 * 1024u;

        char bufferTile0Memory[bufferSize] = {};
        char bufferTile1Memory[bufferSize] = {};

        for (auto index = 0u; index < bufferSize; index++) {
            bufferTile0Memory[index] = index % 255;
            bufferTile1Memory[index] = index % 255;
        }

        auto retVal = CL_INVALID_VALUE;

        cl_mem_flags flags = CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR;
        MemoryProperties memoryProperties =
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice());
        memoryProperties.pDevice = &context->getDevice(1)->getDevice();
        auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), memoryProperties, flags, 0, bufferSize, bufferTile0Memory, retVal));
        ASSERT_NE(nullptr, srcBuffer);

        flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
        memoryProperties.pDevice = &context->getDevice(2)->getDevice();
        auto dstBuffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), memoryProperties, flags, 0, bufferSize, bufferTile1Memory, retVal));
        ASSERT_NE(nullptr, dstBuffer);

        auto cmdQ = commandQueues[0][0].get();

        expectMemory<FamilyType>(AUBFixture::getGpuPointer(srcBuffer->getGraphicsAllocation(rootDeviceIndex)), bufferTile0Memory, bufferSize, 0, 0);
        expectMemory<FamilyType>(AUBFixture::getGpuPointer(dstBuffer->getGraphicsAllocation(rootDeviceIndex)), bufferTile1Memory, bufferSize, 0, 0);

        cl_uint numEventsInWaitList = 0;
        cl_event *eventWaitList = nullptr;
        cl_event *event = nullptr;

        retVal = cmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(),
                                         0, 0,
                                         bufferSize, numEventsInWaitList,
                                         eventWaitList, event);

        EXPECT_EQ(CL_SUCCESS, retVal);

        cmdQ->flush();

        expectMemory<FamilyType>(AUBFixture::getGpuPointer(dstBuffer->getGraphicsAllocation(rootDeviceIndex)), bufferTile0Memory, bufferSize, 0, 0);
    }
}
