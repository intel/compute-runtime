/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

struct OOMSetting {
    bool oomCS;
    bool oomISH;
};

static OOMSetting oomSettings[] = {
    {true, false},
    {false, true},
    {true, true}};

struct OOMCommandQueueImageTest : public ClDeviceFixture,
                                  public CommandQueueFixture,
                                  public ::testing::TestWithParam<OOMSetting> {

    using CommandQueueFixture::SetUp;

    OOMCommandQueueImageTest() {
    }

    void SetUp() override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        ClDeviceFixture::SetUp();
        context = new MockContext(pClDevice);
        CommandQueueFixture::SetUp(context, pClDevice, 0);

        srcImage = Image2dHelper<>::create(context);
        dstImage = Image2dHelper<>::create(context);

        const auto &oomSetting = GetParam();
        auto oomSize = 10u;
        if (oomSetting.oomCS) {
            auto &cs = pCmdQ->getCS(oomSize);

            // CommandStream may be larger than requested so grab what wasnt requested
            cs.getSpace(cs.getAvailableSpace() - oomSize);
            ASSERT_EQ(oomSize, cs.getAvailableSpace());
        }

        if (oomSetting.oomISH) {
            auto &ish = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, oomSize);

            // IndirectHeap may be larger than requested so grab what wasnt requested
            ish.getSpace(ish.getAvailableSpace() - oomSize);
            ASSERT_EQ(oomSize, ish.getAvailableSpace());
        }
    }

    void TearDown() override {
        if (IsSkipped()) {
            return;
        }
        delete dstImage;
        delete srcImage;
        context->release();

        CommandQueueFixture::TearDown();
        ClDeviceFixture::TearDown();
    }

    MockContext *context;
    Image *srcImage = nullptr;
    Image *dstImage = nullptr;
};

HWTEST_P(OOMCommandQueueImageTest, WhenCopyingImageThenMaxAvailableSpaceIsNotExceeded) {
    CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 10);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeISH = indirectHeap.getUsed();

    auto retVal1 = EnqueueCopyImageHelper<>::enqueue(pCmdQ);
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

HWTEST_P(OOMCommandQueueImageTest, WhenFillingImageThenMaxAvailableSpaceIsNotExceeded) {
    CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 10);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeISH = indirectHeap.getUsed();

    auto retVal1 = EnqueueFillImageHelper<>::enqueue(pCmdQ);
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

HWTEST_P(OOMCommandQueueImageTest, WhenReadingImageThenMaxAvailableSpaceIsNotExceeded) {
    CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 10);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeISH = indirectHeap.getUsed();

    auto retVal1 = EnqueueReadImageHelper<>::enqueue(pCmdQ);
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

HWTEST_P(OOMCommandQueueImageTest, WhenWritingImageThenMaxAvailableSpaceIsNotExceeded) {
    CommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0, false);

    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 10);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeISH = indirectHeap.getUsed();

    auto retVal1 = EnqueueWriteImageHelper<>::enqueue(pCmdQ);
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

INSTANTIATE_TEST_CASE_P(
    OOM,
    OOMCommandQueueImageTest,
    testing::ValuesIn(oomSettings));
