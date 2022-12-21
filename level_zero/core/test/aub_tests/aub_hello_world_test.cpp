/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "test_mode.h"

namespace L0 {
namespace ult {

using AUBHelloWorldL0 = Test<AUBFixtureL0>;
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

    EXPECT_TRUE(csr->expectMemory(dstMemory, srcMemory, size, AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual));

    driverHandle->svmAllocsManager->freeSVMAlloc(srcMemory);
    driverHandle->svmAllocsManager->freeSVMAlloc(dstMemory);
}

} // namespace ult
} // namespace L0
