/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

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
                                              AllocationType::INTERNAL_HOST_MEMORY,
                                              ptr,
                                              size,
                                              0u,
                                              MemoryPool::System4KBPages,
                                              MemoryManager::maxOsContextCount,
                                              canonizedGpuAddress);
        uint32_t patternToCommand[4];
        memset(patternToCommand, 4, patternSize);
        BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, patternToCommand, patternSize, stream, mockAllocation.getUnderlyingBufferSize(), *device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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
