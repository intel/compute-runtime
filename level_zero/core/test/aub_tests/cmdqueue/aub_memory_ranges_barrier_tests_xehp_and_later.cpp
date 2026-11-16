/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/helpers/constants.h"
#include "shared/test/common/helpers/cmd_buffer_validator.h"
#include "shared/test/common/helpers/test_traits.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <cstring>
#include <string>
#include <vector>

namespace L0 {
namespace ult {

struct L3ControlSupportedL0Matcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (NEO::HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::l3ControlSupported;
        }
        return false;
    }
};

struct MemoryRangesBarrierL0AubFixture : public AUBFixtureL0 {
    void setUp() {
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        AUBFixtureL0::setUp();
    }
};

using MemoryRangesBarrierL0AubTests = Test<MemoryRangesBarrierL0AubFixture>;

HWTEST2_F(MemoryRangesBarrierL0AubTests, givenAllocationsWhenAppendMemoryRangesBarrierThenL3FlushCommandIsSubmitted, L3ControlSupportedL0Matcher) {
    using L3_CONTROL = typename FamilyType::L3_CONTROL;

    constexpr size_t bufferSize = MemoryConstants::pageSize;

    auto bufferAMemory = allocateHostMemory(bufferSize, 1);
    auto bufferBMemory = allocateHostMemory(bufferSize, 1);
    memset(bufferAMemory, 1, bufferSize);
    memset(bufferBMemory, 129, bufferSize);

    auto srcBuffer = allocateDeviceMemory(bufferSize, MemoryConstants::pageSize);
    auto dstBuffer1 = allocateDeviceMemory(bufferSize, MemoryConstants::pageSize);
    auto dstBuffer2 = allocateDeviceMemory(bufferSize, MemoryConstants::pageSize);

    auto cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, srcBuffer, bufferAMemory, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer1, bufferBMemory, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer2, bufferBMemory, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdListHandle, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer1, srcBuffer, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer2, srcBuffer, bufferSize, nullptr, 0, nullptr));

    const void *ranges[] = {dstBuffer1, dstBuffer2};
    const size_t rangeSizes[] = {bufferSize, bufferSize};

    auto &cmdStream = *commandList->getCmdContainer().getCommandStream();
    auto sizeUsed = cmdStream.getUsed();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryRangesBarrier(cmdListHandle, 2, rangeSizes, ranges, nullptr, 0, nullptr));

    std::string err;
    auto cmdBuffOk = NEO::expectCmdBuff<FamilyType>(cmdStream, sizeUsed,
                                                    std::vector<NEO::MatchCmd *>{
                                                        new NEO::MatchAnyCmd(NEO::anyNumber),
                                                        new NEO::MatchHwCmd<FamilyType, L3_CONTROL>(1),
                                                        new NEO::MatchAnyCmd(NEO::anyNumber),
                                                    },
                                                    &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer2, srcBuffer, bufferSize, nullptr, 0, nullptr));
    executeAndSynchronize();

    expectMemory<FamilyType>(dstBuffer1, bufferAMemory, bufferSize);
    expectMemory<FamilyType>(dstBuffer2, bufferAMemory, bufferSize);
}

} // namespace ult
} // namespace L0
