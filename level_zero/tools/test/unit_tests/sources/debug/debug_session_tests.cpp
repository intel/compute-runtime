/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include "common/StateSaveAreaHeader.h"
#include "encode_surface_state_args.h"

namespace L0 {
namespace ult {

struct MockDebugSession : public L0::DebugSessionImp {

    using L0::DebugSession::allThreads;
    using L0::DebugSession::debugArea;

    using L0::DebugSessionImp::calculateThreadSlotOffset;
    using L0::DebugSessionImp::checkTriggerEventsForAttention;
    using L0::DebugSessionImp::fillResumeAndStoppedThreadsFromNewlyStopped;
    using L0::DebugSessionImp::generateEventsAndResumeStoppedThreads;
    using L0::DebugSessionImp::generateEventsForPendingInterrupts;
    using L0::DebugSessionImp::generateEventsForStoppedThreads;
    using L0::DebugSessionImp::getRegisterSize;
    using L0::DebugSessionImp::getStateSaveAreaHeader;
    using L0::DebugSessionImp::markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention;
    using L0::DebugSessionImp::newAttentionRaised;
    using L0::DebugSessionImp::readSbaRegisters;
    using L0::DebugSessionImp::registersAccessHelper;
    using L0::DebugSessionImp::resumeAccidentallyStoppedThreads;
    using L0::DebugSessionImp::sendInterrupts;
    using L0::DebugSessionImp::typeToRegsetDesc;

    using L0::DebugSessionImp::interruptSent;
    using L0::DebugSessionImp::stateSaveAreaHeader;
    using L0::DebugSessionImp::triggerEvents;

    using L0::DebugSessionImp::expectedAttentionEvents;
    using L0::DebugSessionImp::interruptMutex;
    using L0::DebugSessionImp::interruptRequests;
    using L0::DebugSessionImp::isValidGpuAddress;
    using L0::DebugSessionImp::newlyStoppedThreads;
    using L0::DebugSessionImp::pendingInterrupts;
    using L0::DebugSessionImp::readSLMMemory;
    using L0::DebugSessionImp::readStateSaveAreaHeader;
    using L0::DebugSessionImp::tileAttachEnabled;
    using L0::DebugSessionImp::tileSessions;
    using L0::DebugSessionImp::writeSLMMemory;

    MockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device, true) {}

    MockDebugSession(const zet_debug_config_t &config, L0::Device *device, bool rootAttach) : DebugSessionImp(config, device) {
        if (device) {
            topologyMap = DebugSessionMock::buildMockTopology(device);
        }
        setAttachMode(rootAttach);
        if (rootAttach) {
            createEuThreads();
        }
    }

    ~MockDebugSession() override {
        for (auto session : tileSessions) {
            delete session.first;
        }
    }

    bool closeConnection() override { return true; }
    ze_result_t initialize() override {

        auto numTiles = connectedDevice->getNEODevice()->getNumSubDevices();
        if (numTiles > 0 && tileAttachEnabled) {
            tileSessions.resize(numTiles);
            zet_debug_config_t config = {};

            for (uint32_t i = 0; i < numTiles; i++) {
                auto subDevice = connectedDevice->getNEODevice()->getSubDevice(i)->getSpecializedDevice<Device>();
                tileSessions[i] = std::pair<DebugSessionImp *, bool>{new MockDebugSession(config, subDevice), false};
            }
        }

        return ZE_RESULT_SUCCESS;
    }

    void attachTile() override { attachTileCalled = true; };
    void detachTile() override { detachTileCalled = true; };
    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override { cleanRootSessionDeviceIndices.push_back(deviceIndex); };

    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override {
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
        return readMemoryResult;
    }

    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override {
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        readRegistersCallCount++;
        readRegistersReg = type;

        if (readRegistersSizeToFill > 0 && readRegistersSizeToFill <= sizeof(regs)) {
            memcpy_s(pRegisterValues, readRegistersSizeToFill, regs, readRegistersSizeToFill);
        }
        if (readRegistersResult != ZE_RESULT_FORCE_UINT32) {
            return readRegistersResult;
        }
        return DebugSessionImp::readRegistersImp(thread, type, start, count, pRegisterValues);
    }

    ze_result_t writeRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        writeRegistersCallCount++;
        writeRegistersReg = type;

        if (writeRegistersResult != ZE_RESULT_FORCE_UINT32) {
            return writeRegistersResult;
        }

        return DebugSessionImp::writeRegistersImp(thread, type, start, count, pRegisterValues);
    }

    ze_result_t readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) override {
        sbaBuffer = sba;
        return readSbaBufferResult;
    }

    void readStateSaveAreaHeader() override {
        readStateSaveAreaHeaderCalled++;
        DebugSessionImp::readStateSaveAreaHeader();
    }
    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override {
        resumeImpCalled++;
        resumeThreadCount = threads.size();
        resumedThreads.push_back(std::move(threads));
        resumedDevices.push_back(deviceIndex);
        return resumeImpResult;
    }

    ze_result_t interruptImp(uint32_t deviceIndex) override {
        interruptImpCalled++;
        interruptedDevices.push_back(deviceIndex);
        return interruptImpResult;
    }

    ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override {
        if (gpuVa != 0 && gpuVa >= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()) &&
            gpuVa <= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data() + stateSaveAreaHeader.size())) {
            [[maybe_unused]] auto offset = ptrDiff(gpuVa, reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()));
            memcpy_s(output, size, reinterpret_cast<void *>(gpuVa), size);
        }

        else if (gpuVa != 0 && gpuVa >= reinterpret_cast<uint64_t>(readMemoryBuffer) &&
                 gpuVa <= reinterpret_cast<uint64_t>(ptrOffset(readMemoryBuffer, sizeof(readMemoryBuffer)))) {
            memcpy_s(output, size, reinterpret_cast<void *>(gpuVa), std::min(size, sizeof(readMemoryBuffer)));
        }
        return readMemoryResult;
    }
    ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override {

        if (gpuVa != 0 && gpuVa >= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()) &&
            gpuVa <= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data() + stateSaveAreaHeader.size())) {
            [[maybe_unused]] auto offset = ptrDiff(gpuVa, reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()));
            memcpy_s(reinterpret_cast<void *>(gpuVa), size, input, size);
        }

        return writeMemoryResult;
    }

    void resumeAccidentallyStoppedThreads(const std::vector<EuThread::ThreadId> &threadIds) override {
        resumeAccidentallyStoppedCalled++;
        return DebugSessionImp::resumeAccidentallyStoppedThreads(threadIds);
    }

    void startAsyncThread() override {}
    bool readModuleDebugArea() override { return true; }

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override {
        events.push_back(debugEvent);
    }

    bool isForceExceptionOrForceExternalHaltOnlyExceptionReason(uint32_t *cr0) override {
        return onlyForceException;
    }

    bool isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(uint32_t *cr0) {
        return DebugSessionImp::isForceExceptionOrForceExternalHaltOnlyExceptionReason(cr0);
    }

    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
        if (skipReadSystemRoutineIdent) {
            srMagic.count = threadStopped ? 1 : 0;
            return readSystemRoutineIdentRetVal;
        }
        return DebugSessionImp::readSystemRoutineIdent(thread, vmHandle, srMagic);
    }

    bool areRequestedThreadsStopped(ze_device_thread_t thread) override {
        if (areRequestedThreadsStoppedReturnValue != -1) {
            return areRequestedThreadsStoppedReturnValue != 0;
        }
        return DebugSessionImp::areRequestedThreadsStopped(thread);
    }

    bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) override {
        writeResumeCommandCalled++;
        if (skipWriteResumeCommand) {
            return true;
        }
        return L0::DebugSessionImp::writeResumeCommand(threadIds);
    }

    ze_result_t cmdRegisterAccessHelper(const EuThread::ThreadId &threadId, SIP::sip_command &command, bool write) override {

        if (slmTesting) {
            if (write &&
                (command.command == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_READ) ||
                 command.command == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_WRITE))) {
                cmdResgisterWrittenForSLM = command.command;
                if (forceCmdAccessFail) {
                    return ZE_RESULT_FORCE_UINT32;
                }
            }

            if (cmdResgisterWrittenForSLM != static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME)) {
                cmdRegisterAccessCount++;
            }
            if (!forceSlmCmdNotReady) {
                if (cmdRegisterAccessCount == 3) { // SIP restores cmd to READY

                    uint32_t size = command.size * slmSendBytesSize;

                    if (cmdResgisterWrittenForSLM == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_READ)) {
                        memcpy_s(command.buffer, size, slMemory + command.offset, size);
                    } else if (cmdResgisterWrittenForSLM == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_WRITE)) {
                        memcpy_s(slMemory + command.offset, size, command.buffer, size);
                        cmdRegisterAccessCount = 0;
                        cmdResgisterWrittenForSLM = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
                    }

                    command.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);
                    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(this->stateSaveAreaHeader.data()))->regHeader.cmd;
                    this->registersAccessHelper(this->allThreads[threadId].get(), regdesc, 0, 1, &command, true);

                } else if (cmdRegisterAccessCount == 4) { // Reading the data does an extra call to retrieve the data
                    cmdRegisterAccessCount = 0;
                    cmdResgisterWrittenForSLM = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
                    if (!memcmp(slMemory, "FailReadingData", strlen("FailReadingData"))) {
                        return ZE_RESULT_FORCE_UINT32;
                    }
                }
            }
        }

        return L0::DebugSessionImp::cmdRegisterAccessHelper(threadId, command, write);
    }

    bool checkThreadIsResumed(const EuThread::ThreadId &threadID) override {
        checkThreadIsResumedCalled++;
        if (skipCheckThreadIsResumed) {
            return true;
        }
        return L0::DebugSessionImp::checkThreadIsResumed(threadID);
    }

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        if (returnStateSaveAreaGpuVa) {
            return reinterpret_cast<uint64_t>(this->stateSaveAreaHeader.data());
        }
        return 0;
    };

    int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) override {
        if (returnTimeDiff != -1) {
            return returnTimeDiff;
        }
        return L0::DebugSessionImp::getTimeDifferenceMilliseconds(time);
    }

    const NEO::TopologyMap &getTopologyMap() override {
        return this->topologyMap;
    }

    NEO::TopologyMap topologyMap;

    uint32_t interruptImpCalled = 0;
    uint32_t resumeImpCalled = 0;
    uint32_t resumeAccidentallyStoppedCalled = 0;
    size_t resumeThreadCount = 0;
    ze_result_t interruptImpResult = ZE_RESULT_SUCCESS;
    ze_result_t resumeImpResult = ZE_RESULT_SUCCESS;
    bool onlyForceException = true;
    bool threadStopped = true;
    int areRequestedThreadsStoppedReturnValue = -1;
    bool readSystemRoutineIdentRetVal = true;
    size_t readRegistersSizeToFill = 0;
    ze_result_t readSbaBufferResult = ZE_RESULT_SUCCESS;
    ze_result_t readRegistersResult = ZE_RESULT_FORCE_UINT32;
    ze_result_t readMemoryResult = ZE_RESULT_SUCCESS;
    ze_result_t writeMemoryResult = ZE_RESULT_SUCCESS;
    ze_result_t writeRegistersResult = ZE_RESULT_FORCE_UINT32;

    uint32_t readStateSaveAreaHeaderCalled = 0;
    uint32_t readRegistersCallCount = 0;
    uint32_t readRegistersReg = 0;
    uint32_t writeRegistersCallCount = 0;
    uint32_t writeRegistersReg = 0;

    bool skipWriteResumeCommand = true;
    uint32_t writeResumeCommandCalled = 0;

    bool skipCheckThreadIsResumed = true;
    uint32_t checkThreadIsResumedCalled = 0;

    bool skipReadSystemRoutineIdent = true;
    std::vector<zet_debug_event_t> events;
    std::vector<uint32_t> interruptedDevices;
    std::vector<uint32_t> resumedDevices;
    std::vector<std::vector<EuThread::ThreadId>> resumedThreads;

    NEO::SbaTrackedAddresses sba;
    uint64_t readMemoryBuffer[64];
    uint64_t regs[16];

    int returnTimeDiff = -1;
    bool returnStateSaveAreaGpuVa = true;

    bool attachTileCalled = false;
    bool detachTileCalled = false;
    std::vector<uint32_t> cleanRootSessionDeviceIndices;

    int cmdRegisterAccessCount = 0;
    uint32_t cmdResgisterWrittenForSLM = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
    bool forceSlmCmdNotReady = false;
    bool forceCmdAccessFail = false;
    bool slmTesting = false;
    static constexpr int slmSize = 256;
    char slMemory[slmSize];
};

using DebugSessionTest = ::testing::Test;

TEST(DeviceWithDebugSessionTest, GivenDeviceWithDebugSessionWhenCallingReleaseResourcesThenCloseConnectionIsCalled) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = std::unique_ptr<L0::Device>(Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue));
    ASSERT_NE(nullptr, device);

    zet_debug_config_t config = {};
    auto session = new DebugSessionMock(config, device.get());
    static_cast<DeviceImp *>(device.get())->setDebugSession(session);
    static_cast<DeviceImp *>(device.get())->releaseResources();

    EXPECT_TRUE(session->closeConnectionCalled);
}

TEST(DebugSessionTest, givenNullDeviceWhenDebugSessionCreatedThenAllThreadsAreEmpty) {
    auto sessionMock = std::make_unique<MockDebugSession>(zet_debug_config_t{0x1234}, nullptr);
    EXPECT_TRUE(sessionMock->allThreads.empty());
}

TEST(DebugSessionTest, givenAllStoppedThreadsWhenInterruptCalledThenErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
    }

    ze_device_thread_t apiThread = {0, 0, 0, UINT32_MAX};
    auto result = sessionMock->interrupt(apiThread);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST(DebugSessionTest, givenNoPendingInterruptWhenSendInterruptCalledThenInterruptImpNotCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->interruptSent = true;

    sessionMock->sendInterrupts();
    EXPECT_EQ(0u, sessionMock->interruptImpCalled);
}

TEST(DebugSessionTest, givenPendingInterruptWhenNewInterruptForThreadCalledThenErrorNotReadyReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};

    sessionMock->pendingInterrupts.push_back({apiThread, false});
    auto result = sessionMock->interrupt(apiThread);

    EXPECT_EQ(ZE_RESULT_NOT_READY, result);
}

TEST(DebugSessionTest, givenInterruptAlreadySentWhenSendInterruptCalledSecondTimeThenEarlyReturns) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->threadStopped = 0;

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);
    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);

    ze_device_thread_t apiThread2 = {0, 0, 0, 2};
    result = sessionMock->interrupt(apiThread2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);
}

TEST(DebugSessionTest, givenInterruptRequestWhenInterruptImpFailsInSendInterruptThenApiEventsArePushed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;
    ze_device_thread_t apiThread = {0, 0, 0, 1};
    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(1u, sessionMock->interruptImpCalled);

    EXPECT_EQ(1u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
}

TEST(DebugSessionTest, givenPendingInteruptWhenHadnlingThreadWithAttentionThenPendingInterruptIsMarkedComplete) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_TRUE(sessionMock->allThreads[thread]->isStopped());

    EXPECT_TRUE(sessionMock->pendingInterrupts[0].second);
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenStoppedThreadWhenAddingNewlyStoppedThenThreadIsNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->allThreads[thread]->stopThread(1u);

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenNoStoppedThreadWhenAddingNewlyStoppedThenThreadIsNotAdded) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->threadStopped = 0;

    EuThread::ThreadId thread(0, 0, 0, 0, 0);
    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenNoInterruptsSentWhenGenerateEventsAndResumeCalledThenTriggerEventsNotChanged) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    sessionMock->interruptSent = false;
    sessionMock->triggerEvents = false;

    sessionMock->generateEventsAndResumeStoppedThreads();
    EXPECT_FALSE(sessionMock->triggerEvents);
}

TEST(DebugSessionTest, givenTriggerEventsWhenGenerateEventsAndResumeCalledThenEventsGeneratedAndThreadsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    ze_device_thread_t apiThread2 = {0, 0, 1, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, true});

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(EuThread::ThreadId(0, apiThread2), 1u);

    sessionMock->triggerEvents = true;
    sessionMock->interruptSent = true;

    sessionMock->onlyForceException = false;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(2u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->events[1].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->events[1].info.thread.thread));

    EXPECT_EQ(1u, sessionMock->resumeAccidentallyStoppedCalled);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPendingInterruptAfterTimeoutWhenGenerateEventsAndResumeStoppedThreadsIsCalledThenEventsUnavailableAreSentAndFlagsSetFalse) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->triggerEvents = false;
    sessionMock->interruptSent = true;
    sessionMock->returnTimeDiff = 5 * DebugSessionImp::interruptTimeout;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(1u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
    EXPECT_FALSE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, givenPendingInterruptBeforeTimeoutWhenGenerateEventsAndResumeStoppedThreadsIsCalledThenEventsNotTriggered) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->triggerEvents = false;
    sessionMock->interruptSent = true;
    sessionMock->returnTimeDiff = DebugSessionImp::interruptTimeout / 2;

    sessionMock->generateEventsAndResumeStoppedThreads();

    EXPECT_EQ(0u, sessionMock->events.size());
    EXPECT_EQ(1u, sessionMock->pendingInterrupts.size());
    EXPECT_FALSE(sessionMock->triggerEvents);
    EXPECT_TRUE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, givenErrorFromReadSystemRoutineIdentWhenCheckingThreadStateThenThreadIsNotStopped) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readSystemRoutineIdentRetVal = false;
    EuThread::ThreadId thread(0, 0, 0, 0, 0);

    sessionMock->markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(thread, 1u);

    EXPECT_FALSE(sessionMock->allThreads[thread]->isStopped());
    EXPECT_EQ(0u, sessionMock->newlyStoppedThreads.size());
}

TEST(DebugSessionTest, givenPendingInterruptsWhenGeneratingEventsThenStoppedEventIsGeneratedForCompletedInterrupt) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    ze_device_thread_t apiThread2 = {0, 0, 1, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});
    sessionMock->pendingInterrupts.push_back({apiThread2, true});

    sessionMock->generateEventsForPendingInterrupts();

    EXPECT_EQ(2u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED, sessionMock->events[1].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->events[1].info.thread.thread));

    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());
}

TEST(DebugSessionTest, givenEmptyStateSaveAreaWhenGetStateSaveAreaCalledThenReadStateSaveAreaCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    auto stateSaveArea = sessionMock->getStateSaveAreaHeader();
    EXPECT_EQ(nullptr, stateSaveArea);
    EXPECT_EQ(1u, sessionMock->readStateSaveAreaHeaderCalled);
}

TEST(DebugSessionTest, givenStoppedThreadsWhenFillingResumeAndStoppedThreadsFromNewlyStoppedThenContainerFilledBasedOnExceptionReason) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    EuThread::ThreadId thread2 = {0, 0, 0, 1, 1};
    EuThread::ThreadId thread3 = {0, 0, 0, 1, 2};

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->newlyStoppedThreads.push_back(thread2);
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    sessionMock->onlyForceException = true;

    {
        std::vector<EuThread::ThreadId> resumeThreads;
        std::vector<EuThread::ThreadId> stoppedThreads;

        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread2]->stopThread(1u);

        sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads);
        EXPECT_EQ(2u, resumeThreads.size());
        EXPECT_EQ(0u, stoppedThreads.size());
    }

    sessionMock->newlyStoppedThreads.push_back(thread);
    sessionMock->newlyStoppedThreads.push_back(thread2);
    sessionMock->newlyStoppedThreads.push_back(thread3);

    sessionMock->onlyForceException = false;
    {
        std::vector<EuThread::ThreadId> resumeThreads;
        std::vector<EuThread::ThreadId> stoppedThreads;

        sessionMock->allThreads[thread]->stopThread(1u);
        sessionMock->allThreads[thread2]->stopThread(1u);

        sessionMock->fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreads);
        EXPECT_EQ(0u, resumeThreads.size());
        EXPECT_EQ(2u, stoppedThreads.size());
    }
}

TEST(DebugSessionTest, givenThreadsToResumeWhenResumeAccidentallyStoppedThreadsCalledThenThreadsResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = counter++;
            return true;
        }

        int counter = 0;
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    EuThread::ThreadId thread2 = {0, 0, 0, 1, 1};

    std::vector<EuThread::ThreadId> threadIds;
    sessionMock->allThreads[thread]->verifyStopped(1u);
    sessionMock->allThreads[thread2]->verifyStopped(1u);

    threadIds.push_back(thread);
    threadIds.push_back(thread2);

    sessionMock->resumeAccidentallyStoppedThreads(threadIds);

    EXPECT_EQ(2u, sessionMock->resumeThreadCount);
    EXPECT_EQ(1u, sessionMock->resumeImpCalled);
    EXPECT_EQ(3u, sessionMock->checkThreadIsResumedCalled);

    EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[thread2]->isRunning());
}

TEST(DebugSessionTest, givenCr0RegisterWhenIsFEOrFEHOnlyExceptionReasonThenTrueReturnedForFEorFEHBitsOnly) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    uint32_t cr0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    cr0[1] = 1 << 26;
    EXPECT_TRUE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = 1 << 30;
    EXPECT_TRUE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 26) | (1 << 30);
    EXPECT_TRUE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));

    cr0[1] = (1 << 27) | (1 << 30);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 27) | (1 << 26);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
    cr0[1] = (1 << 28);
    EXPECT_FALSE(sessionMock->isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0));
}

TEST(DebugSessionTest, givenSomeThreadsRunningWhenResumeCalledThenOnlyStoppedThreadsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t thread = {0, 0, 0, UINT32_MAX};

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        sessionMock->allThreads[thread]->stopThread(1u);
    }

    EuThread::ThreadId euthread(0, 0, 0, 0, 3);
    sessionMock->allThreads[euthread]->resumeThread();

    auto result = sessionMock->resume(thread);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount - 1u, sessionMock->resumeThreadCount);

    for (uint32_t i = 0; i < hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount; i++) {
        EuThread::ThreadId thread(0, 0, 0, 0, i);
        EXPECT_TRUE(sessionMock->allThreads[thread]->isRunning());
    }
}

TEST(DebugSessionTest, givenAllThreadsRunningWhenResumeCalledThenErrorUnavailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    ze_device_thread_t thread = {0, 0, 0, 1};
    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST(DebugSessionTest, givenErrorFromResumeImpWhenResumeCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 1};
    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenErrorFromReadSbaBufferWhenReadSbaRegistersCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readSbaBufferResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];
    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenErrorFromReadRegistersWhenReadSbaRegistersCalledThenErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];
    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

HWTEST2_F(DebugSessionTest, givenErrorFromReadMemoryWhenReadSbaRegistersCalledThenErrorReturned, IsAtLeastXeHpCore) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->readRegistersSizeToFill = 6 * sizeof(uint32_t);

    uint32_t *scratchIndex = reinterpret_cast<uint32_t *>(ptrOffset(sessionMock->regs, 5 * sizeof(uint32_t)));
    *scratchIndex = 1u << 10u;

    ze_device_thread_t thread = {0, 0, 0, 0};
    uint64_t sba[9];

    auto result = sessionMock->readSbaRegisters(EuThread::ThreadId(0, thread), 0, 9, sba);

    auto &hwHelper = HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.isScratchSpaceSurfaceStateAccessible());
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST(DebugSessionTest, givenUnknownRegsetTypeWhenTypeToRegsetFlagsCalledThenZeroIsReturned) {
    EXPECT_EQ(0u, DebugSessionImp::typeToRegsetFlags(0));
    EXPECT_EQ(0u, DebugSessionImp::typeToRegsetFlags(UINT32_MAX));
}

TEST(DebugSessionTest, givenNullptrStateSaveAreaGpuVaWhenCallingCheckIsThreadResumedThenReportThreadHasBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
            return 0;
        };
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, whenCallingCheckThreadIsResumedWithoutSrMagicThenThreadIsAssumedRunning) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->skipReadSystemRoutineIdent = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        sessionMock->stateSaveAreaHeader.resize(size);
    }

    auto stateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(sessionMock->stateSaveAreaHeader.data());
    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);

    auto threadSlotOffset = sessionMock->calculateThreadSlotOffset(threadId);
    auto srMagicOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.sr_magic_offset;
    SIP::sr_ident srMagic = {{0}};
    sessionMock->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(stateSaveAreaHeader) + srMagicOffset);

    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenErrorFromReadSystemRoutineIdentWhenCallingCheckThreadIsResumedThenThreadIsAssumedRunning) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->readSystemRoutineIdentRetVal = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenSrMagicWithCounterLessThanlLastThreadCounterThenThreadHasBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 0;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);
    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, givenSrMagicWithCounterEqualToPrevousThenThreadHasNotBeenResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 1;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);

    EXPECT_EQ(1u, sessionMock->allThreads[threadId]->getLastCounter());

    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_FALSE(resumed);
}

TEST(DebugSessionTest, givenSrMagicWithCounterBiggerThanPreviousThenThreadIsResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 3;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1u);
    sessionMock->allThreads[threadId]->stopThread(1u);

    EXPECT_EQ(1u, sessionMock->allThreads[threadId]->getLastCounter());

    bool resumed = sessionMock->checkThreadIsResumed(threadId);

    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, givenSrMagicWithCounterOverflowingZeroThenThreadIsResumed) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = 0;
            return true;
        }
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->skipCheckThreadIsResumed = false;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(255u);
    sessionMock->allThreads[threadId]->stopThread(1u);

    EXPECT_EQ(255u, sessionMock->allThreads[threadId]->getLastCounter());

    bool resumed = sessionMock->checkThreadIsResumed(threadId);
    EXPECT_TRUE(resumed);
}

TEST(DebugSessionTest, GivenBindlessSipVersion1AndResumeWARequiredWhenCallingResumeThenBitInRegisterIsWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->writeRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    if (l0HwHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->readRegistersReg);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->writeRegistersReg);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenErrorFromReadRegisterWhenResumingThreadThenRegisterIsNotWrittenWithResumeBitAndErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->writeRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);

    if (l0HwHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->readRegistersReg);
        EXPECT_EQ(0u, sessionMock->writeRegistersReg);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenErrorFromWriteRegisterWhenResumingThreadThenRegisterIsNotWrittenWithResumeBitAndErrorReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->writeRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);

    if (l0HwHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), sessionMock->readRegistersReg);
        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenNonBindlessSipVersion1AndResumeWARequiredWhenCallingResumeThenBitInRegisterIsWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->writeRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->readMemoryResult = ZE_RESULT_SUCCESS;
    sessionMock->debugArea.reserved1 = 0u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    if (l0HwHelper.isResumeWARequired()) {
        EXPECT_EQ(1u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
        EXPECT_EQ(uint32_t(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), sessionMock->writeRegistersReg);
    } else {
        EXPECT_EQ(0u, sessionMock->readRegistersCallCount);
        EXPECT_EQ(0u, sessionMock->writeRegistersCallCount);
        EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
        EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
    }

    EXPECT_FALSE(sessionMock->allThreads[threadId]->isStopped());
}

TEST(DebugSessionTest, GivenBindlessSipVersion2WhenWritingResumeFailsThenErrorIsReturned) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            srMagic.count = ++counter;
            return true;
        }

      private:
        int counter = 0;
    };
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->readRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->writeRegistersResult = ZE_RESULT_ERROR_UNKNOWN;
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    sessionMock->skipWriteResumeCommand = false;
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);

    EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
    EXPECT_EQ(2u, sessionMock->checkThreadIsResumedCalled);
}

TEST(DebugSessionTest, GivenBindlessSipVersion2WhenResumingThreadThenCheckIfThreadIsResumedIsCalledUntilSipCounterIsIncremented) {
    class InternalMockDebugSession : public MockDebugSession {
      public:
        InternalMockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device) {}
        bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) override {
            if (counter > 5) {
                srMagic.count = 4;
            } else {
                srMagic.count = 1;
            }

            counter++;
            return true;
        }
        int counter = 0;
    };

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<InternalMockDebugSession>(config, &deviceImp);
    ASSERT_NE(nullptr, sessionMock);
    sessionMock->debugArea.reserved1 = 1u;
    sessionMock->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    sessionMock->skipCheckThreadIsResumed = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    sessionMock->allThreads[threadId]->verifyStopped(1);
    sessionMock->allThreads[threadId]->stopThread(1u);

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, sessionMock->writeResumeCommandCalled);
    EXPECT_EQ(7u, sessionMock->checkThreadIsResumedCalled);

    sessionMock->allThreads[threadId]->verifyStopped(3);
    sessionMock->allThreads[threadId]->stopThread(1u);

    sessionMock->checkThreadIsResumedCalled = 0;
    result = sessionMock->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, sessionMock->checkThreadIsResumedCalled);
}

using MultiTileDebugSessionTest = Test<MultipleDevicesWithCustomHwInfo>;

TEST_F(MultiTileDebugSessionTest, givenThreadsFromMultipleTilesWhenResumeCalledThenThreadsResumedInAllTiles) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile0]->stopThread(1u);
    sessionMock->allThreads[threadTile1]->stopThread(1u);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(2u, sessionMock->resumeImpCalled);

    sessionMock->allThreads[threadTile1]->stopThread(1u);

    result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(3u, sessionMock->resumeImpCalled);
}

TEST_F(MultiTileDebugSessionTest, givenThreadFromSingleTileWhenResumeCalledThenThreadIsResumed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile0]->stopThread(1u);

    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile1]->stopThread(1u);

    ze_device_thread_t thread = {sliceCount, 0, 0, 0};

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(1u, sessionMock->resumeImpCalled);
    EXPECT_EQ(1u, sessionMock->resumedDevices[0]);

    ze_device_thread_t resumedThreadExpected = {0, 0, 0, 0};
    EXPECT_EQ(resumedThreadExpected.slice, sessionMock->resumedThreads[0][0].slice);
    EXPECT_EQ(resumedThreadExpected.subslice, sessionMock->resumedThreads[0][0].subslice);
    EXPECT_EQ(resumedThreadExpected.eu, sessionMock->resumedThreads[0][0].eu);
    EXPECT_EQ(resumedThreadExpected.thread, sessionMock->resumedThreads[0][0].thread);

    thread = {0, 0, 0, 0};
    sessionMock->resumeImpCalled = 0;
    sessionMock->resumedDevices.clear();

    result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());

    EXPECT_EQ(1u, sessionMock->resumeImpCalled);
    EXPECT_EQ(0u, sessionMock->resumedDevices[0]);

    resumedThreadExpected = {0, 0, 0, 0};
    EXPECT_EQ(resumedThreadExpected.slice, sessionMock->resumedThreads[0][0].slice);
    EXPECT_EQ(resumedThreadExpected.subslice, sessionMock->resumedThreads[0][0].subslice);
    EXPECT_EQ(resumedThreadExpected.eu, sessionMock->resumedThreads[0][0].eu);
    EXPECT_EQ(resumedThreadExpected.thread, sessionMock->resumedThreads[0][0].thread);
}

TEST_F(MultiTileDebugSessionTest, givenRunningThreadsWhenResumeCalledThenThreadsNotResumedAndErrorNotAvailableReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};
    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    EXPECT_EQ(0u, sessionMock->resumeImpCalled);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());
}

TEST_F(MultiTileDebugSessionTest, givenErrorFromResumeWithinDeviceWhenResumeCalledThenThreadsAreResumedAndErrorUnkwnownReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    EuThread::ThreadId threadTile0(0, 0, 0, 0, 0);
    EuThread::ThreadId threadTile1(1, 0, 0, 0, 0);
    sessionMock->allThreads[threadTile0]->stopThread(1u);
    sessionMock->allThreads[threadTile1]->stopThread(1u);

    ze_device_thread_t thread = {UINT32_MAX, 0, 0, 0};

    sessionMock->resumeImpResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto result = sessionMock->resume(thread);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    EXPECT_EQ(2u, sessionMock->resumeImpCalled);

    EXPECT_TRUE(sessionMock->allThreads[threadTile0]->isRunning());
    EXPECT_TRUE(sessionMock->allThreads[threadTile1]->isRunning());
}

TEST_F(MultiTileDebugSessionTest, givenOneDeviceRequestWhenSendingInterruptsThenOnlyOneInterruptCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    // Second subdevice's slice
    ze_device_thread_t apiThread = {sliceCount, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_EQ(1u, sessionMock->interruptImpCalled);
    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->expectedAttentionEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTwoDevicesInRequestsWhenSendingInterruptsThenTwoInterruptsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    ze_device_thread_t apiThread2 = {sliceCount, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = sessionMock->interrupt(apiThread2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);
    EXPECT_EQ(2u, sessionMock->expectedAttentionEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenSendingInterruptsThenInterruptIsSentWithCorrectDeviceIndex) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    sessionMock->tileAttachEnabled = true;
    sessionMock->initialize();

    auto tileSession1 = static_cast<MockDebugSession *>(sessionMock->tileSessions[1].first);
    auto tileSession0 = static_cast<MockDebugSession *>(sessionMock->tileSessions[0].first);

    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = tileSession1->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = tileSession0->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    tileSession1->sendInterrupts();
    tileSession0->sendInterrupts();

    EXPECT_TRUE(tileSession1->interruptSent);
    EXPECT_TRUE(tileSession0->interruptSent);
    EXPECT_EQ(1u, tileSession1->interruptedDevices[0]);
    EXPECT_EQ(0u, tileSession0->interruptedDevices[0]);
}

TEST_F(MultiTileDebugSessionTest, givenAllSlicesInRequestWhenSendingInterruptsThenTwoInterruptsCalled) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;

    ze_device_thread_t apiThread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);
    EXPECT_EQ(2u, sessionMock->expectedAttentionEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTwoInterruptsSentWhenCheckingTriggerEventsThenTriggerEventsWhenAllAttEventsReceived) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    ze_device_thread_t apiThread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();
    EXPECT_EQ(2u, sessionMock->expectedAttentionEvents);

    sessionMock->newAttentionRaised(0);
    EXPECT_EQ(1u, sessionMock->expectedAttentionEvents);
    sessionMock->checkTriggerEventsForAttention();

    EXPECT_FALSE(sessionMock->triggerEvents);

    sessionMock->newAttentionRaised(1);
    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);
    sessionMock->checkTriggerEventsForAttention();

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST_F(MultiTileDebugSessionTest, givenTwoDevicesInRequestsWhenAllInterruptsReturnErrorThenAllInterruptRequestsGenerateUnavailableEvents) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->readRegistersResult = ZE_RESULT_SUCCESS;
    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t apiThread = {0, 0, 0, 0};
    ze_device_thread_t apiThread2 = {sliceCount, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = sessionMock->interrupt(apiThread2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(2u, sessionMock->interruptRequests.size());

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_FALSE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);

    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());

    EXPECT_EQ(2u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[1].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread2, sessionMock->events[1].info.thread.thread));
}

TEST_F(MultiTileDebugSessionTest, givenAllSlicesInRequestWhenAllInterruptsReturnErrorThenAllInterruptRequestsGenerateUnavailableEvents) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_ERROR_UNKNOWN;

    ze_device_thread_t apiThread = {UINT32_MAX, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, sessionMock->interruptRequests.size());

    sessionMock->sendInterrupts();

    EXPECT_EQ(2u, sessionMock->interruptImpCalled);
    EXPECT_FALSE(sessionMock->interruptSent);
    EXPECT_EQ(0u, sessionMock->interruptedDevices[0]);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[1]);

    EXPECT_EQ(0u, sessionMock->expectedAttentionEvents);
    EXPECT_EQ(0u, sessionMock->pendingInterrupts.size());

    EXPECT_EQ(1u, sessionMock->events.size());

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, sessionMock->events[0].type);
    EXPECT_TRUE(DebugSession::areThreadsEqual(apiThread, sessionMock->events[0].info.thread.thread));
}

TEST_F(MultiTileDebugSessionTest, GivenMultitileDeviceWhenCallingAreRequestedThreadsStoppedThenCorrectValueIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);
    ASSERT_NE(nullptr, sessionMock);

    ze_device_thread_t thread = {0, 0, 0, 0};
    ze_device_thread_t allSlices = {UINT32_MAX, 0, 0, 0};

    sessionMock->allThreads[EuThread::ThreadId(0, thread)]->stopThread(1u);
    sessionMock->allThreads[EuThread::ThreadId(1, thread)]->stopThread(1u);

    auto stopped = sessionMock->areRequestedThreadsStopped(thread);
    EXPECT_TRUE(stopped);

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_FALSE(stopped);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EuThread::ThreadId threadId(0, i, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
    }

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_FALSE(stopped);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EuThread::ThreadId threadId(1, i, 0, 0, 0);
        sessionMock->allThreads[threadId]->stopThread(1u);
    }

    stopped = sessionMock->areRequestedThreadsStopped(allSlices);
    EXPECT_TRUE(stopped);
}

struct DebugSessionRegistersAccess {
    void setUp() {
        zet_debug_config_t config = {};
        config.pid = 0x1234;
        auto hwInfo = *NEO::defaultHwInfo.get();

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);
        deviceImp = std::make_unique<Mock<L0::DeviceImp>>(neoDevice, neoDevice->getExecutionEnvironment());

        session = std::make_unique<MockDebugSession>(config, deviceImp.get());

        session->allThreads[stoppedThreadId]->stopThread(1u);
    }

    void tearDown() {
    }

    void dumpRegisterState() {
        if (session->stateSaveAreaHeader.size() == 0) {
            return;
        }
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());

        for (uint32_t thread = 0; thread < pStateSaveAreaHeader->regHeader.num_threads_per_eu; thread++) {
            EuThread::ThreadId threadId(0, 0, 0, 0, thread);
            auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);

            auto srMagicOffset = threadSlotOffset + pStateSaveAreaHeader->regHeader.sr_magic_offset;
            SIP::sr_ident srMagic;
            srMagic.count = 1;
            srMagic.version.major = pStateSaveAreaHeader->versionHeader.version.major;

            session->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(pStateSaveAreaHeader) + srMagicOffset);
        }
    }

    ze_device_thread_t stoppedThread = {0, 0, 0, 0};
    EuThread::ThreadId stoppedThreadId{0, stoppedThread};
    std::unique_ptr<MockDebugSession> session;
    std::unique_ptr<Mock<L0::DeviceImp>> deviceImp;
    NEO::MockDevice *neoDevice = nullptr;
};

using DebugSessionRegistersAccessTest = Test<DebugSessionRegistersAccess>;

TEST_F(DebugSessionRegistersAccessTest, givenTypeToRegsetDescCalledThenCorrectRegdescIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();

    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU), nullptr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), &pStateSaveAreaHeader->regHeader.grf);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.addr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU), &pStateSaveAreaHeader->regHeader.flag);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU), &pStateSaveAreaHeader->regHeader.emask);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.sr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.cr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU), &pStateSaveAreaHeader->regHeader.tdr);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU), &pStateSaveAreaHeader->regHeader.acc);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU), &pStateSaveAreaHeader->regHeader.mme);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU), &pStateSaveAreaHeader->regHeader.sp);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU), &pStateSaveAreaHeader->regHeader.dbg);
    EXPECT_EQ(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU), &pStateSaveAreaHeader->regHeader.fc);
    EXPECT_NE(session->typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU), nullptr);
}

TEST_F(DebugSessionRegistersAccessTest, givenValidRegisterWhenGettingSizeThenCorrectSizeIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    auto pStateSaveAreaHeader = session->getStateSaveAreaHeader();
    EXPECT_EQ(pStateSaveAreaHeader->regHeader.grf.bytes, session->getRegisterSize(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegisterWhenGettingSizeThenZeroSizeIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(0u, session->getRegisterSize(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU));
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, 0x12345, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenUnsupportedRegisterTypeWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, 0x12345, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegistersIndicesWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidRegistersIndicesWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 128, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 127, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenThreadAllWhenReadWriteRegistersCalledThenErrorNotAvailableIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    ze_device_thread_t threadAll = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugReadRegisters(session->toHandle(), threadAll, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugWriteRegisters(session->toHandle(), threadAll, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenNotReportedRegisterSetAndValidRegisterIndicesWhenReadRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    // MME is not present
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenNotReportedRegisterSetAndValidRegisterIndicesWhenWriteRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    // MME is not present
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenInvalidSbaRegistersIndicesWhenReadSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 9, 1, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 8, 2, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenWriteSbaRegistersCalledThenErrorInvalidArgumentIsReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, 0, 1, nullptr));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoneThreadsStoppedWhenReadRegistersCalledThenErrorNotAvailableReturned) {
    session->areRequestedThreadsStoppedReturnValue = 0;
    session->allThreads[stoppedThreadId]->resumeThread();
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoneThreadsStoppedWhenWriteRegistersCalledThenErrorNotAvailableReturned) {
    session->areRequestedThreadsStoppedReturnValue = 0;
    session->allThreads[stoppedThreadId]->resumeThread();
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenReadRegistersCalledThenErrorUnknownReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugReadRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenWriteRegistersCalledThenErrorUnknownReturned) {
    session->areRequestedThreadsStoppedReturnValue = 1;
    char grf[32] = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDebugWriteRegisters(session->toHandle(), stoppedThread, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, grf));
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaGpuVaWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->returnStateSaveAreaGpuVa = false;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenTssMagicCorruptedWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->stateSaveAreaHeader[0] = '!';
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenWriteGpuMemoryErrorWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->writeMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenReadGpuMemoryErrorWhenRegistersAccessHelperCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, givenNoStateSaveAreaWhenReadRegisterCalledThenErrorUnknownReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    session->readMemoryResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.grf;
    uint8_t r0[32];
    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    auto ret = session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, r0, true);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, ret);
}

TEST_F(DebugSessionRegistersAccessTest, GivenSipVersion2WhenWritingResumeCommandThenCmdRegIsWritten) {
    const uint32_t resumeValue = 0;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);
    EuThread::ThreadId thread3(0, 0, 0, 0, 3);

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread0);
    threads.push_back(thread3);

    dumpRegisterState();

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cmd;
    SIP::sip_command resumeCommand = {0};
    resumeCommand.command = 11;
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &resumeCommand, true);

    session->skipWriteResumeCommand = false;
    session->writeResumeCommand(threads);

    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &resumeCommand, false);
    EXPECT_EQ(resumeValue, resumeCommand.command);

    resumeCommand.command = 11;
    session->registersAccessHelper(session->allThreads[thread3].get(), regdesc, 0, 1, &resumeCommand, false);
    EXPECT_EQ(resumeValue, resumeCommand.command);
}

TEST_F(DebugSessionRegistersAccessTest, GivenSipVersion2WhenReadingSLMThenReadisSuccessfull) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
        session->slmTesting = true;
        int i;
        for (i = 0; i < session->slmSize; i++) {
            session->slMemory[i] = i & 127;
        }
    }

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread0);

    dumpRegisterState();

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cmd;
    SIP::sip_command sipCommand = {0};
    sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &sipCommand, true);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    session->skipWriteResumeCommand = false;

    char output[EXCHANGE_BUFFER_SIZE * 3];
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    int readSize = 7;
    auto retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    int i = 0;
    for (i = 0; i < readSize; i++) {
        EXPECT_EQ(output[i], session->slMemory[i]);
    }
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    int offset = 0x05;
    desc.address = 0x10000000 + offset;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    for (i = 0; i < readSize; i++) {
        EXPECT_EQ(output[i], session->slMemory[i + offset]);
    }
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    offset = 0x0f;
    desc.address = 0x10000000 + offset;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    for (i = 0; i < readSize; i++) {
        EXPECT_EQ(output[i], session->slMemory[i + 0xf]);
    }
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    readSize = 132;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    for (i = 0; i < readSize; i++) {
        EXPECT_EQ(output[i], session->slMemory[i + offset]);
    }
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    readSize = 230;
    offset = 0x0a;
    desc.address = 0x10000000 + offset;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);
    for (i = 0; i < readSize; i++) {
        EXPECT_EQ(output[i], session->slMemory[i + offset]);
    }
    memset(output, 0, EXCHANGE_BUFFER_SIZE * 3);

    session->slmTesting = false;
}

TEST_F(DebugSessionRegistersAccessTest, GivenCommandNotReadyOrFailureInCmdAccessOrFailFromAttClearWhenReadingSlmThenErrorIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
        session->slmTesting = true;
    }

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread0);

    dumpRegisterState();

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cmd;
    SIP::sip_command sipCommand = {0};
    sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &sipCommand, true);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    session->skipWriteResumeCommand = false;

    constexpr int readSize = EXCHANGE_BUFFER_SIZE * 2;
    char output[readSize];

    memcpy_s(session->slMemory, strlen("FailReadingData"), "FailReadingData", strlen("FailReadingData"));
    auto retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);

    session->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    session->resumeImpResult = ZE_RESULT_SUCCESS;

    session->forceCmdAccessFail = true;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    session->forceCmdAccessFail = false;

    session->forceSlmCmdNotReady = true;
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
    session->forceSlmCmdNotReady = false;

    session->stateSaveAreaHeader[0] = '!';
    retVal = session->readSLMMemory(thread0, &desc, readSize, output);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);

    session->slmTesting = false;
}

TEST_F(DebugSessionRegistersAccessTest, GivenSipVersion2WhenWritingSLMThenWritesSuccessfull) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
        session->slmTesting = true;
    }
    int i;
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread0);

    dumpRegisterState();

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cmd;
    SIP::sip_command sipCommand = {0};
    sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &sipCommand, true);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    session->skipWriteResumeCommand = false;

    char input1[EXCHANGE_BUFFER_SIZE * 3];
    memset(input1, 0xff, EXCHANGE_BUFFER_SIZE * 3);

    int inputSize = 7;

    auto retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i], input1[i]);
    }
    for (i = inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    inputSize = 23;

    retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i], input1[i]);
    }
    for (i = inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    inputSize = 7;
    int offset = 0x05;
    desc.address = 0x10000000 + offset;

    retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < offset; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i + offset], input1[i]);
    }
    for (i = offset + inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    inputSize = 27;
    offset = 0x0f;
    desc.address = 0x10000000 + offset;

    retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < offset; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i + offset], input1[i]);
    }
    for (i = offset + inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    inputSize = 25;
    offset = 0x07;
    desc.address = 0x10000000 + offset;

    retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < offset; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i + offset], input1[i]);
    }
    for (i = offset + inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    inputSize = EXCHANGE_BUFFER_SIZE + 16 + 8;
    offset = 0x03;
    desc.address = 0x10000000 + offset;

    retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < offset; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i + offset], input1[i]);
    }
    for (i = offset + inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    inputSize = 230;
    offset = 0x0a;
    desc.address = 0x10000000 + offset;
    retVal = session->writeSLMMemory(thread0, &desc, inputSize, input1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, retVal);

    for (i = 0; i < offset; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    for (i = 0; i < inputSize; i++) {
        EXPECT_EQ(session->slMemory[i + offset], input1[i]);
    }
    for (i = offset + inputSize + 1; i < session->slmSize; i++) {
        EXPECT_EQ(session->slMemory[i], i & 127);
    }
    // Restore SLM content
    for (i = 0; i < session->slmSize; i++) {
        session->slMemory[i] = i & 127;
    }

    session->slmTesting = false;
}

TEST_F(DebugSessionRegistersAccessTest, GivenCommandNotReadyOrFailureInCmdAccessOrFailFromAttClearWhenWritingSlmThenErrorIsReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
        session->slmTesting = true;
    }

    EuThread::ThreadId thread0(0, 0, 0, 0, 0);

    std::vector<EuThread::ThreadId> threads;
    threads.push_back(thread0);

    dumpRegisterState();

    auto *regdesc = &(reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data()))->regHeader.cmd;
    SIP::sip_command sipCommand = {0};
    sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &sipCommand, true);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x10000000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_SLM;

    session->skipWriteResumeCommand = false;

    constexpr int writeSize = 16;
    char input[writeSize];

    session->resumeImpResult = ZE_RESULT_ERROR_UNKNOWN;
    auto retVal = session->writeSLMMemory(thread0, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, retVal);
    session->resumeImpResult = ZE_RESULT_SUCCESS;

    session->forceCmdAccessFail = true;
    retVal = session->writeSLMMemory(thread0, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_FORCE_UINT32, retVal);
    session->forceCmdAccessFail = false;

    session->forceSlmCmdNotReady = true;
    retVal = session->writeSLMMemory(thread0, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
    session->registersAccessHelper(session->allThreads[thread0].get(), regdesc, 0, 1, &sipCommand, true);

    retVal = session->writeSLMMemory(thread0, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);

    desc.address = 0x1000000f; //force a read
    retVal = session->writeSLMMemory(thread0, &desc, writeSize, input);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, retVal);
    session->forceSlmCmdNotReady = false;

    session->slmTesting = false;
}
TEST_F(DebugSessionRegistersAccessTest, GivenBindlessSipVersion2WhenCallingResumeThenResumeInCmdRegisterIsWritten) {
    session->debugArea.reserved1 = 1u;
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }
    session->skipWriteResumeCommand = false;

    ze_device_thread_t thread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, thread);
    session->allThreads[threadId]->stopThread(1u);

    dumpRegisterState();

    auto result = session->resume(thread);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(0u, session->readRegistersCallCount);
    EXPECT_EQ(0u, session->writeRegistersCallCount);
    EXPECT_EQ(1u, session->writeResumeCommandCalled);
}

TEST_F(DebugSessionRegistersAccessTest, WhenReadingSbaRegistersThenCorrectAddressesAreReturned) {
    session->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);

    {
        auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
        auto size = pStateSaveAreaHeader->versionHeader.size * 8 +
                    pStateSaveAreaHeader->regHeader.state_area_offset +
                    pStateSaveAreaHeader->regHeader.state_save_size * 16;
        session->stateSaveAreaHeader.resize(size);
    }

    auto &hwHelper = HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    ze_device_thread_t thread = {0, 0, 0, 0};
    ze_device_thread_t thread1 = {0, 0, 0, 1};
    EuThread::ThreadId threadId(0, thread);
    EuThread::ThreadId threadId1(0, thread1);
    session->allThreads[threadId]->stopThread(1u);
    session->allThreads[threadId1]->stopThread(1u);

    dumpRegisterState();

    auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);
    auto stateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());
    auto grfOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.grf.offset;
    uint32_t *r0Thread0 = reinterpret_cast<uint32_t *>(ptrOffset(session->stateSaveAreaHeader.data(), grfOffset));
    r0Thread0[4] = 0x20 << 5;

    threadSlotOffset = session->calculateThreadSlotOffset(threadId1);
    grfOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.grf.offset;
    uint32_t *r0Thread1 = reinterpret_cast<uint32_t *>(ptrOffset(session->stateSaveAreaHeader.data(), grfOffset));
    r0Thread1[4] = 0x24 << 5;

    uint64_t sba[ZET_DEBUG_SBA_COUNT_INTEL_GPU];
    uint64_t sbaExpected[ZET_DEBUG_SBA_COUNT_INTEL_GPU];

    for (uint32_t i = 0; i < ZET_DEBUG_SBA_COUNT_INTEL_GPU; i++) {
        sbaExpected[i] = i * 0x1000;
    }

    sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU] = reinterpret_cast<uint64_t>(session->readMemoryBuffer);

    session->sba.InstructionBaseAddress = sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU];
    session->sba.IndirectObjectBaseAddress = sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU];
    session->sba.GeneralStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    session->sba.DynamicStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU];
    session->sba.BindlessSurfaceStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU];
    session->sba.BindlessSamplerStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU];
    session->sba.SurfaceStateBaseAddress = sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    auto scratchAllocationBase = 0ULL;
    if (hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        hwHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                         &session->readMemoryBuffer[1 * (hwHelper.getRenderSurfaceStateSize() / sizeof(std::decay<decltype(MockDebugSession::readMemoryBuffer[0])>::type))], 1, scratchAllocationBase, 0,
                                                         ptss, nullptr, false, 6, false, true);

        r0Thread0[5] = 1 << 10; // first surface state
    } else {
        r0Thread0[5] = 0;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));
    EXPECT_EQ(0ULL, sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);

    scratchAllocationBase = sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU];
    uint64_t scratchAllocationBase2 = (1ULL << 47) + 0x12000u;
    auto gmmHelper = neoDevice->getGmmHelper();
    auto scratchAllocationBase2Canonized = gmmHelper->canonize(scratchAllocationBase2);

    if (hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        hwHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                         &session->readMemoryBuffer[1 * (hwHelper.getRenderSurfaceStateSize() / sizeof(std::decay<decltype(MockDebugSession::readMemoryBuffer[0])>::type))], 1, scratchAllocationBase, 0,
                                                         ptss, nullptr, false, 6, false, true);

        r0Thread0[5] = 1 << 10; // first surface state

        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = scratchAllocationBase;
    } else {
        r0Thread0[3] = 1; // (2) ^ 1
        r0Thread0[5] = 2 << 10;
        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = ((r0Thread0[5] >> 10) << 10) + sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    }

    sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU] = ((r0Thread0[4] >> 5) << 5) + sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));

    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU], sba[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU], sba[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU], sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);

    if (hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 128;
        hwHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                         &session->readMemoryBuffer[2 * (hwHelper.getRenderSurfaceStateSize() / sizeof(std::decay<decltype(MockDebugSession::readMemoryBuffer[0])>::type))], 1, scratchAllocationBase2Canonized, 0,
                                                         ptss, nullptr, false, 6, false, true);

        r0Thread1[5] = 2 << 10; // second surface state

        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = scratchAllocationBase2 + ptss;
    } else {
        r0Thread1[3] = 2; // (2) ^ 2
        r0Thread1[5] = 2 << 10;
        sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = ((r0Thread1[5] >> 10) << 10) + sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU];
    }

    sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU] = ((r0Thread1[4] >> 5) << 5) + sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU];

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId1, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));

    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_GENERAL_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_SURFACE_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU], sba[ZET_DEBUG_SBA_DYNAMIC_STATE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU], sba[ZET_DEBUG_SBA_INDIRECT_OBJECT_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU], sba[ZET_DEBUG_SBA_INSTRUCTION_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SURFACE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDLESS_SAMPLER_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU], sba[ZET_DEBUG_SBA_BINDING_TABLE_INTEL_GPU]);
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU], sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);

    if (hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        const uint32_t ptss = 0;
        hwHelper.setRenderSurfaceStateForScratchResource(neoDevice->getRootDeviceEnvironment(),
                                                         &session->readMemoryBuffer[0], 1, 0xdeadbeef, 0,
                                                         ptss, nullptr, false, 6, false, true);

        r0Thread1[5] = 0; // Surface state at index 0
    } else {
        r0Thread1[3] = 0;
        r0Thread1[5] = 0;
    }
    sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU] = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->readSbaRegisters(threadId1, 0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, sba));
    EXPECT_EQ(sbaExpected[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU], sba[ZET_DEBUG_SBA_SCRATCH_SPACE_INTEL_GPU]);
}

TEST(DebugSessionTest, givenCanonicalOrDecanonizedAddressWhenCheckingValidAddressThenTrueReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0xffffffff12345678;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    EXPECT_TRUE(sessionMock->isValidGpuAddress(&desc));

    auto gmmHelper = neoDevice->getGmmHelper();
    auto decanonized = gmmHelper->decanonize(desc.address);
    desc.address = decanonized;
    EXPECT_TRUE(sessionMock->isValidGpuAddress(&desc));
}

TEST(DebugSessionTest, givenInvalidAddressWhenCheckingValidAddressThenFalseIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp);
    auto gmmHelper = neoDevice->getGmmHelper();
    auto addressWidth = gmmHelper->getAddressWidth();
    uint64_t address = maxNBitValue(addressWidth) << 1 | 0x4000;

    zet_debug_memory_space_desc_t desc;
    desc.address = address;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    EXPECT_FALSE(sessionMock->isValidGpuAddress(&desc));

    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_FORCE_UINT32;
    EXPECT_FALSE(sessionMock->isValidGpuAddress(&desc));
}

TEST(DebugSessionTest, givenDebugSessionWhenGettingTimeDiffThenDiffReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);

    auto now = std::chrono::high_resolution_clock::now();
    auto diff = sessionMock->getTimeDifferenceMilliseconds(now);

    EXPECT_LE(0, diff);
}

TEST(DebugSessionTest, givenDebugSessionWhenGetContextSaveAreaGpuVaCalledThenZeroIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);
    sessionMock->returnStateSaveAreaGpuVa = false;

    EXPECT_EQ(0ULL, sessionMock->getContextStateSaveAreaGpuVa(1ULL));
}

TEST(DebugSessionTest, givenNewAttentionRaisedWithNoExpectedAttentionThenExpectedAttentionNotChanged) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);
    EXPECT_EQ(0UL, sessionMock->expectedAttentionEvents);
    sessionMock->newAttentionRaised(0);
    EXPECT_EQ(0UL, sessionMock->expectedAttentionEvents);
}

TEST(DebugSessionTest, givenNoPendingInterruptsOrNoNewlyStoppedThreadsAndCheckTriggerEventsForAttentionThenDoNotTriggerEvents) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);

    sessionMock->triggerEvents = false;
    sessionMock->expectedAttentionEvents = 0;
    sessionMock->pendingInterrupts.clear();
    sessionMock->newlyStoppedThreads.clear();

    sessionMock->checkTriggerEventsForAttention();
    EXPECT_FALSE(sessionMock->triggerEvents);

    ze_device_thread_t apiThread = {0, 0, 0, 1};
    sessionMock->pendingInterrupts.push_back({apiThread, false});

    sessionMock->checkTriggerEventsForAttention();
    EXPECT_TRUE(sessionMock->triggerEvents);

    EuThread::ThreadId thread = {0, 0, 0, 0, 1};
    sessionMock->pendingInterrupts.clear();
    sessionMock->newlyStoppedThreads.push_back(thread);

    EXPECT_TRUE(sessionMock->triggerEvents);
}

TEST(DebugSessionTest, givenNoPendingInterruptsWhenSendInterruptsCalledThenNoInterruptsSent) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto sessionMock = std::make_unique<MockDebugSession>(config, nullptr);
    sessionMock->interruptSent = false;
    sessionMock->pendingInterrupts.clear();
    sessionMock->sendInterrupts();
    EXPECT_FALSE(sessionMock->interruptSent);
}

TEST(DebugSessionTest, GivenRootDeviceAndTileAttachWhenDebugSessionIsCreatedThenThreadsAreNotCreated) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto sessionMock = std::make_unique<MockDebugSession>(config, &deviceImp, false);
    ASSERT_NE(nullptr, sessionMock);

    EXPECT_EQ(0u, sessionMock->allThreads.size());
}

TEST_F(MultiTileDebugSessionTest, GivenSubDeviceAndTileAttachWhenRootDeviceDebugSessionCreateFailsThenTileAttachFails) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        GTEST_SKIP();
    }
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    device->getNEODevice()->getExecutionEnvironment()->releaseRootDeviceEnvironmentResources(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);

    auto osInterface = new OsInterfaceWithDebugAttach;
    osInterface->debugAttachAvailable = false;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();

    zet_debug_session_handle_t debugSesion = nullptr;
    auto retVal = zetDebugAttach(subDevice0->toHandle(), &config, &debugSesion);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, retVal);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenGettingDebugPropertiesThenDebugAttachIsSetForRootAndSubdevices) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        GTEST_SKIP();
    }

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    device->getNEODevice()->getExecutionEnvironment()->releaseRootDeviceEnvironmentResources(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();

    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);

    result = zetDeviceGetDebugProperties(subDevice0->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);

    result = zetDeviceGetDebugProperties(subDevice1->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenAttachingToRootDeviceThenTileAttachIsDisabledAndSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto deviceImp = static_cast<DeviceImp *>(device);
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);
    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, debugSession);
    EXPECT_TRUE(sessionMock->isAttached());
    EXPECT_FALSE(sessionMock->tileAttachEnabled);
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenAttachingToTileDevicesThenDebugSessionForRootIsCreatedAndTileSessionsAreReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession1 = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();
    auto tileSession1 = static_cast<MockDebugSession *>(sessionMock->tileSessions[1].first);

    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    EXPECT_TRUE(sessionMock->tileSessions[0].second);
    EXPECT_EQ(sessionMock->tileSessions[0].first, L0::DebugSession::fromHandle(debugSession0));
    EXPECT_TRUE(sessionMock->tileSessions[0].first->isAttached());

    EXPECT_NE(nullptr, deviceImp->getDebugSession(config));

    EXPECT_FALSE(tileSession1->attachTileCalled);
    result = zetDebugAttach(subDevice1->toHandle(), &config, &debugSession1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession1);

    EXPECT_TRUE(sessionMock->tileSessions[1].second);
    EXPECT_EQ(sessionMock->tileSessions[1].first, L0::DebugSession::fromHandle(debugSession1));
    EXPECT_TRUE(sessionMock->tileSessions[1].first->isAttached());

    EXPECT_TRUE(tileSession1->attachTileCalled);
    EXPECT_FALSE(tileSession1->detachTileCalled);

    result = zetDebugDetach(debugSession1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, deviceImp->getDebugSession(config));
    EXPECT_FALSE(sessionMock->tileSessions[1].second);
    EXPECT_FALSE(sessionMock->tileSessions[1].first->isAttached());

    EXPECT_TRUE(tileSession1->detachTileCalled);
    ASSERT_EQ(1u, sessionMock->cleanRootSessionDeviceIndices.size());
    EXPECT_EQ(1u, sessionMock->cleanRootSessionDeviceIndices[0]);

    result = zetDebugDetach(debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, deviceImp->getDebugSession(config));
}

TEST_F(MultiTileDebugSessionTest, givenTileAttachEnabledWhenTileAttachFailsDuringDebugSessionCreateThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    sessionMock->tileSessions[0].second = true;

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    ze_result_t result = ZE_RESULT_SUCCESS;

    auto debugSession = subDevice0->createDebugSession(config, result, false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(MultiTileDebugSessionTest, givenAttachedTileDeviceWhenAttachingToRootDeviceThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession1 = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugAttach(deviceImp->toHandle(), &config, &debugSession1);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    result = zetDebugDetach(debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MultiTileDebugSessionTest, givenAttachedRootDeviceWhenAttachingToTiletDeviceThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSessionRoot = nullptr;

    L0::Device *device = driverHandle->devices[0];
    auto neoDevice = device->getNEODevice();
    auto deviceImp = static_cast<DeviceImp *>(device);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();

    auto result = zetDebugAttach(deviceImp->toHandle(), &config, &debugSessionRoot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession0);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_EQ(nullptr, debugSession0);

    result = zetDebugDetach(debugSessionRoot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct AffinityMaskForSingleSubDevice : MultipleDevicesWithCustomHwInfo {
    void setUp() {
        DebugManager.flags.ZE_AFFINITY_MASK.set("0.1");
        MultipleDevicesWithCustomHwInfo::numSubDevices = 2;
        MultipleDevicesWithCustomHwInfo::setUp();
    }

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using AffinityMaskForSingleSubDeviceTest = Test<AffinityMaskForSingleSubDevice>;

TEST_F(AffinityMaskForSingleSubDeviceTest, givenDeviceDebugSessionWhenSendingInterruptsThenInterruptIsSentWithCorrectDeviceIndex) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto sessionMock = std::make_unique<MockDebugSession>(config, deviceImp);

    sessionMock->interruptImpResult = ZE_RESULT_SUCCESS;
    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = sessionMock->interrupt(apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    sessionMock->sendInterrupts();

    EXPECT_TRUE(sessionMock->interruptSent);
    EXPECT_EQ(1u, sessionMock->interruptedDevices[0]);
}

} // namespace ult
} // namespace L0
