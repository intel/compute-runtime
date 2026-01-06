/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

struct BlitColorTests : public DeviceFixture, public testing::TestWithParam<size_t> {
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

template <typename FamilyType>
class GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed {
  public:
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed(Device *device) : device(device) {}
    void testBodyImpl(size_t patternSize, COLOR_DEPTH expectedDepth) {
        uint32_t streamBuffer[100] = {};
        LinearStream stream(streamBuffer, sizeof(streamBuffer));
        auto size = 0x1000;
        auto ptr = reinterpret_cast<void *>(0x1234);
        auto gmmHelper = device->getGmmHelper();
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
        MockGraphicsAllocation mockAllocation(0,
                                              1u /*num gmms*/,
                                              AllocationType::internalHostMemory,
                                              ptr,
                                              size,
                                              0u,
                                              MemoryPool::system4KBPages,
                                              MemoryManager::maxOsContextCount,
                                              canonizedGpuAddress);
        uint32_t patternToCommand[4];
        memset(patternToCommand, 4, patternSize);
        auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), patternToCommand, patternSize, 0);

        BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, device->getRootDeviceEnvironmentRef());
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
        auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
        {
            auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
            EXPECT_EQ(expectedDepth, cmd->getColorDepth());
        }
    }
    Device *device;
};
