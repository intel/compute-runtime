/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"

#include "test_mode.h"

namespace L0 {
namespace ult {

class AUBHelloWorldL0 : public AUBFixtureL0,
                        public ::testing::Test {
  protected:
    void SetUp() override {
        AUBFixtureL0::SetUp(NEO::defaultHwInfo.get());
    }
    void TearDown() override {
        AUBFixtureL0::TearDown();
    }
};

TEST_F(AUBHelloWorldL0, whenAppendMemoryCopyIsCalledThenMemoryIsProperlyCopied) {
    uint8_t size = 8;
    uint8_t val = 255;

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY,
                                                                           context->rootDeviceIndices,
                                                                           context->deviceBitfields);

    auto srcMemory = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    auto dstMemory = driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    memset(srcMemory, val, size);
    commandList->appendMemoryCopy(dstMemory, srcMemory, size, 0, 0, nullptr);
    commandList->close();
    auto pHCmdList = std::make_unique<ze_command_list_handle_t>(commandList->toHandle());

    pCmdq->executeCommandLists(1, pHCmdList.get(), nullptr, false);
    pCmdq->synchronize(std::numeric_limits<uint32_t>::max());

    EXPECT_TRUE(csr->expectMemory(dstMemory, srcMemory, val, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(srcMemory);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstMemory);
}

} // namespace ult
} // namespace L0