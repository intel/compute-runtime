/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

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
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    auto subslice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    ze_device_thread_t physicalThread = {0, subslice, 2, 3};

    auto threads = debugSession->getSingleThreadsForDevice(0, physicalThread, hwInfo);

    EXPECT_EQ(1u, threads.size());

    EXPECT_EQ(0u, threads[0].tileIndex);
    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subslice, threads[0].subslice);
    EXPECT_EQ(2u, threads[0].eu);
    EXPECT_EQ(3u, threads[0].thread);
}

TEST(DebugSession, givenAllThreadsWithLowSliceDisabledWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.gtSystemInfo.MaxSlicesSupported = 2;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    hwInfo.gtSystemInfo.SliceInfo[2].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    ze_device_thread_t physicalThread = {UINT32_MAX, 0, 0, 0};

    auto threads = debugSession->getSingleThreadsForDevice(0, physicalThread, hwInfo);

    EXPECT_EQ(2u, threads.size());

    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(0u, threads[i].tileIndex);
        EXPECT_EQ(i + 2, threads[i].slice);
        EXPECT_EQ(0u, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(0u, threads[i].thread);
    }
}

TEST(DebugSession, givenAllThreadsWhenGettingSingleThreadsThenCorrectThreadsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    auto subslice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    ze_device_thread_t physicalThread = {0, subslice, 2, UINT32_MAX};
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    auto threads = debugSession->getSingleThreadsForDevice(0, physicalThread, hwInfo);

    EXPECT_EQ(numThreadsPerEu, threads.size());

    for (uint32_t i = 0; i < numThreadsPerEu; i++) {
        EXPECT_EQ(0u, threads[i].tileIndex);
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
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    auto subslice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    ze_device_thread_t physicalThread = {0, subslice, UINT32_MAX, 0};
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    auto threads = debugSession->getSingleThreadsForDevice(0, physicalThread, hwInfo);

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
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    ze_device_thread_t physicalThread = {0, UINT32_MAX, 0, 0};
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;

    auto threads = debugSession->getSingleThreadsForDevice(0, physicalThread, hwInfo);

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
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    ze_device_thread_t physicalThread = {UINT32_MAX, 0, 0, 0};
    const uint32_t numSlices = neoDevice->getGfxCoreHelper().getHighestEnabledSlice(hwInfo);

    auto threads = debugSession->getSingleThreadsForDevice(0, physicalThread, hwInfo);

    EXPECT_EQ(numSlices, threads.size());

    for (uint32_t i = 0; i < numSlices; i++) {
        EXPECT_EQ(0u, threads[i].tileIndex);
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
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    debugSession->debugArea.reserved1 = 1u;

    EXPECT_TRUE(debugSession->isBindlessSystemRoutine());
}

TEST(DebugSession, givenBindfulSystemRoutineWhenQueryingIsBindlessThenFalseReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);

    debugSession->debugArea.reserved1 = 0u;

    EXPECT_FALSE(debugSession->isBindlessSystemRoutine());
}

TEST(DebugSession, givenApiThreadAndSingleTileWhenConvertingThenCorrectValuesReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);

    ze_device_thread_t thread = {hwInfo.gtSystemInfo.SliceCount - 1, hwInfo.gtSystemInfo.SubSliceCount - 1, 0, 0};

    uint32_t deviceIndex = 0;

    auto convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(convertedThread.slice, thread.slice);
    EXPECT_EQ(convertedThread.subslice, thread.subslice);
    EXPECT_EQ(convertedThread.eu, thread.eu);
    EXPECT_EQ(convertedThread.thread, thread.thread);

    thread.slice = UINT32_MAX;
    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    if (hwInfo.gtSystemInfo.SliceCount == 1) {
        EXPECT_EQ(convertedThread.slice, 0u);
    } else {
        EXPECT_EQ(convertedThread.slice, thread.slice);
    }
}

TEST(DebugSession, givenApiThreadAndSingleTileWhenGettingDeviceIndexThenCorrectIndexIsReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);

    ze_device_thread_t thread = {hwInfo.gtSystemInfo.SliceCount - 1, 0, 0, 0};

    uint32_t deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    thread.slice = UINT32_MAX;

    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);
}

TEST(DebugSession, givenAllStoppedThreadsWhenAreRequestedThreadsStoppedCalledThenTrueReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<DebugSessionMock>(config, &deviceImp);
    sessionMock->initialize();
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
    }

    ze_device_thread_t apiThread = {0, 0, 0, UINT32_MAX};
    EXPECT_TRUE(sessionMock->areRequestedThreadsStopped(apiThread));
}

TEST(DebugSession, givenSomeStoppedThreadsWhenAreRequestedThreadsStoppedCalledThenFalseReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<DebugSessionMock>(config, &deviceImp);
    sessionMock->initialize();
    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        if (i % 2) {
            sessionMock->allThreads[thread]->stopThread(1u);
            sessionMock->allThreads[thread]->reportAsStopped();
        }
    }

    ze_device_thread_t apiThread = {0, 0, 0, UINT32_MAX};
    EXPECT_FALSE(sessionMock->areRequestedThreadsStopped(apiThread));
}

TEST(DebugSession, givenApiThreadAndSingleTileWhenFillingDevicesThenVectorEntryIsSet) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);

    ze_device_thread_t thread = {hwInfo.gtSystemInfo.SliceCount - 1, hwInfo.gtSystemInfo.SubSliceCount - 1, 0, 0};

    std::vector<uint8_t> devices(1);
    debugSession->fillDevicesFromThread(thread, devices);

    EXPECT_EQ(1u, devices[0]);
}

TEST(DebugSession, givenDifferentCombinationsOfThreadsAndMemoryTypeCheckExpectedMemoryAccess) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<DebugSessionMock>(config, &deviceImp);
    sessionMock->initialize();
    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    zet_debug_memory_space_desc_t desc;
    desc.address = 0x1000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    ze_result_t retVal = sessionMock->sanityMemAccessThreadCheck(thread, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    retVal = sessionMock->sanityMemAccessThreadCheck(thread, &desc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread = {1, 1, UINT32_MAX, UINT32_MAX};
    retVal = sessionMock->sanityMemAccessThreadCheck(thread, &desc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, retVal);

    thread = {0, 0, 0, 1};
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    retVal = sessionMock->sanityMemAccessThreadCheck(thread, &desc);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread]->reportAsStopped();
    }

    retVal = sessionMock->sanityMemAccessThreadCheck(thread, &desc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
}

TEST(DebugSession, givenDifferentThreadsWhenGettingPerThreadScratchOffsetThenCorrectOffsetReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);
    auto &productHelper = neoDevice->getProductHelper();

    const uint32_t multiplyFactor = productHelper.getThreadEuRatioForScratch(hwInfo) / 8u;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) * multiplyFactor;
    EuThread::ThreadId thread0Eu0 = {0, 0, 0, 0, 0};
    EuThread::ThreadId thread0Eu1 = {0, 0, 0, 1, 0};
    EuThread::ThreadId thread2Subslice1 = {0, 0, 1, 0, 2};

    const uint32_t ptss = 128;

    auto size = debugSession->getPerThreadScratchOffset(ptss, thread0Eu0);
    EXPECT_EQ(0u, size);

    size = debugSession->getPerThreadScratchOffset(ptss, thread0Eu1);
    EXPECT_EQ(ptss * numThreadsPerEu, size);

    size = debugSession->getPerThreadScratchOffset(ptss, thread2Subslice1);
    EXPECT_EQ(2 * ptss + ptss * hwInfo.gtSystemInfo.MaxEuPerSubSlice * numThreadsPerEu, size);
}

TEST(DebugSession, GivenLogsEnabledWhenPrintBitmaskCalledThenBitmaskIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(255);

    ::testing::internal::CaptureStdout();

    uint64_t bitmask[2] = {0x404080808080, 0x1111ffff1111ffff};
    DebugSession::printBitmask(reinterpret_cast<uint8_t *>(bitmask), sizeof(bitmask));

    auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_TRUE(hasSubstr(output, std::string("\nINFO: Bitmask: ")));
    EXPECT_TRUE(hasSubstr(output, std::string("[0] = 0x0000404080808080")));
    EXPECT_TRUE(hasSubstr(output, std::string("[1] = 0x1111ffff1111ffff")));
}

TEST(DebugSession, GivenLogsDisabledWhenPrintBitmaskCalledThenBitmaskIsNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(0);

    ::testing::internal::CaptureStdout();

    uint64_t bitmask[2] = {0x404080808080, 0x1111ffff1111ffff};
    DebugSession::printBitmask(reinterpret_cast<uint8_t *>(bitmask), sizeof(bitmask));

    auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(0u, output.size());
}

TEST(DebugSession, WhenConvertingThreadIdsThenDeviceFunctionsAreCalled) {

    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {0, 0, 0, 0};

    auto threadID = sessionMock->convertToThreadId(thread);

    EXPECT_EQ(0u, threadID.tileIndex);
    EXPECT_EQ(0u, threadID.slice);
    EXPECT_EQ(0u, threadID.subslice);
    EXPECT_EQ(0u, threadID.eu);
    EXPECT_EQ(0u, threadID.thread);

    auto apiThread = sessionMock->convertToApi(threadID);

    EXPECT_EQ(0u, apiThread.slice);
    EXPECT_EQ(0u, apiThread.subslice);
    EXPECT_EQ(0u, apiThread.eu);
    EXPECT_EQ(0u, apiThread.thread);

    uint32_t deviceIndex = 1;

    auto physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(0u, physicalThread.slice);
    EXPECT_EQ(0u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = UINT32_MAX;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(uint32_t(UINT32_MAX), physicalThread.slice);
    EXPECT_EQ(0u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = 0;
    thread.subslice = UINT32_MAX;
    thread.eu = 1;
    thread.thread = 3;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(0u, physicalThread.slice);
    EXPECT_EQ(uint32_t(UINT32_MAX), physicalThread.subslice);
    EXPECT_EQ(1u, physicalThread.eu);
    EXPECT_EQ(3u, physicalThread.thread);
}

TEST(DebugSessionTest, WhenConvertingThreadIDsForDeviceWithSingleSliceThenSubsliceIsCorrectlyRemapped) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.gtSystemInfo.SliceCount = 2;
    hwInfo.gtSystemInfo.SubSliceCount = 8;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);
    auto sessionMock = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // fuse off first slice
    sessionMock->topologyMap[0].sliceIndices.erase(sessionMock->topologyMap[0].sliceIndices.begin());
    hwInfo.gtSystemInfo.SliceCount = 1;
    sessionMock->topologyMap[0].subsliceIndices.push_back(2);
    sessionMock->topologyMap[0].subsliceIndices.push_back(3);

    ze_device_thread_t thread = {UINT32_MAX, 1, 0, 0};
    uint32_t deviceIndex = 0;

    auto physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, physicalThread.slice);
    EXPECT_EQ(3u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = 0;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, physicalThread.slice);
    EXPECT_EQ(3u, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);
}

TEST(DebugSessionTest, WhenConvertingThreadIDsForDeviceWithMultipleSlicesThenSubsliceIsNotRemapped) {
    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.SliceCount = 8;
    hwInfo.gtSystemInfo.SubSliceCount = 8;

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    MockDeviceImp deviceImp(neoDevice);

    auto sessionMock = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    // fuse off first slice
    sessionMock->topologyMap[0].sliceIndices.erase(sessionMock->topologyMap[0].sliceIndices.begin());
    hwInfo.gtSystemInfo.SliceCount = 7;

    ze_device_thread_t thread = {UINT32_MAX, 1, 0, 0};
    uint32_t deviceIndex = 0;

    auto physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(UINT32_MAX, physicalThread.slice);
    EXPECT_EQ(thread.subslice, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);

    thread.slice = 0;
    physicalThread = sessionMock->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, physicalThread.slice);
    EXPECT_EQ(thread.subslice, physicalThread.subslice);
    EXPECT_EQ(0u, physicalThread.eu);
    EXPECT_EQ(0u, physicalThread.thread);
}

struct AffinityMaskMultipleSubdevices : MultipleDevicesWithCustomHwInfo {
    void setUp() {
        debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.1,0.3");
        MultipleDevicesWithCustomHwInfo::numSubDevices = 4;
        MultipleDevicesWithCustomHwInfo::setUp();
    }

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using AffinityMaskMultipleSubdevicesTest = Test<AffinityMaskMultipleSubdevices>;

TEST_F(AffinityMaskMultipleSubdevicesTest, givenApiThreadAndMultipleTilesWhenConvertingToPhysicalThenCorrectValuesReturned) {

    L0::Device *device = driverHandle->devices[0];
    auto deviceImp = static_cast<DeviceImp *>(device);

    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, deviceImp);
    ASSERT_NE(nullptr, debugSession);

    ze_device_thread_t thread = {2 * sliceCount - 1, 0, 0, 0};

    uint32_t deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(1u, deviceIndex);

    auto convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread = {3 * sliceCount - 1, 0, 0, 0};

    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(3u, deviceIndex);

    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(3u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread = {sliceCount - 1, 0, 0, 0};

    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread.slice = UINT32_MAX;
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(UINT32_MAX, deviceIndex);
}

using DebugSessionMultiTile = Test<MultipleDevicesWithCustomHwInfo>;

TEST_F(DebugSessionMultiTile, givenApiThreadAndMultipleTilesWhenConvertingToPhysicalThenCorrectValueReturned) {
    L0::Device *device = driverHandle->devices[0];

    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, device);

    ze_device_thread_t thread = {sliceCount * 2 - 1, 0, 0, 0};

    uint32_t deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);

    auto convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(1u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread = {sliceCount - 1, 0, 0, 0};

    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);

    EXPECT_EQ(0u, deviceIndex);
    EXPECT_EQ(sliceCount - 1, convertedThread.slice);
    EXPECT_EQ(thread.subslice, convertedThread.subslice);
    EXPECT_EQ(thread.eu, convertedThread.eu);
    EXPECT_EQ(thread.thread, convertedThread.thread);

    thread.slice = UINT32_MAX;
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(UINT32_MAX, deviceIndex);

    deviceIndex = 0;
    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);
    EXPECT_EQ(convertedThread.slice, thread.slice);

    deviceIndex = 1;
    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);
    EXPECT_EQ(convertedThread.slice, thread.slice);

    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, deviceImp->subDevices[1]);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    convertedThread = debugSession->convertToPhysicalWithinDevice(thread, deviceIndex);
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

TEST_F(DebugSessionMultiTile, givenApiThreadAndMultiTileWhenFillingDevicesThenVectorEntriesAreSet) {
    L0::Device *device = driverHandle->devices[0];

    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, device);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    std::vector<uint8_t> devices(numSubDevices);
    debugSession->fillDevicesFromThread(thread, devices);

    EXPECT_EQ(1u, devices[0]);
    EXPECT_EQ(1u, devices[1]);
}

TEST_F(DebugSessionMultiTile, givenApiThreadAndSingleTileWhenFillingDevicesThenVectorEntryIsSet) {
    L0::Device *device = driverHandle->devices[0];

    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, device);

    ze_device_thread_t thread = {sliceCount * numSubDevices - 1, 0, 0, 0};

    std::vector<uint8_t> devices(numSubDevices);
    debugSession->fillDevicesFromThread(thread, devices);

    EXPECT_EQ(0u, devices[0]);
    EXPECT_EQ(1u, devices[1]);
}

TEST_F(DebugSessionMultiTile, givenApiThreadAndMultipleTilesWhenGettingDeviceIndexThenCorrectValueReturned) {
    L0::Device *device = driverHandle->devices[0];
    auto debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, device);

    ze_device_thread_t thread = {sliceCount * 2 - 1, 0, 0, 0};

    uint32_t deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(1u, deviceIndex);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    thread.slice = UINT32_MAX;
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(UINT32_MAX, deviceIndex);

    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, deviceImp->subDevices[0]);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(0u, deviceIndex);

    debugSession = std::make_unique<DebugSessionMock>(zet_debug_config_t{0x1234}, deviceImp->subDevices[1]);

    thread = {sliceCount - 1, 0, 0, 0};
    deviceIndex = debugSession->getDeviceIndexFromApiThread(thread);
    EXPECT_EQ(1u, deviceIndex);
}

} // namespace ult
} // namespace L0
