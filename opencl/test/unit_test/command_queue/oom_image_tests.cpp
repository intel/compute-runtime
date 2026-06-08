/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

struct OOMSetting {
    bool oomCS;
    bool oomISH;
};

struct OOMCommandQueueImageTest : public ClDeviceFixture,
                                  public ::testing::Test {

    void SetUp() override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        ClDeviceFixture::setUp();
        context = new MockContext(pClDevice);

        srcImage = Image2dHelperUlt<>::create(context);
        dstImage = Image2dHelperUlt<>::create(context);
        initialized = true;
    }

    void TearDown() override {
        if (!initialized) {
            return;
        }
        delete dstImage;
        delete srcImage;
        context->release();
        ClDeviceFixture::tearDown();
    }

    static void applyOOMSetting(CommandQueue &queue, const OOMSetting &oomSetting) {
        const auto oomSize = 10u;
        if (oomSetting.oomCS) {
            auto &cs = queue.getCS(oomSize);
            cs.getSpace(cs.getAvailableSpace() - oomSize);
            ASSERT_EQ(oomSize, cs.getAvailableSpace());
        }
        if (oomSetting.oomISH) {
            auto &ish = queue.getIndirectHeap(IndirectHeap::Type::dynamicState, oomSize);
            ish.getSpace(ish.getAvailableSpace() - oomSize);
            ASSERT_EQ(oomSize, ish.getAvailableSpace());
        }
    }

    MockContext *context;
    Image *srcImage = nullptr;
    Image *dstImage = nullptr;
    bool initialized = false;
};

HWTEST_F(OOMCommandQueueImageTest, WhenCopyingImageThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : {OOMSetting{true, false}, OOMSetting{false, true}, OOMSetting{true, true}}) {
        CommandQueueHw<FamilyType> oomQueue(context, pClDevice, 0, false);
        applyOOMSetting(oomQueue, oomSetting);
        CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

        auto &commandStream = oomQueue.getCS(1024);
        auto &indirectHeap = oomQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        auto retVal1 = EnqueueCopyImageHelper<>::enqueue(&oomQueue);
        auto retVal2 = EnqueueCopyImageHelper<>::enqueue(&cmdQ);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        if (usedAfterISH > usedBeforeISH) {
            EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());
        } else {
            EXPECT_LE(usedAfterISH, indirectHeap.getMaxAvailableSpace());
        }

        EXPECT_EQ(CL_SUCCESS, retVal1);
        EXPECT_EQ(CL_SUCCESS, retVal2);
    }
}

HWTEST_F(OOMCommandQueueImageTest, WhenFillingImageThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : {OOMSetting{true, false}, OOMSetting{false, true}, OOMSetting{true, true}}) {
        CommandQueueHw<FamilyType> oomQueue(context, pClDevice, 0, false);
        applyOOMSetting(oomQueue, oomSetting);
        CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

        auto &commandStream = oomQueue.getCS(1024);
        auto &indirectHeap = oomQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        auto retVal1 = EnqueueFillImageHelper<>::enqueue(&oomQueue);
        auto retVal2 = EnqueueFillImageHelper<>::enqueue(&cmdQ);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        if (usedAfterISH > usedBeforeISH) {
            EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());
        } else {
            EXPECT_LE(usedAfterISH, indirectHeap.getMaxAvailableSpace());
        }

        EXPECT_EQ(CL_SUCCESS, retVal1);
        EXPECT_EQ(CL_SUCCESS, retVal2);
    }
}

HWTEST_F(OOMCommandQueueImageTest, WhenReadingImageThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : {OOMSetting{true, false}, OOMSetting{false, true}, OOMSetting{true, true}}) {
        CommandQueueHw<FamilyType> oomQueue(context, pClDevice, 0, false);
        applyOOMSetting(oomQueue, oomSetting);
        CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

        auto &commandStream = oomQueue.getCS(1024);
        auto &indirectHeap = oomQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        auto retVal1 = EnqueueReadImageHelper<>::enqueue(&oomQueue);
        auto retVal2 = EnqueueReadImageHelper<>::enqueue(&cmdQ);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        if (usedAfterISH > usedBeforeISH) {
            EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());
        } else {
            EXPECT_LE(usedAfterISH, indirectHeap.getMaxAvailableSpace());
        }

        EXPECT_EQ(CL_SUCCESS, retVal1);
        EXPECT_EQ(CL_SUCCESS, retVal2);
    }
}

HWTEST_F(OOMCommandQueueImageTest, WhenWritingImageThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : {OOMSetting{true, false}, OOMSetting{false, true}, OOMSetting{true, true}}) {
        CommandQueueHw<FamilyType> oomQueue(context, pClDevice, 0, false);
        applyOOMSetting(oomQueue, oomSetting);
        CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

        auto &commandStream = oomQueue.getCS(1024);
        auto &indirectHeap = oomQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        auto retVal1 = EnqueueWriteImageHelper<>::enqueue(&oomQueue);
        auto retVal2 = EnqueueWriteImageHelper<>::enqueue(&cmdQ);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        if (usedAfterISH > usedBeforeISH) {
            EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());
        } else {
            EXPECT_LE(usedAfterISH, indirectHeap.getMaxAvailableSpace());
        }

        EXPECT_EQ(CL_SUCCESS, retVal1);
        EXPECT_EQ(CL_SUCCESS, retVal2);
    }
}
