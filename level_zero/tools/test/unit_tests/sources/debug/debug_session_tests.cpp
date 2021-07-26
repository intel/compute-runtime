/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

TEST(RootDebugSession, givenSingleThreadWhenGettingSingleThreadsThenCorrectThreadIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    auto subslice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    ze_device_thread_t physicalThread = {0, subslice, 2, 3};

    auto threads = debugSession->getSingleThreads(physicalThread, hwInfo);

    EXPECT_EQ(1u, threads.size());
    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subslice, threads[0].subslice);
    EXPECT_EQ(2u, threads[0].eu);
    EXPECT_EQ(3u, threads[0].thread);
}

TEST(RootDebugSession, givenAllThreadsWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    auto subslice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    ze_device_thread_t physicalThread = {0, subslice, 2, UINT32_MAX};
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    auto threads = debugSession->getSingleThreads(physicalThread, hwInfo);

    EXPECT_EQ(numThreadsPerEu, threads.size());

    for (uint32_t i = 0; i < numThreadsPerEu; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(subslice, threads[i].subslice);
        EXPECT_EQ(2u, threads[i].eu);
        EXPECT_EQ(i, threads[i].thread);
    }
}

TEST(RootDebugSession, givenAllEUsWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    auto subslice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    ze_device_thread_t physicalThread = {0, subslice, UINT32_MAX, 0};
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    auto threads = debugSession->getSingleThreads(physicalThread, hwInfo);

    EXPECT_EQ(numEuPerSubslice, threads.size());

    for (uint32_t i = 0; i < numEuPerSubslice; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(subslice, threads[i].subslice);
        EXPECT_EQ(i, threads[i].eu);
        EXPECT_EQ(0u, threads[i].thread);
    }
}

TEST(RootDebugSession, givenAllSubslicesWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    ze_device_thread_t physicalThread = {0, UINT32_MAX, 0, 0};
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;

    auto threads = debugSession->getSingleThreads(physicalThread, hwInfo);

    EXPECT_EQ(numSubslicesPerSlice, threads.size());

    for (uint32_t i = 0; i < numSubslicesPerSlice; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(i, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(0u, threads[i].thread);
    }
}

TEST(RootDebugSession, givenAllSlicesWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    ze_device_thread_t physicalThread = {UINT32_MAX, 0, 0, 0};
    const uint32_t numSlices = hwInfo.gtSystemInfo.MaxSlicesSupported;

    auto threads = debugSession->getSingleThreads(physicalThread, hwInfo);

    EXPECT_EQ(numSlices, threads.size());

    for (uint32_t i = 0; i < numSlices; i++) {
        EXPECT_EQ(i, threads[i].slice);
        EXPECT_EQ(0u, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(0u, threads[i].thread);
    }
}

TEST(RootDebugSession, givenBindlessSystemRoutineWhenQueryingIsBindlessThenTrueReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    debugSession->debugArea.reserved1 = 1u;

    EXPECT_TRUE(debugSession->isBindlessSystemRoutine());
}

TEST(RootDebugSession, givenBindfulSystemRoutineWhenQueryingIsBindlessThenFalseReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    debugSession->debugArea.reserved1 = 0u;

    EXPECT_FALSE(debugSession->isBindlessSystemRoutine());
}

} // namespace ult
} // namespace L0
