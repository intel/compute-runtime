/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

namespace L0 {
DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd, void *params);

namespace ult {

using CreateDebugSessionHelperFunc = decltype(&L0::createDebugSessionHelper);

class OsInterfaceWithDebugAttach : public NEO::OSInterface {
  public:
    OsInterfaceWithDebugAttach() : OSInterface() {}
    bool isDebugAttachAvailable() const override {
        return debugAttachAvailable;
    }

    bool debugAttachAvailable = true;
};

struct DebugSessionMock : public L0::DebugSession {
    using L0::DebugSession::allThreads;
    using L0::DebugSession::debugArea;
    using L0::DebugSession::fillDevicesFromThread;
    using L0::DebugSession::getPerThreadScratchOffset;
    using L0::DebugSession::getSingleThreadsForDevice;
    using L0::DebugSession::isBindlessSystemRoutine;

    DebugSessionMock(const zet_debug_config_t &config, L0::Device *device) : DebugSession(config, device), config(config) {
        topologyMap = buildMockTopology(device);
    }

    bool closeConnection() override {
        closeConnectionCalled = true;
        return true;
    }

    static NEO::TopologyMap buildMockTopology(L0::Device *device) {
        NEO::TopologyMap topologyMap;
        auto &hwInfo = device->getHwInfo();
        uint32_t tileCount;
        if (hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid) {
            tileCount = hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount;
        } else {
            tileCount = 1;
        }

        for (uint32_t deviceIdx = 0; deviceIdx < tileCount; deviceIdx++) {
            NEO::TopologyMapping mapping;
            for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
                mapping.sliceIndices.push_back(static_cast<int>(i));
            }
            if (hwInfo.gtSystemInfo.SliceCount == 1) {
                for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount; i++) {
                    mapping.subsliceIndices.push_back(static_cast<int>(i));
                }
            }
            topologyMap[deviceIdx] = mapping;
        }
        return topologyMap;
    }

    ze_result_t initialize() override {
        if (config.pid == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        createEuThreads();
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t interrupt(ze_device_thread_t thread) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t resume(ze_device_thread_t thread) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    void startAsyncThread() override {
        asyncThreadStarted = true;
    }

    bool readModuleDebugArea() override {
        return true;
    }

    void detachTileDebugSession(DebugSession *tileSession) override {}
    bool areAllTileDebugSessionDetached() override { return true; }

    L0::DebugSession *attachTileDebugSession(L0::Device *device) override {
        return nullptr;
    }

    void setAttachMode(bool isRootAttach) override {}

    const NEO::TopologyMap &getTopologyMap() override {
        return this->topologyMap;
    }

    NEO::TopologyMap topologyMap;
    zet_debug_config_t config;
    bool asyncThreadStarted = false;
    bool closeConnectionCalled = false;
};

struct MockDebugSession : public L0::DebugSessionImp {

    using L0::DebugSession::allThreads;
    using L0::DebugSession::debugArea;

    using L0::DebugSessionImp::applyResumeWa;
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

    ze_result_t waitForCmdReady(EuThread::ThreadId threadId, uint16_t retryCount) override {
        return DebugSessionImp::waitForCmdReady(threadId, 1);
    }

    ze_result_t cmdRegisterAccessHelper(const EuThread::ThreadId &threadId, SIP::sip_command &command, bool write) override {

        ze_result_t status = ZE_RESULT_SUCCESS;

        if (slmTesting) {
            uint32_t size = command.size * slmSendBytesSize;

            // initial wait for ready
            if (!write && slmCmdRegisterCmdvalue == static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME)) {
                if (!memcmp(slmMemory, "FailWaiting", strlen("FailWaiting"))) {
                    return ZE_RESULT_FORCE_UINT32;
                }
                command.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);
                status = ZE_RESULT_SUCCESS;
            } else {
                if (write) { // writing to CMD register
                    if (forceCmdAccessFail) {
                        return ZE_RESULT_FORCE_UINT32;
                    } else if (command.command == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_WRITE)) {
                        memcpy_s(slmMemory + command.offset, size, command.buffer, size);
                    }
                    slmCmdRegisterCmdvalue = command.command;
                }

                if (slmCmdRegisterCmdvalue != static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME)) {
                    slmCmdRegisterAccessCount++;
                }

                if (slmCmdRegisterAccessCount == slmCmdRegisterAccessReadyCount) { // SIP restores cmd to READY

                    command.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::READY);

                    if (slmCmdRegisterCmdvalue == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_WRITE)) {
                        slmCmdRegisterAccessCount = 0;
                        slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
                    }
                } else if (slmCmdRegisterAccessCount == slmCmdRegisterAccessReadyCount + 1) { // Reading the data does an extra call to retrieve the data
                    if (!memcmp(slmMemory, "FailReadingData", strlen("FailReadingData"))) {
                        status = ZE_RESULT_FORCE_UINT32;
                    } else if (slmCmdRegisterCmdvalue == static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_READ)) {
                        memcpy_s(command.buffer, size, slmMemory + command.offset, size);
                    }

                    slmCmdRegisterAccessCount = 0;
                    slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
                }
            }
            return status;
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

    int slmCmdRegisterAccessCount = 0;
    uint32_t slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);
    bool forceCmdAccessFail = false;
    int slmCmdRegisterAccessReadyCount = 2;
    bool slmTesting = false;
    static constexpr int slmSize = 256;
    char slmMemory[slmSize];
    char originalSlmMemory[slmSize];
};

} // namespace ult
} // namespace L0
