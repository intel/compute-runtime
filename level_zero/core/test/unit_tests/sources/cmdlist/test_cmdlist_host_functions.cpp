/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include <level_zero/driver_experimental/zex_cmdlist.h>

#include <ranges>

namespace L0 {
namespace ult {

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenRegularCommandListWhenZexCommandListAppendHostFunctionIsCalledThenUnsupportedFeatureIsReturned) {

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

    auto result =
        zexCommandListAppendHostFunction(commandList->toHandle(), nullptr, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST_F(HostFunctionTests, givenImmediateCommandListWhenZexCommandListAppendHostFunctionIsCalledThenUnsupportedFeatureIsReturned) {

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    auto result =
        zexCommandListAppendHostFunction(commandList->toHandle(), nullptr, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST_F(HostFunctionTests, givenRegularCmdListWhenDispatchHostFunctionIsCalledThenCommandsToPatchAreSet) {

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    commandList->dispatchHostFunction(pHostFunction, pUserData);

    ASSERT_EQ(4u, commandList->commandsToPatch.size());

    EXPECT_EQ(CommandToPatch::HostFunctionEntry, commandList->commandsToPatch[0].type);
    EXPECT_EQ(reinterpret_cast<uint64_t>(pHostFunction), commandList->commandsToPatch[0].baseAddress);
    EXPECT_NE(nullptr, commandList->commandsToPatch[0].pCommand);

    EXPECT_EQ(CommandToPatch::HostFunctionUserData, commandList->commandsToPatch[1].type);
    EXPECT_EQ(reinterpret_cast<uint64_t>(pUserData), commandList->commandsToPatch[1].baseAddress);
    EXPECT_NE(nullptr, commandList->commandsToPatch[1].pCommand);

    EXPECT_EQ(CommandToPatch::HostFunctionSignalInternalTag, commandList->commandsToPatch[2].type);
    EXPECT_NE(nullptr, commandList->commandsToPatch[2].pCommand);

    EXPECT_EQ(CommandToPatch::HostFunctionWaitInternalTag, commandList->commandsToPatch[3].type);
    EXPECT_NE(nullptr, commandList->commandsToPatch[3].pCommand);
}

HWTEST_F(HostFunctionTests, givenImmediateCmdListWhenDispatchHostFunctionIscalledThenCorrectCommandsAreProgrammedAndHostFunctionDataWasInitializedInCsr) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue)));

    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    uint64_t hostFunctionAddress = reinterpret_cast<uint64_t>(pHostFunction);

    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    uint64_t userDataAddress = reinterpret_cast<uint64_t>(pUserData);

    auto *cmdStream = commandList->commandContainer.getCommandStream();

    commandList->dispatchHostFunction(pHostFunction, pUserData);

    auto csr = commandList->getCsr(false);

    auto *hostFunctionAllocation = csr->getHostFunctionDataAllocation();
    ASSERT_NE(nullptr, hostFunctionAllocation);

    auto &hostFunctionData = csr->getHostFunctionData();

    auto &residencyContainer = commandList->commandContainer.getResidencyContainer();
    EXPECT_TRUE(std::ranges::find(residencyContainer, hostFunctionAllocation) != residencyContainer.end());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdStream, 0);

    auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(3u, miStores.size());

    auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miWait.size());

    // program callback address
    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionData.entry), miStoreUserHostFunction->getAddress());
    EXPECT_EQ(getLowPart(hostFunctionAddress), miStoreUserHostFunction->getDataDword0());
    EXPECT_EQ(getHighPart(hostFunctionAddress), miStoreUserHostFunction->getDataDword1());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    // program callback data
    auto miStoreUserData = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[1]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionData.userData), miStoreUserData->getAddress());
    EXPECT_EQ(getLowPart(userDataAddress), miStoreUserData->getDataDword0());
    EXPECT_EQ(getHighPart(userDataAddress), miStoreUserData->getDataDword1());
    EXPECT_TRUE(miStoreUserData->getStoreQword());

    // signal pending job
    auto miStoreSignalTag = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[2]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionData.internalTag), miStoreSignalTag->getAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::pending), miStoreSignalTag->getDataDword0());
    EXPECT_FALSE(miStoreSignalTag->getStoreQword());

    // wait for completion
    auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionData.internalTag), miWaitTag->getSemaphoreGraphicsAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::completed), miWaitTag->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());
}

} // namespace ult
} // namespace L0
