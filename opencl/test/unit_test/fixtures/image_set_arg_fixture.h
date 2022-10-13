/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"
namespace NEO {
class Kernel;
class Image;

class ImageSetArgTest : public ClDeviceFixture,
                        public testing::Test {

  public:
    ImageSetArgTest() = default;

  protected:
    template <typename FamilyType>
    void setupChannels(int imgChannelOrder) {
        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

        expectedChannelRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED;
        expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN;
        expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE;

        if (imgChannelOrder == CL_A) {
            expectedChannelRed = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        } else if (imgChannelOrder == CL_RA ||
                   imgChannelOrder == CL_R ||
                   imgChannelOrder == CL_Rx) {
            expectedChannelGreen = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        } else if (imgChannelOrder == CL_RG ||
                   imgChannelOrder == CL_RGx) {
            expectedChannelBlue = RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO;
        }
    }

    void SetUp() override;

    void TearDown() override;

    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    std::unique_ptr<MockProgram> program;
    MockKernel *pKernel = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char surfaceStateHeap[0x80] = {};
    Image *srcImage = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    int expectedChannelRed = 0;
    int expectedChannelGreen = 0;
    int expectedChannelBlue = 0;
};

class ImageMediaBlockSetArgTest : public ImageSetArgTest {
  protected:
    void SetUp() override;
};
} // namespace NEO