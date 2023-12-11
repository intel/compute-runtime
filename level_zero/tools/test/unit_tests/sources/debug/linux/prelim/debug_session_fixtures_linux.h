/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"

#include "common/StateSaveAreaHeader.h"

#include <atomic>
#include <queue>
#include <type_traits>
#include <unordered_map>

namespace L0 {
namespace ult {

using typeOfUUID = std::decay<decltype(prelim_drm_i915_debug_event_vm_bind::uuids[0])>::type;

struct MockIoctlHandler : public L0::DebugSessionLinuxi915::IoctlHandler {
    using EventPair = std::pair<char *, uint64_t>;
    using EventQueue = std::queue<EventPair>;

    int ioctl(int fd, unsigned long request, void *arg) override {
        ioctlCalled++;

        if ((request == PRELIM_I915_DEBUG_IOCTL_READ_EVENT) && (arg != nullptr)) {
            auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(arg);
            debugEventInput = *debugEvent;

            if (!eventQueue.empty()) {
                auto frontEvent = eventQueue.front();
                memcpy(arg, frontEvent.first, frontEvent.second);
                eventQueue.pop();
                return 0;
            } else {
                debugEventRetVal = -1;
            }
            return debugEventRetVal;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_ACK_EVENT) && (arg != nullptr)) {
            auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(arg);
            debugEventAcked = *debugEvent;
            ackCount++;
            return 0;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_READ_UUID) && (arg != nullptr)) {
            prelim_drm_i915_debug_read_uuid *uuid = reinterpret_cast<prelim_drm_i915_debug_read_uuid *>(arg);
            if (returnUuid) {
                uuid->client_handle = returnUuid->client_handle;
                uuid->handle = returnUuid->handle;
                uuid->flags = returnUuid->flags;
                memcpy(uuid->uuid, returnUuid->uuid, sizeof(prelim_drm_i915_debug_read_uuid::uuid));
                int returnError = 0;
                if (uuid->payload_size >= returnUuid->payload_size) {
                    memcpy(reinterpret_cast<void *>(uuid->payload_ptr), reinterpret_cast<void *>(returnUuid->payload_ptr), returnUuid->payload_size);
                } else {
                    returnError = -1;
                }
                returnUuid = nullptr;
                return returnError;
            }
            return -1;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_VM_OPEN) && (arg != nullptr)) {
            prelim_drm_i915_debug_vm_open *vmOpenIn = reinterpret_cast<prelim_drm_i915_debug_vm_open *>(arg);
            vmOpen = *vmOpenIn;
            return vmOpenRetVal;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_EU_CONTROL) && (arg != nullptr)) {
            prelim_drm_i915_debug_eu_control *euControlArg = reinterpret_cast<prelim_drm_i915_debug_eu_control *>(arg);
            EuControlArg arg;
            arg.euControl = *euControlArg;

            euControlArg->seqno = euControlOutputSeqno;

            if (euControlArg->bitmask_size != 0) {
                arg.euControlBitmaskSize = euControlArg->bitmask_size;
                arg.euControlBitmask = std::make_unique<uint8_t[]>(arg.euControlBitmaskSize);

                memcpy(arg.euControlBitmask.get(), reinterpret_cast<void *>(euControlArg->bitmask_ptr), arg.euControlBitmaskSize);
                if (euControlArg->cmd == PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED && euControlArg->bitmask_ptr && outputBitmask.get()) {
                    memcpy_s(reinterpret_cast<uint64_t *>(euControlArg->bitmask_ptr), euControlArg->bitmask_size, outputBitmask.get(), outputBitmaskSize);
                }
            }
            euControlArgs.push_back(std::move(arg));
        }

        return ioctlRetVal;
    }

    int poll(pollfd *pollFd, unsigned long int numberOfFds, int timeout) override {
        passedTimeout = timeout;
        pollCounter++;

        if (eventQueue.empty() && pollRetVal >= 0) {
            return 0;
        }
        return pollRetVal;
    }

    int64_t pread(int fd, void *buf, size_t count, off_t offset) override {
        preadCalled++;
        preadOffset = offset;
        if ((midZeroReturn > 0) && (preadCalled > 1)) {
            midZeroReturn--;
            return 0;
        }

        if (!pReadArrayRef.empty()) {
            auto offsetInMemory = offset - pReadBase;
            auto result = memcpy_s(buf, count, pReadArrayRef.begin() + offsetInMemory, std::min(count, pReadArrayRef.size() - offsetInMemory));
            if (result == 0) {
                return count;
            }
            return -1;
        }

        if (count > 0 && preadRetVal > 0) {
            memset(buf, 0xaa, count);
        }
        return preadRetVal;
    }

    int64_t pwrite(int fd, const void *buf, size_t count, off_t offset) override {
        pwriteCalled++;
        pwriteOffset = offset;
        if ((midZeroReturn > 0) && (pwriteCalled > 1)) {
            midZeroReturn--;
            return 0;
        }

        if (!pWriteArrayRef.empty()) {
            auto offsetInMemory = offset - pWriteBase;
            auto result = memcpy_s(pWriteArrayRef.begin() + offsetInMemory, pWriteArrayRef.size() - offsetInMemory, buf, count);
            if (result == 0) {
                return count;
            }
            return -1;
        }

        return pwriteRetVal;
    }

    void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) override {
        mmapCalled++;
        if (mmapFail) {
            return MAP_FAILED;
        }
        if (mmapRet) {
            return mmapRet + (off - mmapBase);
        } else {
            auto ret = new char[size];
            if (size > 0) {
                memset(ret, 0xaa, size);
            }
            return ret;
        }
    }

    int munmap(void *addr, size_t size) override {
        munmapCalled++;
        if (mmapRet) {
            return 0;
        }
        if (addr != MAP_FAILED && addr != 0) {
            if (*static_cast<char *>(addr) != static_cast<char>(0xaa)) {
                memoryModifiedInMunmap = true;
            }
            delete[] (char *)addr;
        } else {
            return -1;
        }
        return 0;
    }

    void setPreadMemory(char *memory, size_t size, uint64_t baseAddress) {
        pReadArrayRef = ArrayRef<char>(memory, size);
        pReadBase = baseAddress;
    }

    void setPwriteMemory(char *memory, size_t size, uint64_t baseAddress) {
        pWriteArrayRef = ArrayRef<char>(memory, size);
        pWriteBase = baseAddress;
    }

    struct EuControlArg {
        EuControlArg() : euControlBitmask(nullptr) {
            memset(&euControl, 0, sizeof(euControl));
        }
        EuControlArg(EuControlArg &&in) : euControl(in.euControl), euControlBitmask(std::move(in.euControlBitmask)), euControlBitmaskSize(in.euControlBitmaskSize){};
        prelim_drm_i915_debug_eu_control euControl = {};
        std::unique_ptr<uint8_t[]> euControlBitmask;
        size_t euControlBitmaskSize = 0;
    };

    prelim_drm_i915_debug_event debugEventInput = {};
    prelim_drm_i915_debug_event debugEventAcked = {};
    prelim_drm_i915_debug_read_uuid *returnUuid = nullptr;
    prelim_drm_i915_debug_vm_open vmOpen = {};
    std::vector<EuControlArg> euControlArgs;
    std::unique_ptr<uint8_t[]> outputBitmask;
    size_t outputBitmaskSize = 0;

    int ioctlRetVal = 0;
    int debugEventRetVal = 0;
    int ioctlCalled = 0;
    int pollRetVal = 0;
    int vmOpenRetVal = 600;
    int passedTimeout = 0;

    ArrayRef<char> pReadArrayRef;
    uint64_t pReadBase = 0;
    int64_t preadCalled = 0;
    int64_t preadRetVal = 0;

    ArrayRef<char> pWriteArrayRef;
    uint64_t pWriteBase = 0;
    int64_t pwriteCalled = 0;
    int64_t pwriteRetVal = 0;

    uint64_t preadOffset = 0;
    uint64_t pwriteOffset = 0;

    uint8_t midZeroReturn = 0;
    int64_t mmapCalled = 0;
    int64_t munmapCalled = 0;
    bool mmapFail = false;
    char *mmapRet = nullptr;
    uint64_t mmapBase = 0;
    bool memoryModifiedInMunmap = false;
    std::atomic<int> pollCounter = 0;
    EventQueue eventQueue;
    uint64_t euControlOutputSeqno = 10;
    uint32_t ackCount = 0;
};

struct MockDebugSessionLinuxi915 : public L0::DebugSessionLinuxi915 {
    using L0::DebugSessionImp::allThreads;
    using L0::DebugSessionImp::apiEvents;
    using L0::DebugSessionImp::attachTile;
    using L0::DebugSessionImp::detachTile;
    using L0::DebugSessionImp::enqueueApiEvent;
    using L0::DebugSessionImp::expectedAttentionEvents;
    using L0::DebugSessionImp::fillResumeAndStoppedThreadsFromNewlyStopped;
    using L0::DebugSessionImp::generateEventsForPendingInterrupts;
    using L0::DebugSessionImp::interruptSent;
    using L0::DebugSessionImp::isValidGpuAddress;
    using L0::DebugSessionImp::newAttentionRaised;
    using L0::DebugSessionImp::sipSupportsSlm;
    using L0::DebugSessionImp::stateSaveAreaHeader;
    using L0::DebugSessionImp::tileAttachEnabled;
    using L0::DebugSessionImp::tileSessions;
    using L0::DebugSessionImp::tileSessionsEnabled;
    using L0::DebugSessionImp::triggerEvents;

    using L0::DebugSessionLinuxi915::asyncThread;
    using L0::DebugSessionLinuxi915::blockOnFenceMode;
    using L0::DebugSessionLinuxi915::checkAllEventsCollected;
    using L0::DebugSessionLinuxi915::checkStoppedThreadsAndGenerateEvents;
    using L0::DebugSessionLinuxi915::clientHandle;
    using L0::DebugSessionLinuxi915::clientHandleClosed;
    using L0::DebugSessionLinuxi915::clientHandleToConnection;
    using L0::DebugSessionLinuxi915::closeAsyncThread;
    using L0::DebugSessionLinuxi915::closeInternalEventsThread;
    using L0::DebugSessionLinuxi915::createTileSessionsIfEnabled;
    using L0::DebugSessionLinuxi915::debugArea;
    using L0::DebugSessionLinuxi915::euControlInterruptSeqno;
    using L0::DebugSessionLinuxi915::eventsToAck;
    using L0::DebugSessionLinuxi915::extractVaFromUuidString;
    using L0::DebugSessionLinuxi915::fd;
    using L0::DebugSessionLinuxi915::getContextStateSaveAreaGpuVa;
    using L0::DebugSessionLinuxi915::getContextStateSaveAreaSize;
    using L0::DebugSessionLinuxi915::getIsaInfoForAllInstances;
    using L0::DebugSessionLinuxi915::getISAVMHandle;
    using L0::DebugSessionLinuxi915::getRegisterSetProperties;
    using L0::DebugSessionLinuxi915::getSbaBufferGpuVa;
    using L0::DebugSessionLinuxi915::getStateSaveAreaHeader;
    using L0::DebugSessionLinuxi915::handleEvent;
    using L0::DebugSessionLinuxi915::handleEventsAsync;
    using L0::DebugSessionLinuxi915::handleVmBindEvent;
    using L0::DebugSessionLinuxi915::internalEventQueue;
    using L0::DebugSessionLinuxi915::internalEventThread;
    using L0::DebugSessionLinuxi915::interruptImp;
    using L0::DebugSessionLinuxi915::ioctl;
    using L0::DebugSessionLinuxi915::ioctlHandler;
    using L0::DebugSessionLinuxi915::newlyStoppedThreads;
    using L0::DebugSessionLinuxi915::pendingInterrupts;
    using L0::DebugSessionLinuxi915::pendingVmBindEvents;
    using L0::DebugSessionLinuxi915::printContextVms;
    using L0::DebugSessionLinuxi915::pushApiEvent;
    using L0::DebugSessionLinuxi915::readEventImp;
    using L0::DebugSessionLinuxi915::readGpuMemory;
    using L0::DebugSessionLinuxi915::readInternalEventsAsync;
    using L0::DebugSessionLinuxi915::readModuleDebugArea;
    using L0::DebugSessionLinuxi915::readSbaBuffer;
    using L0::DebugSessionLinuxi915::readStateSaveAreaHeader;
    using L0::DebugSessionLinuxi915::startAsyncThread;
    using L0::DebugSessionLinuxi915::threadControl;
    using L0::DebugSessionLinuxi915::ThreadControlCmd;
    using L0::DebugSessionLinuxi915::typeToRegsetDesc;
    using L0::DebugSessionLinuxi915::typeToRegsetFlags;
    using L0::DebugSessionLinuxi915::uuidL0CommandQueueHandleToDevice;
    using L0::DebugSessionLinuxi915::writeGpuMemory;

    MockDebugSessionLinuxi915(const zet_debug_config_t &config, L0::Device *device, int debugFd, void *params) : DebugSessionLinuxi915(config, device, debugFd, params) {
        clientHandleToConnection[mockClientHandle].reset(new ClientConnection);
        clientHandle = mockClientHandle;
        createEuThreads();
    }

    MockDebugSessionLinuxi915(const zet_debug_config_t &config, L0::Device *device, int debugFd) : MockDebugSessionLinuxi915(config, device, debugFd, nullptr) {}

    ze_result_t initialize() override {
        if (initializeRetVal != ZE_RESULT_FORCE_UINT32) {
            bool isRootDevice = !connectedDevice->getNEODevice()->isSubDevice();
            if (isRootDevice && !tileAttachEnabled) {
                createEuThreads();
            }
            createTileSessionsIfEnabled();

            clientHandle = mockClientHandle;
            return initializeRetVal;
        }
        return DebugSessionLinuxi915::initialize();
    }

    std::unordered_map<uint64_t, std::pair<std::string, uint32_t>> &getClassHandleToIndex() {
        return clientHandleToConnection[mockClientHandle]->classHandleToIndex;
    }

    int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) override {
        if (returnTimeDiff != -1) {
            return returnTimeDiff;
        }
        return L0::DebugSessionLinuxi915::getTimeDifferenceMilliseconds(time);
    }

    int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) override {
        numThreadsPassedToThreadControl = threads.size();
        return L0::DebugSessionLinuxi915::threadControl(threads, tile, threadCmd, bitmask, bitmaskSize);
    }

    ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        readRegistersCallCount++;
        return L0::DebugSessionLinuxi915::readRegisters(thread, type, start, count, pRegisterValues);
    }

    ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        writeRegistersCallCount++;
        writeRegistersReg = type;
        return L0::DebugSessionLinuxi915::writeRegisters(thread, type, start, count, pRegisterValues);
    }

    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override {
        resumedThreads.push_back(threads);
        resumedDevices.push_back(deviceIndex);
        return L0::DebugSessionLinuxi915::resumeImp(threads, deviceIndex);
    }

    ze_result_t interruptImp(uint32_t deviceIndex) override {
        interruptedDevice = deviceIndex;
        return L0::DebugSessionLinuxi915::interruptImp(deviceIndex);
    }

    void handleEvent(prelim_drm_i915_debug_event *event) override {
        handleEventCalledCount++;
        L0::DebugSessionLinuxi915::handleEvent(event);
    }

    bool areRequestedThreadsStopped(ze_device_thread_t thread) override {
        if (allThreadsStopped) {
            return allThreadsStopped;
        }
        return L0::DebugSessionLinuxi915::areRequestedThreadsStopped(thread);
    }

    void ensureThreadStopped(ze_device_thread_t thread) {
        auto threadId = convertToThreadId(thread);
        if (allThreads.find(threadId) == allThreads.end()) {
            allThreads[threadId] = std::make_unique<EuThread>(threadId);
        }
        allThreads[threadId]->stopThread(vmHandle);
        allThreads[threadId]->reportAsStopped();
    }

    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) override {
        readSystemRoutineIdentCallCount++;
        srIdent.count = 0;
        if (stoppedThreads.size()) {
            auto entry = stoppedThreads.find(thread->getThreadId());
            if (entry != stoppedThreads.end()) {
                srIdent.count = entry->second;
            }
            return true;
        }
        return L0::DebugSessionImp::readSystemRoutineIdent(thread, vmHandle, srIdent);
    }

    bool readSystemRoutineIdentFromMemory(EuThread *thread, const void *stateSaveArea, SIP::sr_ident &srIdent) override {
        readSystemRoutineIdentFromMemoryCallCount++;
        srIdent.count = 0;
        if (stoppedThreads.size()) {
            auto entry = stoppedThreads.find(thread->getThreadId());
            if (entry != stoppedThreads.end()) {
                srIdent.count = entry->second;
            }
            return true;
        }
        return L0::DebugSessionImp::readSystemRoutineIdentFromMemory(thread, stateSaveArea, srIdent);
    }

    bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) override {
        writeResumeCommandCalled++;
        if (skipWriteResumeCommand) {
            return true;
        }
        return L0::DebugSessionLinuxi915::writeResumeCommand(threadIds);
    }

    bool checkThreadIsResumed(const EuThread::ThreadId &threadID) override {
        checkThreadIsResumedCalled++;
        if (skipcheckThreadIsResumed) {
            return true;
        }
        return L0::DebugSessionLinuxi915::checkThreadIsResumed(threadID);
    }

    bool checkForceExceptionBit(uint64_t memoryHandle, EuThread::ThreadId threadId, uint32_t *cr0, const SIP::regset_desc *regDesc) override {
        if (skipCheckForceExceptionBit) {
            return false;
        }
        return L0::DebugSessionLinuxi915::checkForceExceptionBit(memoryHandle, threadId, cr0, regDesc);
    }

    float getThreadStartLimitTime() override {
        if (threadStartLimit >= 0.0) {
            return threadStartLimit;
        }
        return L0::DebugSessionLinuxi915::getThreadStartLimitTime();
    }

    void startInternalEventsThread() override {
        if (synchronousInternalEventRead) {
            internalThreadHasStarted = true;
            return;
        }
        if (failInternalEventsThreadStart) {
            return;
        }
        return DebugSessionLinuxi915::startInternalEventsThread();
    }

    std::unique_ptr<uint64_t[]> getInternalEvent() override {
        getInternalEventCounter++;
        if (synchronousInternalEventRead) {
            readInternalEventsAsync();
        }
        return DebugSessionLinuxi915::getInternalEvent();
    }

    void processPendingVmBindEvents() override {
        processPendingVmBindEventsCalled++;
        return DebugSessionLinuxi915::processPendingVmBindEvents();
    }

    void checkStoppedThreadsAndGenerateEvents(const std::vector<EuThread::ThreadId> &threads, uint64_t memoryHandle, uint32_t deviceIndex) override {
        checkStoppedThreadsAndGenerateEventsCallCount++;
        return DebugSessionLinuxi915::checkStoppedThreadsAndGenerateEvents(threads, memoryHandle, deviceIndex);
    }

    void addThreadToNewlyStoppedFromRaisedAttention(EuThread::ThreadId threadId, uint64_t memoryHandle, const void *stateSaveArea) override {
        addThreadToNewlyStoppedFromRaisedAttentionCallCount++;
        return DebugSessionImp::addThreadToNewlyStoppedFromRaisedAttention(threadId, memoryHandle, stateSaveArea);
    }

    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override {
        cleanRootSessionAfterDetachCallCount++;
        return DebugSessionLinuxi915::cleanRootSessionAfterDetach(deviceIndex);
    }

    TileDebugSessionLinuxi915 *createTileSession(const zet_debug_config_t &config, L0::Device *device, L0::DebugSessionImp *rootDebugSession) override;

    ze_result_t initializeRetVal = ZE_RESULT_FORCE_UINT32;
    bool allThreadsStopped = false;
    int64_t returnTimeDiff = -1;
    static constexpr uint64_t mockClientHandle = 1;
    size_t numThreadsPassedToThreadControl = 0;
    uint32_t readRegistersCallCount = 0;
    uint32_t writeRegistersCallCount = 0;
    uint32_t writeRegistersReg = 0;
    uint32_t handleEventCalledCount = 0;
    uint64_t vmHandle = UINT64_MAX;
    bool skipWriteResumeCommand = true;
    uint32_t writeResumeCommandCalled = 0;
    bool skipcheckThreadIsResumed = true;
    uint32_t checkThreadIsResumedCalled = 0;
    bool skipCheckForceExceptionBit = false;
    uint32_t interruptedDevice = std::numeric_limits<uint32_t>::max();
    uint32_t processPendingVmBindEventsCalled = 0;
    uint32_t checkStoppedThreadsAndGenerateEventsCallCount = 0;
    uint32_t addThreadToNewlyStoppedFromRaisedAttentionCallCount = 0;
    uint32_t readSystemRoutineIdentCallCount = 0;
    uint32_t readSystemRoutineIdentFromMemoryCallCount = 0;
    uint32_t cleanRootSessionAfterDetachCallCount = 0;

    std::vector<uint32_t> resumedDevices;
    std::vector<std::vector<EuThread::ThreadId>> resumedThreads;

    std::unordered_map<uint64_t, uint8_t> stoppedThreads;

    std::atomic<int> getInternalEventCounter = 0;
    bool synchronousInternalEventRead = false;
    bool failInternalEventsThreadStart = false;
    float threadStartLimit = -1.0;
};

struct MockAsyncThreadDebugSessionLinuxi915 : public MockDebugSessionLinuxi915 {
    using MockDebugSessionLinuxi915::MockDebugSessionLinuxi915;
    static void *mockAsyncThreadFunction(void *arg) {
        DebugSessionLinuxi915::asyncThreadFunction(arg);
        reinterpret_cast<MockAsyncThreadDebugSessionLinuxi915 *>(arg)->asyncThreadFinished = true;
        return nullptr;
    }

    void startAsyncThread() override {
        asyncThread.thread = NEO::Thread::create(mockAsyncThreadFunction, reinterpret_cast<void *>(this));
    }

    std::atomic<bool> asyncThreadFinished{false};
};

struct MockTileDebugSessionLinuxi915 : TileDebugSessionLinuxi915 {
    using DebugSession::allThreads;
    using DebugSessionImp::apiEvents;
    using DebugSessionImp::checkTriggerEventsForAttention;
    using DebugSessionImp::expectedAttentionEvents;
    using DebugSessionImp::interruptImp;
    using DebugSessionImp::newlyStoppedThreads;
    using DebugSessionImp::resumeImp;
    using DebugSessionImp::sendInterrupts;
    using DebugSessionImp::sipSupportsSlm;
    using DebugSessionImp::stateSaveAreaHeader;
    using DebugSessionImp::triggerEvents;
    using DebugSessionLinuxi915::detached;
    using DebugSessionLinuxi915::ioctl;
    using DebugSessionLinuxi915::pushApiEvent;
    using TileDebugSessionLinuxi915::cleanRootSessionAfterDetach;
    using TileDebugSessionLinuxi915::getAllMemoryHandles;
    using TileDebugSessionLinuxi915::getContextStateSaveAreaGpuVa;
    using TileDebugSessionLinuxi915::getContextStateSaveAreaSize;
    using TileDebugSessionLinuxi915::getSbaBufferGpuVa;
    using TileDebugSessionLinuxi915::isAttached;
    using TileDebugSessionLinuxi915::modules;
    using TileDebugSessionLinuxi915::processEntryState;
    using TileDebugSessionLinuxi915::readGpuMemory;
    using TileDebugSessionLinuxi915::readModuleDebugArea;
    using TileDebugSessionLinuxi915::readSbaBuffer;
    using TileDebugSessionLinuxi915::readStateSaveAreaHeader;
    using TileDebugSessionLinuxi915::startAsyncThread;
    using TileDebugSessionLinuxi915::TileDebugSessionLinuxi915;
    using TileDebugSessionLinuxi915::writeGpuMemory;

    void ensureThreadStopped(ze_device_thread_t thread, uint64_t vmHandle) {
        auto threadId = convertToThreadId(thread);
        if (allThreads.find(threadId) == allThreads.end()) {
            allThreads[threadId] = std::make_unique<EuThread>(threadId);
        }
        allThreads[threadId]->stopThread(vmHandle);
        allThreads[threadId]->reportAsStopped();
    }

    bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) override {
        if (writeResumeResult != -1) {
            return writeResumeResult == 0 ? false : true;
        }
        return writeResumeCommand(threadIds);
    }

    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) override {
        srIdent.count = 0;
        if (stoppedThreads.size()) {
            auto entry = stoppedThreads.find(thread->getThreadId());
            if (entry != stoppedThreads.end()) {
                srIdent.count = entry->second;
            }
            return true;
        }
        return L0::DebugSessionLinuxi915::readSystemRoutineIdent(thread, vmHandle, srIdent);
    }

    int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) override {
        if (returnTimeDiff != -1) {
            return returnTimeDiff;
        }
        return L0::DebugSessionLinuxi915::getTimeDifferenceMilliseconds(time);
    }

    int writeResumeResult = -1;
    int64_t returnTimeDiff = -1;
    std::unordered_map<uint64_t, uint8_t> stoppedThreads;
};

struct DebugApiLinuxFixture : public DeviceFixture {
    void setUp() {
        setUp(nullptr);
    }

    void setUp(NEO::HardwareInfo *hwInfo);

    void tearDown() {
        DeviceFixture::tearDown();
    }
    DrmQueryMock *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct DebugApiPageFaultEventFixture : public DebugApiLinuxFixture {
    void setUp() {
        DebugApiLinuxFixture::setUp();
        zet_debug_config_t config = {};
        config.pid = 0x1234;

        sessionMock = std::make_unique<MockDebugSessionLinuxi915>(config, device, 10);
        ASSERT_NE(nullptr, sessionMock);
        sessionMock->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

        auto handler = new MockIoctlHandler;
        sessionMock->ioctlHandler.reset(handler);
        SIP::version version = {2, 0, 0};
        initStateSaveArea(sessionMock->stateSaveAreaHeader, version, device);
        handler->setPreadMemory(sessionMock->stateSaveAreaHeader.data(), sessionMock->stateSaveAreaHeader.size(), 0x1000);
        sessionMock->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
        sessionMock->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;
        DebugSessionLinuxi915::BindInfo cssaInfo = {0x1000, sessionMock->stateSaveAreaHeader.size()};
        sessionMock->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = cssaInfo;
    }

    void tearDown() {
        DebugApiLinuxFixture::tearDown();
    }

    void buildPfi915Event() {
        buildPfi915Event(MockDebugSessionLinuxi915::mockClientHandle);
    }
    void buildPfi915Event(uint64_t clientHandle) {
        prelim_drm_i915_debug_event_page_fault pf = {};
        pf.base.type = PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT;
        pf.base.flags = 0;
        pf.base.size = sizeof(prelim_drm_i915_debug_event_page_fault) + (bitmaskSize * 3u);
        pf.client_handle = clientHandle;
        pf.lrc_handle = lrcHandle;
        pf.flags = 0;
        pf.ci.engine_class = 0;
        pf.ci.engine_instance = 0;
        pf.bitmask_size = static_cast<uint32_t>(bitmaskSize * 3);

        memcpy(data, &pf, sizeof(prelim_drm_i915_debug_event_page_fault));
        memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask)), bitmaskBefore.get(), bitmaskSize);
        memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask) + bitmaskSize), bitmaskAfter.get(), bitmaskSize);
        memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask) + (2 * bitmaskSize)), bitmaskResolved.get(), bitmaskSize);
    }

    size_t bitmaskSize = 256;
    uint8_t data[sizeof(prelim_drm_i915_debug_event_page_fault) + (256 * 3)];
    std::unique_ptr<uint8_t[]> bitmaskBefore, bitmaskAfter, bitmaskResolved;
    std::unique_ptr<MockDebugSessionLinuxi915> sessionMock;
    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;
};

struct DebugApiLinuxMultiDeviceFixture : public MultipleDevicesWithCustomHwInfo {
    void setUp();

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    NEO::Device *neoDevice = nullptr;
    L0::DeviceImp *deviceImp = nullptr;
    DrmQueryMock *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct MockDebugSessionLinuxi915Helper {

    void setupSessionClassHandlesAndUuidMap(MockDebugSessionLinuxi915 *session) {
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->classHandleToIndex[sbaClassHandle] = {"SBA AREA", static_cast<uint32_t>(NEO::DrmResourceClass::SbaTrackingBuffer)};
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->classHandleToIndex[moduleDebugClassHandle] = {"DEBUG AREA", static_cast<uint32_t>(NEO::DrmResourceClass::ModuleHeapDebugArea)};
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->classHandleToIndex[contextSaveClassHandle] = {"CONTEXT SAVE AREA", static_cast<uint32_t>(NEO::DrmResourceClass::ContextSaveArea)};
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->classHandleToIndex[isaClassHandle] = {"ISA", static_cast<uint32_t>(NEO::DrmResourceClass::Isa)};
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->classHandleToIndex[elfClassHandle] = {"ELF", static_cast<uint32_t>(NEO::DrmResourceClass::Elf)};

        DebugSessionLinuxi915::UuidData isaUuidData = {
            .handle = isaUUID,
            .classHandle = isaClassHandle,
            .classIndex = NEO::DrmResourceClass::Isa,
            .data = std::make_unique<char[]>(4),
            .dataSize = 4};
        DeviceBitfield bitfield;
        auto deviceBitfield = static_cast<uint32_t>(bitfield.to_ulong());
        memcpy(isaUuidData.data.get(), &deviceBitfield, sizeof(deviceBitfield));

        DebugSessionLinuxi915::UuidData elfUuidData = {
            .handle = elfUUID,
            .classHandle = elfClassHandle,
            .classIndex = NEO::DrmResourceClass::Elf,
            .data = std::make_unique<char[]>(elfSize),
            .dataSize = elfSize};
        memcpy(elfUuidData.data.get(), "ELF", sizeof("ELF"));

        DebugSessionLinuxi915::UuidData cookieUuidData = {
            .handle = cookieUUID,
            .classHandle = isaUUID,
            .classIndex = NEO::DrmResourceClass::MaxSize,
        };
        DebugSessionLinuxi915::UuidData sbaUuidData = {
            .handle = sbaUUID,
            .classHandle = sbaClassHandle,
            .classIndex = NEO::DrmResourceClass::SbaTrackingBuffer,
            .data = nullptr,
            .dataSize = 0};

        DebugSessionLinuxi915::UuidData debugAreaUuidData = {
            .handle = debugAreaUUID,
            .classHandle = moduleDebugClassHandle,
            .classIndex = NEO::DrmResourceClass::ModuleHeapDebugArea,
            .data = nullptr,
            .dataSize = 0};

        DebugSessionLinuxi915::UuidData stateSaveUuidData = {
            .handle = stateSaveUUID,
            .classHandle = contextSaveClassHandle,
            .classIndex = NEO::DrmResourceClass::ContextSaveArea,
            .data = nullptr,
            .dataSize = 0};

        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(isaUUID, std::move(isaUuidData));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(elfUUID, std::move(elfUuidData));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(cookieUUID, std::move(cookieUuidData));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(sbaUUID, std::move(sbaUuidData));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(debugAreaUUID, std::move(debugAreaUuidData));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(stateSaveUUID, std::move(stateSaveUuidData));

        DebugSessionLinuxi915::UuidData zebinModuleUuidData = {
            .handle = zebinModuleUUID,
            .classHandle = zebinModuleClassHandle,
            .classIndex = NEO::DrmResourceClass::L0ZebinModule,
            .data = std::make_unique<char[]>(sizeof(kernelCount)),
            .dataSize = sizeof(kernelCount)};

        memcpy(zebinModuleUuidData.data.get(), &kernelCount, sizeof(kernelCount));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->classHandleToIndex[zebinModuleClassHandle] = {"L0_ZEBIN_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(zebinModuleUUID, std::move(zebinModuleUuidData));
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount = 2;
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].ptr = elfVa;
    }

    void setupVmToTile(MockDebugSessionLinuxi915 *session) {
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vm0] = 0;
        session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vm1] = 1;
    }

    void addIsaVmBindEvent(MockDebugSessionLinuxi915 *session, uint64_t vm, bool ack, bool create) {
        uint64_t vmBindIsaData[(sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID) + sizeof(uint64_t)) / sizeof(uint64_t)];
        prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

        vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
        if (create) {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
        } else {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        }

        if (ack) {
            vmBindIsa->base.flags |= PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
        }
        vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
        vmBindIsa->base.seqno = 20u;
        vmBindIsa->client_handle = MockDebugSessionLinuxi915::mockClientHandle;
        vmBindIsa->va_start = isaGpuVa;
        vmBindIsa->va_length = isaSize;
        vmBindIsa->vm_handle = vm;
        vmBindIsa->num_uuids = 3;

        auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

        typeOfUUID uuidsTemp[3];
        uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
        uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
        uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

        memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

        session->handleEvent(&vmBindIsa->base);
    }

    void addZebinVmBindEvent(MockDebugSessionLinuxi915 *session, uint64_t vm, bool ack, bool create, uint64_t kernelIndex, uint32_t zebinModuleId) {
        uint64_t vmBindIsaData[(sizeof(prelim_drm_i915_debug_event_vm_bind) + 4 * sizeof(typeOfUUID) + sizeof(uint64_t)) / sizeof(uint64_t)];
        prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

        vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
        if (create) {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
        } else {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        }

        if (ack) {
            vmBindIsa->base.flags |= PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
        }

        vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 4 * sizeof(typeOfUUID);
        vmBindIsa->base.seqno = 10;
        vmBindIsa->client_handle = MockDebugSessionLinuxi915::mockClientHandle;
        if (zebinModuleId == 0) {
            vmBindIsa->va_start = kernelIndex == 0 ? isaGpuVa : isaGpuVa + isaSize;
        } else {
            vmBindIsa->va_start = kernelIndex == 0 ? isaGpuVa * 4 : isaGpuVa * 4 + isaSize;

            DebugSessionLinuxi915::UuidData zebinModuleUuidData = {
                .handle = zebinModuleUUID1,
                .classHandle = zebinModuleClassHandle,
                .classIndex = NEO::DrmResourceClass::L0ZebinModule,
                .data = std::make_unique<char[]>(sizeof(kernelCount)),
                .dataSize = sizeof(kernelCount)};

            session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.emplace(zebinModuleUUID1, std::move(zebinModuleUuidData));
            session->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID1].segmentCount = kernelCount;
        }
        vmBindIsa->va_length = isaSize;
        vmBindIsa->vm_handle = vm;
        vmBindIsa->num_uuids = 4;
        auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
        typeOfUUID uuidsTemp[4];
        uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
        uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
        uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
        uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleId == 0 ? zebinModuleUUID : zebinModuleUUID1);

        memcpy_s(uuids, 4 * sizeof(typeOfUUID), uuidsTemp, sizeof(uuidsTemp));
        session->handleEvent(&vmBindIsa->base);
    }

    void addZebinVmBindEvent(MockDebugSessionLinuxi915 *session, uint64_t vm, bool ack, bool create, uint64_t kernelIndex) {
        return addZebinVmBindEvent(session, vm, ack, create, kernelIndex, 0);
    }

    const uint64_t sbaClassHandle = 1;
    const uint64_t moduleDebugClassHandle = 2;
    const uint64_t contextSaveClassHandle = 3;
    const uint64_t isaClassHandle = 4;
    const uint64_t elfClassHandle = 5;

    const uint64_t isaUUID = 2;
    const uint64_t elfUUID = 3;
    const uint64_t cookieUUID = 5;
    const uint64_t sbaUUID = 6;
    const uint64_t debugAreaUUID = 7;
    const uint64_t stateSaveUUID = 8;

    const uint64_t zebinModuleClassHandle = 101;
    const uint64_t zebinModuleUUID = 9;
    const uint64_t zebinModuleUUID1 = 10;
    const uint32_t kernelCount = 2;

    const uint64_t vm0 = 1;
    const uint64_t vm1 = 2;

    const uint64_t isaGpuVa = 0x345000;
    const uint64_t isaSize = 0x2000;

    const uint64_t elfVa = 0x1000;
    const uint32_t elfSize = 10;
};

} // namespace ult
} // namespace L0
