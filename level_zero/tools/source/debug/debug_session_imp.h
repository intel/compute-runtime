/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/debug/debug_session.h"

#include "common/StateSaveAreaHeader.h"

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <unordered_set>

namespace SIP {
struct StateSaveAreaHeader;
struct regset_desc;
struct sr_ident;
struct sip_command;
} // namespace SIP

namespace L0 {

struct DebugSessionImp : DebugSession {

    enum class Error {
        Success,
        ThreadsRunning,
        Unknown
    };

    DebugSessionImp(const zet_debug_config_t &config, Device *device) : DebugSession(config, device) {
        tileAttachEnabled = NEO::DebugManager.flags.ExperimentalEnableTileAttach.get();
    }

    ze_result_t interrupt(ze_device_thread_t thread) override;
    ze_result_t resume(ze_device_thread_t thread) override;

    ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override;
    ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override;
    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override;

    DebugSession *attachTileDebugSession(Device *device) override;
    void detachTileDebugSession(DebugSession *tileSession) override;
    bool areAllTileDebugSessionDetached() override;

    void setAttachMode(bool isRootAttach) override {
        if (isRootAttach) {
            tileAttachEnabled = false;
        }
    }

    virtual void attachTile() = 0;
    virtual void detachTile() = 0;
    virtual void cleanRootSessionAfterDetach(uint32_t deviceIndex) = 0;

    static const SIP::regset_desc *getSbaRegsetDesc();
    static uint32_t typeToRegsetFlags(uint32_t type);
    constexpr static int64_t interruptTimeout = 2000;

    using ApiEventQueue = std::queue<zet_debug_event_t>;

  protected:
    MOCKABLE_VIRTUAL ze_result_t readRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues);
    MOCKABLE_VIRTUAL ze_result_t writeRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues);
    Error resumeThreadsWithinDevice(uint32_t deviceIndex, ze_device_thread_t physicalThread);
    MOCKABLE_VIRTUAL bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds);
    void applyResumeWa(uint8_t *bitmask, size_t bitmaskSize);
    MOCKABLE_VIRTUAL bool checkThreadIsResumed(const EuThread::ThreadId &threadID);

    virtual ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) = 0;
    virtual ze_result_t interruptImp(uint32_t deviceIndex) = 0;

    virtual ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) = 0;
    virtual ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) = 0;

    template <class bufferType, bool write>
    ze_result_t slmMemoryAccess(EuThread::ThreadId threadId, const zet_debug_memory_space_desc_t *desc, size_t size, bufferType buffer);

    ze_result_t validateThreadAndDescForMemoryAccess(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc);

    virtual void enqueueApiEvent(zet_debug_event_t &debugEvent) = 0;
    MOCKABLE_VIRTUAL bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic);

    ze_result_t readSbaRegisters(EuThread::ThreadId thread, uint32_t start, uint32_t count, void *pRegisterValues);
    MOCKABLE_VIRTUAL bool isForceExceptionOrForceExternalHaltOnlyExceptionReason(uint32_t *cr0);
    void sendInterrupts();
    MOCKABLE_VIRTUAL void markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(EuThread::ThreadId threadId, uint64_t memoryHandle);
    MOCKABLE_VIRTUAL void fillResumeAndStoppedThreadsFromNewlyStopped(std::vector<EuThread::ThreadId> &resumeThreads, std::vector<EuThread::ThreadId> &stoppedThreadsToReport);
    MOCKABLE_VIRTUAL void generateEventsAndResumeStoppedThreads();
    MOCKABLE_VIRTUAL void resumeAccidentallyStoppedThreads(const std::vector<EuThread::ThreadId> &threadIds);
    MOCKABLE_VIRTUAL void generateEventsForStoppedThreads(const std::vector<EuThread::ThreadId> &threadIds);
    MOCKABLE_VIRTUAL void generateEventsForPendingInterrupts();

    const SIP::StateSaveAreaHeader *getStateSaveAreaHeader();
    void validateAndSetStateSaveAreaHeader(uint64_t vmHandle, uint64_t gpuVa);
    virtual void readStateSaveAreaHeader(){};

    virtual uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) = 0;

    ze_result_t registersAccessHelper(const EuThread *thread, const SIP::regset_desc *regdesc,
                                      uint32_t start, uint32_t count, void *pRegisterValues, bool write);

    void slmSipVersionCheck();
    MOCKABLE_VIRTUAL ze_result_t cmdRegisterAccessHelper(const EuThread::ThreadId &threadId, SIP::sip_command &command, bool write);
    MOCKABLE_VIRTUAL ze_result_t waitForCmdReady(EuThread::ThreadId threadId, uint16_t retryCount);

    const SIP::regset_desc *typeToRegsetDesc(uint32_t type);
    uint32_t getRegisterSize(uint32_t type);

    size_t calculateThreadSlotOffset(EuThread::ThreadId threadId);
    size_t calculateRegisterOffsetInThreadSlot(const SIP::regset_desc *const regdesc, uint32_t start);

    void newAttentionRaised(uint32_t deviceIndex) {
        if (expectedAttentionEvents > 0) {
            expectedAttentionEvents--;
        }
    }

    void checkTriggerEventsForAttention() {
        if (pendingInterrupts.size() > 0 || newlyStoppedThreads.size()) {
            if (expectedAttentionEvents == 0) {
                triggerEvents = true;
            }
        }
    }

    bool isValidGpuAddress(const zet_debug_memory_space_desc_t *desc) const;

    MOCKABLE_VIRTUAL int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) {
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDifferenceMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count();
        return timeDifferenceMs;
    }

    std::chrono::high_resolution_clock::time_point interruptTime;
    std::atomic<bool> interruptSent = false;
    std::atomic<bool> triggerEvents = false;

    uint32_t expectedAttentionEvents = 0;

    std::mutex interruptMutex;
    std::mutex threadStateMutex;
    std::queue<ze_device_thread_t> interruptRequests;
    std::vector<std::pair<ze_device_thread_t, bool>> pendingInterrupts;
    std::vector<EuThread::ThreadId> newlyStoppedThreads;
    std::vector<char> stateSaveAreaHeader;
    SIP::version minSlmSipVersion = {2, 0, 0};
    bool sipSupportsSlm = false;

    std::vector<std::pair<DebugSessionImp *, bool>> tileSessions; // DebugSession, attached
    bool tileAttachEnabled = false;
    bool tileSessionsEnabled = false;

    ThreadHelper asyncThread;
    std::mutex asyncThreadMutex;

    ApiEventQueue apiEvents;
    std::condition_variable apiEventCondition;
    constexpr static uint16_t slmAddressSpaceTag = 28;
    constexpr static uint16_t slmSendBytesSize = 16;
    constexpr static uint16_t sipRetryCount = 10;
    uint32_t maxUnitsPerLoop = EXCHANGE_BUFFER_SIZE / slmSendBytesSize;
};

template <class bufferType, bool write>
ze_result_t DebugSessionImp::slmMemoryAccess(EuThread::ThreadId threadId, const zet_debug_memory_space_desc_t *desc, size_t size, bufferType buffer) {
    ze_result_t status;

    if (!sipSupportsSlm) {
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }

    SIP::sip_command sipCommand = {0};

    uint64_t offset = desc->address & maxNBitValue(slmAddressSpaceTag);
    // SIP accesses SLM in units of slmSendBytesSize at offset allignment of slmSendBytesSize
    uint32_t frontPadding = offset % slmSendBytesSize;
    uint64_t alignedOffset = offset - frontPadding;
    uint32_t remainingSlmSendUnits = static_cast<uint32_t>(std::ceil(static_cast<float>(size) / slmSendBytesSize));

    if ((size + frontPadding) > (remainingSlmSendUnits * slmSendBytesSize)) {
        remainingSlmSendUnits++;
    }

    std::unique_ptr<char[]> tmpBuffer(new char[remainingSlmSendUnits * slmSendBytesSize]);

    if constexpr (write) {
        size_t tailPadding = (size % slmSendBytesSize) ? slmSendBytesSize - (size % slmSendBytesSize) : 0;
        if ((frontPadding || tailPadding)) {
            zet_debug_memory_space_desc_t alignedDesc = *desc;
            alignedDesc.address = desc->address - frontPadding;
            size_t alignedSize = remainingSlmSendUnits * slmSendBytesSize;

            status = slmMemoryAccess<void *, false>(threadId, &alignedDesc, alignedSize, tmpBuffer.get());
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }
        }

        memcpy_s(tmpBuffer.get() + frontPadding, size, buffer, size);
    }

    status = waitForCmdReady(threadId, sipRetryCount);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t loops = static_cast<uint32_t>(std::ceil(static_cast<float>(remainingSlmSendUnits) / maxUnitsPerLoop));
    uint32_t accessUnits = 0;
    uint32_t countReadyBytes = 0;
    sipCommand.offset = alignedOffset;

    for (uint32_t loop = 0; loop < loops; loop++) {

        if (remainingSlmSendUnits >= maxUnitsPerLoop) {
            accessUnits = maxUnitsPerLoop;
        } else {
            accessUnits = remainingSlmSendUnits;
        }

        if constexpr (write) {
            sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_WRITE);
            sipCommand.size = static_cast<uint32_t>(accessUnits);
            memcpy_s(sipCommand.buffer, accessUnits * slmSendBytesSize, tmpBuffer.get() + countReadyBytes, accessUnits * slmSendBytesSize);
        } else {
            sipCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::SLM_READ);
            sipCommand.size = static_cast<uint32_t>(accessUnits);
        }

        status = cmdRegisterAccessHelper(threadId, sipCommand, true);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        status = resumeImp(std::vector<EuThread::ThreadId>{threadId}, threadId.tileIndex);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        status = waitForCmdReady(threadId, sipRetryCount);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        if constexpr (!write) { // Read need an extra access to retrieve data
            status = cmdRegisterAccessHelper(threadId, sipCommand, false);
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }

            memcpy_s(tmpBuffer.get() + countReadyBytes, accessUnits * slmSendBytesSize, sipCommand.buffer, accessUnits * slmSendBytesSize);
        }

        remainingSlmSendUnits -= accessUnits;
        countReadyBytes += accessUnits * slmSendBytesSize;
        sipCommand.offset += accessUnits * slmSendBytesSize;
    }

    if constexpr (!write) {
        memcpy_s(buffer, size, tmpBuffer.get() + frontPadding, size);
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
