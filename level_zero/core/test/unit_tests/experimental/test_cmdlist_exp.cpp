/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

using namespace NEO;

namespace L0 {

namespace ult {

using CommandListExpTest = Test<DeviceFixture>;

HWTEST_F(CommandListExpTest, givenCmdListWithSimulatedCsrWhenVerifyMemoryCalledThenExpectMemoryIsCalled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    MockAubCsr<FamilyType> mockCommandStreamReceiver("", true, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.callBaseExpectMemory = false;
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    void *ptr = reinterpret_cast<void *>(0x1000);
    char data[10] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCommandListVerifyMemory(commandList->toHandle(), ptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_TRUE(mockCommandStreamReceiver.expectMemoryCalled);

    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListExpTest, givenCmdListWithSimulatedCsrWhenVerifyMemoryCalledWithInvalidParamsThenErrorIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    MockAubCsr<FamilyType> mockCommandStreamReceiver("", true, *neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.callBaseExpectMemory = false;
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    void *ptr = reinterpret_cast<void *>(0x1000);
    char data[10] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, zexCommandListVerifyMemory(nullptr, ptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, zexCommandListVerifyMemory(commandList->toHandle(), nullptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, zexCommandListVerifyMemory(commandList->toHandle(), ptr, nullptr, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zexCommandListVerifyMemory(commandList->toHandle(), ptr, data, 0, zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
    EXPECT_FALSE(mockCommandStreamReceiver.expectMemoryCalled);

    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListExpTest, givenRegularCmdListWithSimulatedCsrWhenVerifyMemoryCalledThenExpectMemoryIsCalled) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0, returnValue, false));
    void *ptr = reinterpret_cast<void *>(0x1000);
    char data[10] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zexCommandListVerifyMemory(commandList->toHandle(), ptr, data, sizeof(data), zex_verify_memory_compare_type_t::ZEX_VERIFY_MEMORY_COMPARE_EQUAL));
}

} // namespace ult
} // namespace L0
