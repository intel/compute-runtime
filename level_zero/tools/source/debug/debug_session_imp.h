/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/debug/debug_session.h"

#include <atomic>
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
        createEuThreads();
    }

    ze_result_t interrupt(ze_device_thread_t thread) override;
    ze_result_t resume(ze_device_thread_t thread) override;

    ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override;
    ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override;
    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override;

    DebugSession *attachTileDebugSession(Device *device) override;
    void detachTileDebugSession(DebugSession *tileSession) override;
    bool areAllTileDebugSessionDetached() override;

    static const SIP::regset_desc *getSbaRegsetDesc();
    static uint32_t typeToRegsetFlags(uint32_t type);
    constexpr static int64_t interruptTimeout = 2000;

    using ApiEventQueue = std::queue<zet_debug_event_t>;

  protected:
    MOCKABLE_VIRTUAL ze_result_t readRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues);
    MOCKABLE_VIRTUAL ze_result_t writeRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues);
    Error resumeThreadsWithinDevice(uint32_t deviceIndex, ze_device_thread_t physicalThread);
    MOCKABLE_VIRTUAL bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds);
    MOCKABLE_VIRTUAL bool checkThreadIsResumed(const EuThread::ThreadId &threadID);

    virtual ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) = 0;
    virtual ze_result_t interruptImp(uint32_t deviceIndex) = 0;

    virtual ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) = 0;
    virtual ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) = 0;
    ze_result_t validateThreadAndDescForMemoryAccess(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc);

    virtual void enqueueApiEvent(zet_debug_event_t &debugEvent) = 0;
    virtual bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) = 0;

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
    void validateAndSetStateSaveAreaHeader(const std::vector<char> &data);
    virtual void readStateSaveAreaHeader(){};

    virtual uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) {
        return 0;
    };

    ze_result_t registersAccessHelper(const EuThread *thread, const SIP::regset_desc *regdesc,
                                      uint32_t start, uint32_t count, void *pRegisterValues, bool write);

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

    bool isValidGpuAddress(uint64_t address) const;

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

    std::vector<std::pair<DebugSession *, bool>> tileSessions; // DebugSession, attached
    bool tileAttachEnabled = false;

    ThreadHelper asyncThread;
    std::mutex asyncThreadMutex;

    ApiEventQueue apiEvents;
    std::condition_variable apiEventCondition;
};

} // namespace L0
