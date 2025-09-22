/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstddef>

using namespace NEO;

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenHostFunctionDataStoredWhenProgramHostFunctionIsCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    constexpr auto size = 1024u;
    std::byte buff[size] = {};
    LinearStream stream(buff, size);

    uint64_t userHostFunctionStored = 10u;
    uint64_t userDataStored = 20u;
    uint32_t tagStored = 0;

    HostFunctionData hostFunctionData{
        .entry = &userHostFunctionStored,
        .userData = &userDataStored,
        .internalTag = &tagStored};

    uint64_t userCallback = 0xAAAA'0000ull;
    uint64_t userCallbackData = 0xBBBB'000ull;

    HostFunctionHelper::programHostFunction<FamilyType>(stream, hostFunctionData, userCallback, userCallbackData);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);

    auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(3u, miStores.size());

    auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miWait.size());

    // program callback address
    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&userHostFunctionStored), miStoreUserHostFunction->getAddress());
    EXPECT_EQ(getLowPart(userCallback), miStoreUserHostFunction->getDataDword0());
    EXPECT_EQ(getHighPart(userCallback), miStoreUserHostFunction->getDataDword1());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    // program callback data
    auto miStoreUserData = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[1]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&userDataStored), miStoreUserData->getAddress());
    EXPECT_EQ(getLowPart(userCallbackData), miStoreUserData->getDataDword0());
    EXPECT_EQ(getHighPart(userCallbackData), miStoreUserData->getDataDword1());
    EXPECT_TRUE(miStoreUserData->getStoreQword());

    // signal pending job
    auto miStoreSignalTag = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[2]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&tagStored), miStoreSignalTag->getAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::pending), miStoreSignalTag->getDataDword0());
    EXPECT_FALSE(miStoreSignalTag->getStoreQword());

    // wait for completion
    auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&tagStored), miWaitTag->getSemaphoreGraphicsAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::completed), miWaitTag->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());
}

HWTEST_F(HostFunctionTests, givenCommandBufferPassedWhenProgramHostFunctionsAreCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    constexpr auto size = 1024u;
    std::byte buff[size] = {};

    uint64_t userHostFunctionStored = 10u;
    uint64_t userDataStored = 20u;
    uint32_t tagStored = 0;

    HostFunctionData hostFunctionData{
        .entry = &userHostFunctionStored,
        .userData = &userDataStored,
        .internalTag = &tagStored};

    uint64_t userCallback = 0xAAAA'0000ull;
    uint64_t userCallbackData = 0xBBBB'000ull;

    LinearStream commandStream(buff, size);
    auto miStoreDataImmBuffer1 = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    HostFunctionHelper::programHostFunctionAddress<FamilyType>(nullptr, miStoreDataImmBuffer1, hostFunctionData, userCallback);

    auto miStoreDataImmBuffer2 = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    HostFunctionHelper::programHostFunctionUserData<FamilyType>(nullptr, miStoreDataImmBuffer2, hostFunctionData, userCallbackData);

    auto miStoreDataImmBuffer3 = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    HostFunctionHelper::programSignalHostFunctionStart<FamilyType>(nullptr, miStoreDataImmBuffer3, hostFunctionData);

    auto semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    HostFunctionHelper::programWaitForHostFunctionCompletion<FamilyType>(nullptr, semaphoreCommand, hostFunctionData);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(3u, miStores.size());

    auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miWait.size());

    // program callback address
    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&userHostFunctionStored), miStoreUserHostFunction->getAddress());
    EXPECT_EQ(getLowPart(userCallback), miStoreUserHostFunction->getDataDword0());
    EXPECT_EQ(getHighPart(userCallback), miStoreUserHostFunction->getDataDword1());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    // program callback data
    auto miStoreUserData = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[1]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&userDataStored), miStoreUserData->getAddress());
    EXPECT_EQ(getLowPart(userCallbackData), miStoreUserData->getDataDword0());
    EXPECT_EQ(getHighPart(userCallbackData), miStoreUserData->getDataDword1());
    EXPECT_TRUE(miStoreUserData->getStoreQword());

    // signal pending job
    auto miStoreSignalTag = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[2]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&tagStored), miStoreSignalTag->getAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::pending), miStoreSignalTag->getDataDword0());
    EXPECT_FALSE(miStoreSignalTag->getStoreQword());

    // wait for completion
    auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&tagStored), miWaitTag->getSemaphoreGraphicsAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::completed), miWaitTag->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());
}
