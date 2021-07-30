/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

TEST(DebugSession, givenThreadWhenIsThreadAllCalledThenTrueReturnedOnlyForAllValuesEqualMax) {
    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_TRUE(DebugSession::isThreadAll(thread));

    thread = {0, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isThreadAll(thread));
    thread = {UINT32_MAX, 0, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isThreadAll(thread));
    thread = {UINT32_MAX, UINT32_MAX, 0, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isThreadAll(thread));
    thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, 0};
    EXPECT_FALSE(DebugSession::isThreadAll(thread));
}

TEST(DebugSession, givenThreadWhenIsSingleThreadCalledThenTrueReturnedOnlyForNonMaxValues) {
    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {0, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, 0, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, UINT32_MAX, 0, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, 0};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, UINT32_MAX, 0, 0};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, 0, 0, 0};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, 0, UINT32_MAX, 0};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {0, UINT32_MAX, 0, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {UINT32_MAX, 0, 0, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {0, UINT32_MAX, UINT32_MAX, 0};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {0, 0, UINT32_MAX, 0};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {0, 0, 0, UINT32_MAX};
    EXPECT_FALSE(DebugSession::isSingleThread(thread));

    thread = {1, 2, 3, 0};
    EXPECT_TRUE(DebugSession::isSingleThread(thread));
}

TEST(DebugSession, givenThreadsWhenAreThreadsEqualCalledThenTrueReturnedForEqualThreads) {
    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    ze_device_thread_t thread2 = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

    EXPECT_TRUE(DebugSession::areThreadsEqual(thread, thread2));

    thread = {0, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::areThreadsEqual(thread, thread2));
    thread2 = {UINT32_MAX, 0, UINT32_MAX, UINT32_MAX};
    EXPECT_FALSE(DebugSession::areThreadsEqual(thread, thread2));

    thread = {1, 0, 0, 0};
    thread2 = {1, 0, 0, UINT32_MAX};
    EXPECT_FALSE(DebugSession::areThreadsEqual(thread, thread2));

    thread2 = {1, 0, 1, 0};
    EXPECT_FALSE(DebugSession::areThreadsEqual(thread, thread2));

    thread2 = {1, 1, 0, 0};
    EXPECT_FALSE(DebugSession::areThreadsEqual(thread, thread2));

    thread2 = {0, 0, 0, 0};
    EXPECT_FALSE(DebugSession::areThreadsEqual(thread, thread2));
    {
        ze_device_thread_t thread = {1, 1, 1, 1};
        ze_device_thread_t thread2 = {1, 1, 1, 1};

        EXPECT_TRUE(DebugSession::areThreadsEqual(thread, thread2));
    }
}

TEST(DebugSession, givenThreadWhenCheckSingleThreadWithinDeviceThreadCalledThenTrueReturnedForMatchingThread) {
    ze_device_thread_t thread = {0, 1, 2, 3};
    ze_device_thread_t thread2 = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

    EXPECT_TRUE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 2, 3};
    thread2 = {0, UINT32_MAX, UINT32_MAX, UINT32_MAX};

    EXPECT_TRUE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 2, 3};
    thread2 = {0, 1, 2, 3};

    EXPECT_TRUE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 2, 3};
    thread2 = {0, UINT32_MAX, UINT32_MAX, 4};

    EXPECT_FALSE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 2, 3};
    thread2 = {0, 1, 2, 4};

    EXPECT_FALSE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 2, 3};
    thread2 = {1, 1, 2, 3};

    EXPECT_FALSE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 2, 3};
    thread2 = {0, 2, 2, 3};

    EXPECT_FALSE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 3, 3};
    thread2 = {0, 1, 2, 3};

    EXPECT_FALSE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));

    thread = {0, 1, 3, 3};
    thread2 = {UINT32_MAX, 0, 2, 3};

    EXPECT_FALSE(DebugSession::checkSingleThreadWithinDeviceThread(thread, thread2));
}

TEST(DebugSession, givenSingleThreadWhenGettingSingleThreadsThenCorrectThreadIsReturned) {
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

TEST(DebugSession, givenAllThreadsWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
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

TEST(DebugSession, givenAllEUsWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
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

TEST(DebugSession, givenAllSubslicesWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
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

TEST(DebugSession, givenAllSlicesWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
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

TEST(DebugSession, givenBindlessSystemRoutineWhenQueryingIsBindlessThenTrueReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    debugSession->debugArea.reserved1 = 1u;

    EXPECT_TRUE(debugSession->isBindlessSystemRoutine());
}

TEST(DebugSession, givenBindfulSystemRoutineWhenQueryingIsBindlessThenFalseReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    debugSession->debugArea.reserved1 = 0u;

    EXPECT_FALSE(debugSession->isBindlessSystemRoutine());
}

TEST(DebugSession, givenApiThreadAndSingleTileWhenConvertingThenCorrectValuesReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);

    ze_device_thread_t thread = {hwInfo.gtSystemInfo.SliceCount - 1, hwInfo.gtSystemInfo.SubSliceCount - 1, 0, 0};

    uint32_t deviceIndex = 0;

    auto convertedThread = debugSession->convertToPhysical(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(convertedThread.slice, thread.slice);
    EXPECT_EQ(convertedThread.subslice, thread.subslice);
    EXPECT_EQ(convertedThread.eu, thread.eu);
    EXPECT_EQ(convertedThread.thread, thread.thread);

    thread.slice = UINT32_MAX;
    convertedThread = debugSession->convertToPhysical(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(convertedThread.slice, thread.slice);
}

using DebugSessionMultiTile = Test<MultipleDevicesWithCustomHwInfo>;

TEST_F(DebugSessionMultiTile, givenApiThreadAndMultipleTilesWhenConvertingToPhysicalThenCorrectValueReturned) {
    L0::Device *device = driverHandle->devices[0];

    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, device);

    ze_device_thread_t thread = {sliceCount * 2 - 1, 0, 0, 0};

    uint32_t deviceIndex = 0;

    auto convertedThread = debugSession->convertToPhysical(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread = {sliceCount - 1, 0, 0, 0};
    convertedThread = debugSession->convertToPhysical(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread.slice = UINT32_MAX;
    convertedThread = debugSession->convertToPhysical(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(convertedThread.slice, thread.slice);

    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, deviceImp->subDevices[1]);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = 10;
    convertedThread = debugSession->convertToPhysical(thread, deviceIndex);
    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);
}

TEST_F(DebugSessionMultiTile, WhenConvertingToThreadIdAndBackThenCorrectThreadIdsAreReturned) {
    L0::Device *device = driverHandle->devices[0];

    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, device);

    ze_device_thread_t thread = {sliceCount * 2 - 1, 0, 0, 0};

    auto threadID = debugSession->convertToThreadId(thread);

    EXPECT_EQ(1u, threadID.tileIndex);
    EXPECT_EQ(sliceCount - 1, threadID.slice);
    EXPECT_EQ(thread.subslice, threadID.subslice);
    EXPECT_EQ(thread.eu, threadID.eu);
    EXPECT_EQ(thread.thread, threadID.thread);

    auto apiThread = debugSession->convertToApi(threadID);

    EXPECT_EQ(thread.slice, apiThread.slice);
    EXPECT_EQ(thread.subslice, apiThread.subslice);
    EXPECT_EQ(thread.eu, apiThread.eu);
    EXPECT_EQ(thread.thread, apiThread.thread);

    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, deviceImp->subDevices[1]);

    thread = {sliceCount - 1, 0, 0, 0};

    threadID = debugSession->convertToThreadId(thread);

    EXPECT_EQ(1u, threadID.tileIndex);
    EXPECT_EQ(sliceCount - 1, threadID.slice);
    EXPECT_EQ(thread.subslice, threadID.subslice);
    EXPECT_EQ(thread.eu, threadID.eu);
    EXPECT_EQ(thread.thread, threadID.thread);

    apiThread = debugSession->convertToApi(threadID);

    EXPECT_EQ(thread.slice, apiThread.slice);
    EXPECT_EQ(thread.subslice, apiThread.subslice);
    EXPECT_EQ(thread.eu, apiThread.eu);
    EXPECT_EQ(thread.thread, apiThread.thread);
}

} // namespace ult
} // namespace L0
