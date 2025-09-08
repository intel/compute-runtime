/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_sip.h"

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

    ze_result_t readRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint32_t getRegisterSize(uint32_t type) override {
        return 0;
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
    using L0::DebugSessionImp::addThreadToNewlyStoppedFromRaisedAttention;
    using L0::DebugSessionImp::allocateStateSaveAreaMemory;
    using L0::DebugSessionImp::apiEvents;
    using L0::DebugSessionImp::applyResumeWa;
    using L0::DebugSessionImp::attentionEventContext;
    using L0::DebugSessionImp::calculateSrMagicOffset;
    using L0::DebugSessionImp::calculateThreadSlotOffset;
    using L0::DebugSessionImp::checkTriggerEventsForAttention;
    using L0::DebugSessionImp::dumpDebugSurfaceToFile;
    using L0::DebugSessionImp::fillResumeAndStoppedThreadsFromNewlyStopped;
    using L0::DebugSessionImp::generateEventsAndResumeStoppedThreads;
    using L0::DebugSessionImp::generateEventsForPendingInterrupts;
    using L0::DebugSessionImp::generateEventsForStoppedThreads;
    using L0::DebugSessionImp::getRegisterSize;
    using L0::DebugSessionImp::getStateSaveAreaHeader;
    using L0::DebugSessionImp::interruptTimeout;
    using L0::DebugSessionImp::isValidNode;
    using L0::DebugSessionImp::newAttentionRaised;
    using L0::DebugSessionImp::pollFifo;
    using L0::DebugSessionImp::readDebugScratchRegisters;
    using L0::DebugSessionImp::readFifo;
    using L0::DebugSessionImp::readModeFlags;
    using L0::DebugSessionImp::readSbaRegisters;
    using L0::DebugSessionImp::readThreadScratchRegisters;
    using L0::DebugSessionImp::registersAccessHelper;
    using L0::DebugSessionImp::resumeAccidentallyStoppedThreads;
    using L0::DebugSessionImp::sendInterrupts;
    using L0::DebugSessionImp::stateSaveAreaMemory;
    using L0::DebugSessionImp::typeToRegsetDesc;
    using L0::DebugSessionImp::updateStoppedThreadsAndCheckTriggerEvents;
    using L0::DebugSessionImp::validateAndSetStateSaveAreaHeader;

    using L0::DebugSessionImp::interruptSent;
    using L0::DebugSessionImp::stateSaveAreaHeader;
    using L0::DebugSessionImp::triggerEvents;

    using L0::DebugSessionImp::expectedAttentionEvents;
    using L0::DebugSessionImp::fifoPollInterval;
    using L0::DebugSessionImp::interruptMutex;
    using L0::DebugSessionImp::interruptRequests;
    using L0::DebugSessionImp::isValidGpuAddress;
    using L0::DebugSessionImp::lastFifoReadTime;
    using L0::DebugSessionImp::minSlmSipVersion;
    using L0::DebugSessionImp::newlyStoppedThreads;
    using L0::DebugSessionImp::pendingInterrupts;
    using L0::DebugSessionImp::readStateSaveAreaHeader;
    using L0::DebugSessionImp::sipSupportsSlm;
    using L0::DebugSessionImp::slmMemoryAccess;
    using L0::DebugSessionImp::slmSipVersionCheck;
    using L0::DebugSessionImp::tileAttachEnabled;
    using L0::DebugSessionImp::tileSessions;

    MockDebugSession(const zet_debug_config_t &config, L0::Device *device) : MockDebugSession(config, device, true) {}

    MockDebugSession(const zet_debug_config_t &config, L0::Device *device, bool rootAttach) : MockDebugSession(config, device, rootAttach, 2) {
    }

    MockDebugSession(const zet_debug_config_t &config, L0::Device *device, bool rootAttach, uint32_t stateSaveHeaderVersion) : DebugSessionImp(config, device) {
        if (device) {
            topologyMap = DebugSessionMock::buildMockTopology(device);
        }
        setAttachMode(rootAttach);
        if (rootAttach) {
            createEuThreads();
        }
        if (stateSaveHeaderVersion == 3) {
            stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(3);
        } else {
            stateSaveAreaHeader = NEO::MockSipData::createStateSaveAreaHeader(2);
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
            tileSessionsEnabled = true;
        }

        return ZE_RESULT_SUCCESS;
    }

    void attachTile() override { attachTileCalled = true; };
    void detachTile() override { detachTileCalled = true; };
    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override { cleanRootSessionDeviceIndices.push_back(deviceIndex); };

    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
        return readMemoryResult;
    }

    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override {
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t isValidNode(uint64_t vmHandle, uint64_t gpuVa, SIP::fifo_node &node) override {
        if (isValidNodeResult != ZE_RESULT_SUCCESS) {
            return isValidNodeResult;
        } else {
            return DebugSessionImp::isValidNode(vmHandle, gpuVa, node);
        }
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
        readGpuMemoryCallCount++;
        if (forcereadGpuMemoryFailOnCount == readGpuMemoryCallCount) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        if (gpuVa != 0 && gpuVa >= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()) &&
            ((gpuVa + size) <= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data() + stateSaveAreaHeader.size()))) {
            [[maybe_unused]] auto offset = ptrDiff(gpuVa, reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()));
            memcpy_s(output, size, reinterpret_cast<void *>(gpuVa), size);
        }

        else if (!readMemoryBuffer.empty() &&
                 (gpuVa != 0 && gpuVa >= reinterpret_cast<uint64_t>(readMemoryBuffer.data()) &&
                  gpuVa <= reinterpret_cast<uint64_t>(ptrOffset(readMemoryBuffer.data(), readMemoryBuffer.size())))) {
            memcpy_s(output, size, reinterpret_cast<void *>(gpuVa), std::min(size, readMemoryBuffer.size()));
        }
        return readMemoryResult;
    }
    ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override {
        writeGpuMemoryCallCount++;
        if (forceWriteGpuMemoryFailOnCount == writeGpuMemoryCallCount) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        if (gpuVa != 0 && gpuVa >= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()) &&
            ((gpuVa + size) <= reinterpret_cast<uint64_t>(stateSaveAreaHeader.data() + stateSaveAreaHeader.size()))) {
            [[maybe_unused]] auto offset = ptrDiff(gpuVa, reinterpret_cast<uint64_t>(stateSaveAreaHeader.data()));
            memcpy_s(reinterpret_cast<void *>(gpuVa), size, input, size);
        }
        return writeMemoryResult;
    }

    ze_result_t readThreadScratchRegisters(EuThread::ThreadId thread, uint32_t start, uint32_t count, void *pRegisterValues) override {

        if (readThreadScratchRegistersResult != ZE_RESULT_FORCE_UINT32) {
            return readThreadScratchRegistersResult;
        }
        return DebugSessionImp::readThreadScratchRegisters(thread, start, count, pRegisterValues);
    }

    ze_result_t updateStoppedThreadsAndCheckTriggerEvents(const AttentionEventFields &attention, uint32_t tileIndex, std::vector<EuThread::ThreadId> &threadsWithAttention) override { return ZE_RESULT_SUCCESS; }
    void resumeAccidentallyStoppedThreads(const std::vector<EuThread::ThreadId> &threadIds) override {
        resumeAccidentallyStoppedCalled++;
        return DebugSessionImp::resumeAccidentallyStoppedThreads(threadIds);
    }

    void startAsyncThread() override {}
    bool readModuleDebugArea() override { return true; }

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override {
        apiEvents.push(debugEvent);
        apiEventCondition.notify_all();
    }

    bool isAIPequalToThreadStartIP(uint32_t *cr0, uint32_t *dbg0) override {
        if (callBaseisAIPequalToThreadStartIP) {
            return DebugSessionImp::isAIPequalToThreadStartIP(cr0, dbg0);
        }
        return forceAIPEqualStartIP;
    }

    bool isForceExceptionOrForceExternalHaltOnlyExceptionReason(uint32_t *cr0) override {
        if (callBaseIsForceExceptionOrForceExternalHaltOnlyExceptionReason) {
            return isForceExceptionOrForceExternalHaltOnlyExceptionReasonBase(cr0);
        }
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

    bool readSystemRoutineIdentFromMemory(EuThread *thread, const void *stateSaveArea, SIP::sr_ident &srMagic) override {
        if (skipReadSystemRoutineIdent) {
            srMagic.count = threadStopped ? 1 : 0;
            return readSystemRoutineIdentRetVal;
        }
        return DebugSessionImp::readSystemRoutineIdentFromMemory(thread, stateSaveArea, srMagic);
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
            if (!write && slmCmdRegisterCmdvalue == static_cast<uint32_t>(NEO::SipKernel::Command::resume)) {
                if (!memcmp(slmMemory, "FailWaiting", strlen("FailWaiting"))) {
                    return ZE_RESULT_FORCE_UINT32;
                }
                command.command = static_cast<uint32_t>(NEO::SipKernel::Command::ready);
                status = ZE_RESULT_SUCCESS;
            } else {
                if (write) { // writing to CMD register
                    if (forceCmdAccessFail) {
                        return ZE_RESULT_FORCE_UINT32;
                    } else if (command.command == static_cast<uint32_t>(NEO::SipKernel::Command::slmWrite)) {
                        memcpy_s(slmMemory + command.offset, size, command.buffer, size);
                    }
                    slmCmdRegisterCmdvalue = command.command;
                }

                if (slmCmdRegisterCmdvalue != static_cast<uint32_t>(NEO::SipKernel::Command::resume)) {
                    slmCmdRegisterAccessCount++;
                }

                if (slmCmdRegisterAccessCount == slmCmdRegisterAccessReadyCount) { // SIP restores cmd to READY

                    command.command = static_cast<uint32_t>(NEO::SipKernel::Command::ready);

                    if (slmCmdRegisterCmdvalue == static_cast<uint32_t>(NEO::SipKernel::Command::slmWrite)) {
                        slmCmdRegisterAccessCount = 0;
                        slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);
                    }
                } else if (slmCmdRegisterAccessCount == slmCmdRegisterAccessReadyCount + 1) { // Reading the data does an extra call to retrieve the data
                    if (!memcmp(slmMemory, "FailReadingData", strlen("FailReadingData"))) {
                        status = ZE_RESULT_FORCE_UINT32;
                    } else if (slmCmdRegisterCmdvalue == static_cast<uint32_t>(NEO::SipKernel::Command::slmRead)) {
                        memcpy_s(command.buffer, size, slmMemory + command.offset, size);
                    }

                    slmCmdRegisterAccessCount = 0;
                    slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);
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
    bool checkThreadIsResumed(const EuThread::ThreadId &threadID, const void *stateSaveArea) override {
        checkThreadIsResumedFromPassedSaveAreaCalled++;
        if (skipCheckThreadIsResumed) {
            return true;
        }
        return L0::DebugSessionImp::checkThreadIsResumed(threadID, stateSaveArea);
    }

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        if (returnStateSaveAreaGpuVa && !this->stateSaveAreaHeader.empty()) {
            return reinterpret_cast<uint64_t>(this->stateSaveAreaHeader.data());
        }
        return 0;
    };

    size_t getContextStateSaveAreaSize(uint64_t memoryHandle) override {
        if (forceStateSaveAreaSize.has_value()) {
            return forceStateSaveAreaSize.value();
        }
        if (stateSaveAreaHeader.size()) {
            auto header = getStateSaveAreaHeader();
            auto stateSaveAreaSize = header->regHeader.num_slices *
                                         header->regHeader.num_subslices_per_slice *
                                         header->regHeader.num_eus_per_subslice *
                                         header->regHeader.num_threads_per_eu *
                                         header->regHeader.state_save_size +
                                     header->versionHeader.size * 8 + header->regHeader.state_area_offset;

            return alignUp(stateSaveAreaSize, MemoryConstants::pageSize);
        }

        return 0;
    }

    int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) override {
        if (returnTimeDiff != -1) {
            return returnTimeDiff;
        }
        return L0::DebugSessionImp::getTimeDifferenceMilliseconds(time);
    }

    const NEO::TopologyMap &getTopologyMap() override {
        return this->topologyMap;
    }

    void checkStoppedThreadsAndGenerateEvents(const std::vector<EuThread::ThreadId> &threads, uint64_t memoryHandle, uint32_t deviceIndex) override {
        checkStoppedThreadsAndGenerateEventsCallCount++;
        return L0::DebugSessionImp::checkStoppedThreadsAndGenerateEvents(threads, memoryHandle, deviceIndex);
    }

    ze_result_t readFifo(uint64_t vmHandle, std::vector<EuThread::ThreadId> &threadsWithAttention) override {
        readFifoCallCount++;
        return L0::DebugSessionImp::readFifo(vmHandle, threadsWithAttention);
    }

    NEO::TopologyMap topologyMap;

    uint32_t readFifoCallCount = 0;
    uint32_t interruptImpCalled = 0;
    uint32_t resumeImpCalled = 0;
    uint32_t resumeAccidentallyStoppedCalled = 0;
    size_t resumeThreadCount = 0;
    ze_result_t interruptImpResult = ZE_RESULT_SUCCESS;
    ze_result_t resumeImpResult = ZE_RESULT_SUCCESS;
    bool onlyForceException = true;
    bool forceAIPEqualStartIP = false;
    bool callBaseIsForceExceptionOrForceExternalHaltOnlyExceptionReason = false;
    bool callBaseisAIPequalToThreadStartIP = false;
    bool threadStopped = true;
    int areRequestedThreadsStoppedReturnValue = -1;
    bool readSystemRoutineIdentRetVal = true;
    size_t readRegistersSizeToFill = 0;
    ze_result_t readSbaBufferResult = ZE_RESULT_SUCCESS;
    ze_result_t readRegistersResult = ZE_RESULT_FORCE_UINT32;
    ze_result_t readMemoryResult = ZE_RESULT_SUCCESS;
    ze_result_t writeMemoryResult = ZE_RESULT_SUCCESS;
    ze_result_t writeRegistersResult = ZE_RESULT_FORCE_UINT32;
    ze_result_t readThreadScratchRegistersResult = ZE_RESULT_FORCE_UINT32;
    ze_result_t isValidNodeResult = ZE_RESULT_SUCCESS;

    uint32_t readStateSaveAreaHeaderCalled = 0;
    uint32_t readRegistersCallCount = 0;
    uint32_t readRegistersReg = 0;
    uint32_t writeRegistersCallCount = 0;
    uint32_t writeRegistersReg = 0;

    uint32_t readGpuMemoryCallCount = 0;
    uint32_t forcereadGpuMemoryFailOnCount = 0;

    uint32_t writeGpuMemoryCallCount = 0;
    uint32_t forceWriteGpuMemoryFailOnCount = 0;

    bool skipWriteResumeCommand = true;
    uint32_t writeResumeCommandCalled = 0;

    bool skipCheckThreadIsResumed = true;
    uint32_t checkThreadIsResumedCalled = 0;
    uint32_t checkThreadIsResumedFromPassedSaveAreaCalled = 0;
    uint32_t checkStoppedThreadsAndGenerateEventsCallCount = 0;

    bool skipReadSystemRoutineIdent = true;
    std::vector<uint32_t> interruptedDevices;
    std::vector<uint32_t> resumedDevices;
    std::vector<std::vector<EuThread::ThreadId>> resumedThreads;

    NEO::SbaTrackedAddresses sba;
    std::vector<uint8_t> readMemoryBuffer;
    uint64_t regs[16];

    int64_t returnTimeDiff = -1;
    bool returnStateSaveAreaGpuVa = true;
    std::optional<size_t> forceStateSaveAreaSize = std::nullopt;

    bool attachTileCalled = false;
    bool detachTileCalled = false;
    std::vector<uint32_t> cleanRootSessionDeviceIndices;

    int slmCmdRegisterAccessCount = 0;
    uint32_t slmCmdRegisterCmdvalue = static_cast<uint32_t>(NEO::SipKernel::Command::resume);
    bool forceCmdAccessFail = false;
    int slmCmdRegisterAccessReadyCount = 2;
    bool slmTesting = false;
    static constexpr int slmSize = 256;
    char slmMemory[slmSize];
    char originalSlmMemory[slmSize];
};

} // namespace ult
} // namespace L0
