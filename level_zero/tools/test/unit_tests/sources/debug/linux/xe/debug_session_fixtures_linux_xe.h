/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/debug_mock_drm_xe.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/linux/xe/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"

#include "common/StateSaveAreaHeader.h"

#include <atomic>
#include <queue>
#include <type_traits>
#include <unordered_map>

namespace L0 {
namespace ult {
using typeOfLrcHandle = std::decay<decltype(NEO::EuDebugEventExecQueue::lrcHandle[0])>::type;

struct DebugApiLinuxMultiDeviceFixtureXe : public MultipleDevicesWithCustomHwInfo {
    void setUp();

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    NEO::Device *neoDevice = nullptr;
    L0::DeviceImp *deviceImp = nullptr;
    DrmMockXeDebug *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct DebugApiLinuxXeFixture : public DeviceFixture {
    void setUp() {
        setUp(nullptr);
    }

    void setUp(NEO::HardwareInfo *hwInfo);

    void tearDown() {
        DeviceFixture::tearDown();
    }
    DrmMockXeDebug *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct MockIoctlHandlerXe : public L0::ult::MockIoctlHandler {

    int ioctl(int fd, unsigned long request, void *arg) override {
        ioctlCalled++;

        if ((request == static_cast<uint64_t>(NEO::EuDebugParam::ioctlReadEvent)) && (arg != nullptr)) {
            auto debugEvent = reinterpret_cast<NEO::EuDebugEvent *>(arg);
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
        } else if ((request == static_cast<uint64_t>(NEO::EuDebugParam::ioctlVmOpen)) && (arg != nullptr)) {
            NEO::EuDebugVmOpen *vmOpenIn = reinterpret_cast<NEO::EuDebugVmOpen *>(arg);
            vmOpen = *vmOpenIn;
            return vmOpenRetVal;
        } else if ((request == static_cast<uint64_t>(NEO::EuDebugParam::ioctlEuControl)) && (arg != nullptr)) {
            NEO::EuDebugEuControl *euControlArg = reinterpret_cast<NEO::EuDebugEuControl *>(arg);
            EuControlArg arg;
            arg.euControl = *euControlArg;

            euControlArg->seqno = ++euControlOutputSeqno;

            if (euControlArg->bitmaskSize != 0) {
                arg.euControlBitmaskSize = euControlArg->bitmaskSize;
                arg.euControlBitmask = std::make_unique<uint8_t[]>(arg.euControlBitmaskSize);

                memcpy(arg.euControlBitmask.get(), reinterpret_cast<void *>(euControlArg->bitmaskPtr), arg.euControlBitmaskSize);
                if (euControlArg->cmd == static_cast<uint64_t>(NEO::EuDebugParam::euControlCmdStopped) && euControlArg->bitmaskPtr && outputBitmask.get()) {
                    memcpy_s(reinterpret_cast<uint64_t *>(euControlArg->bitmaskPtr), euControlArg->bitmaskSize, outputBitmask.get(), outputBitmaskSize);
                }
            }
            euControlArgs.push_back(std::move(arg));
        } else if ((request == static_cast<uint64_t>(NEO::EuDebugParam::ioctlReadMetadata)) && (arg != nullptr)) {
            NEO::EuDebugReadMetadata *readMetadata = reinterpret_cast<NEO::EuDebugReadMetadata *>(arg);
            if (returnMetadata) {
                readMetadata->clientHandle = returnMetadata->clientHandle;
                readMetadata->metadataHandle = returnMetadata->metadataHandle;
                readMetadata->flags = returnMetadata->flags;
                int returnError = 0;
                if (readMetadata->size >= returnMetadata->size) {
                    memcpy(reinterpret_cast<void *>(readMetadata->ptr), reinterpret_cast<void *>(returnMetadata->ptr), returnMetadata->size);
                } else {
                    returnError = -1;
                }
                returnMetadata = nullptr;
                return returnError;
            }
            return -1;
        }

        return ioctlRetVal;
    }

    int fsync(int fd) override {
        fsyncCalled++;
        if (numFsyncToSucceed > 0) {
            numFsyncToSucceed--;
            return 0;
        }
        return fsyncRetVal;
    }

    struct EuControlArg {
        EuControlArg() : euControlBitmask(nullptr) {
            memset(&euControl, 0, sizeof(euControl));
        }
        EuControlArg(EuControlArg &&in) noexcept : euControl(in.euControl), euControlBitmask(std::move(in.euControlBitmask)), euControlBitmaskSize(in.euControlBitmaskSize){};
        NEO::EuDebugEuControl euControl = {};
        std::unique_ptr<uint8_t[]> euControlBitmask;
        size_t euControlBitmaskSize = 0;
    };

    NEO::EuDebugEvent debugEventInput = {};
    NEO::EuDebugVmOpen vmOpen = {};
    NEO::EuDebugReadMetadata *returnMetadata = nullptr;

    int ioctlRetVal = 0;
    int debugEventRetVal = 0;
    int ioctlCalled = 0;
    int fsyncCalled = 0;
    int fsyncRetVal = 0;
    int numFsyncToSucceed = 100;
    uint64_t euControlOutputSeqno = 0;
    std::vector<EuControlArg> euControlArgs;
    std::unique_ptr<uint8_t[]> outputBitmask;
    size_t outputBitmaskSize = 0;
    int vmOpenRetVal = 600;
};

struct MockDebugSessionLinuxXe : public L0::DebugSessionLinuxXe {
    using L0::DebugSessionImp::allThreads;
    using L0::DebugSessionImp::apiEvents;
    using L0::DebugSessionImp::expectedAttentionEvents;
    using L0::DebugSessionImp::fifoPollInterval;
    using L0::DebugSessionImp::interruptSent;
    using L0::DebugSessionImp::interruptTimeout;
    using L0::DebugSessionImp::readFifo;
    using L0::DebugSessionImp::stateSaveAreaHeader;
    using L0::DebugSessionImp::triggerEvents;
    using L0::DebugSessionLinux::getClientConnection;
    using L0::DebugSessionLinux::maxRetries;
    using L0::DebugSessionLinux::updateStoppedThreadsAndCheckTriggerEvents;
    using L0::DebugSessionLinuxXe::addThreadToNewlyStoppedFromRaisedAttentionForTileSession;
    using L0::DebugSessionLinuxXe::asyncThread;
    using L0::DebugSessionLinuxXe::asyncThreadFunction;
    using L0::DebugSessionLinuxXe::canHandleVmBind;
    using L0::DebugSessionLinuxXe::checkStoppedThreadsAndGenerateEvents;
    using L0::DebugSessionLinuxXe::checkTriggerEventsForAttentionForTileSession;
    using L0::DebugSessionLinuxXe::ClientConnectionXe;
    using L0::DebugSessionLinuxXe::clientHandleClosed;
    using L0::DebugSessionLinuxXe::clientHandleToConnection;
    using L0::DebugSessionLinuxXe::debugArea;
    using L0::DebugSessionLinuxXe::euControlInterruptSeqno;
    using L0::DebugSessionLinuxXe::eventTypeIsAttention;
    using L0::DebugSessionLinuxXe::getEuControlCmdUnlock;
    using L0::DebugSessionLinuxXe::getThreadStateMutexForTileSession;
    using L0::DebugSessionLinuxXe::getVmHandleFromClientAndlrcHandle;
    using L0::DebugSessionLinuxXe::handleEvent;
    using L0::DebugSessionLinuxXe::handleInternalEvent;
    using L0::DebugSessionLinuxXe::handleVmBind;
    using L0::DebugSessionLinuxXe::internalEventQueue;
    using L0::DebugSessionLinuxXe::internalEventThread;
    using L0::DebugSessionLinuxXe::invalidClientHandle;
    using L0::DebugSessionLinuxXe::ioctlHandler;
    using L0::DebugSessionLinuxXe::newestAttSeqNo;
    using L0::DebugSessionLinuxXe::newlyStoppedThreads;
    using L0::DebugSessionLinuxXe::pendingInterrupts;
    using L0::DebugSessionLinuxXe::readEventImp;
    using L0::DebugSessionLinuxXe::readInternalEventsAsync;
    using L0::DebugSessionLinuxXe::startAsyncThread;
    using L0::DebugSessionLinuxXe::threadControl;
    using L0::DebugSessionLinuxXe::ThreadControlCmd;

    MockDebugSessionLinuxXe(const zet_debug_config_t &config, L0::Device *device, int debugFd, void *params) : DebugSessionLinuxXe(config, device, debugFd, params) {
        clientHandleToConnection[mockClientHandle].reset(new ClientConnectionXe);
        clientHandle = mockClientHandle;
        createEuThreads();
    }
    MockDebugSessionLinuxXe(const zet_debug_config_t &config, L0::Device *device, int debugFd) : MockDebugSessionLinuxXe(config, device, debugFd, nullptr) {}

    void addThreadToNewlyStoppedFromRaisedAttentionForTileSession(EuThread::ThreadId threadId,
                                                                  uint64_t memoryHandle,
                                                                  const void *stateSaveArea,
                                                                  uint32_t tileIndex) override {
        DebugSessionLinuxXe::addThreadToNewlyStoppedFromRaisedAttentionForTileSession(threadId, 0x12345678, nullptr, 0);
        countToAddThreadToNewlyStoppedFromRaisedAttentionForTileSession++;
    }

    ze_result_t initialize() override {
        if (initializeRetVal != ZE_RESULT_FORCE_UINT32) {
            createEuThreads();
            clientHandle = mockClientHandle;
            return initializeRetVal;
        }
        return DebugSessionLinuxXe::initialize();
    }

    void handleAttentionEvent(NEO::EuDebugEventEuAttention *attention) override {
        handleAttentionEventCalled++;
        return DebugSessionLinuxXe::handleAttentionEvent(attention);
    }

    void processPendingVmBindEvents() override {
        processPendingVmBindEventsCallCount++;
        return DebugSessionLinuxXe::processPendingVmBindEvents();
    }

    bool handleVmBind(VmBindData &vmBindData) override {
        handleVmBindCallCount++;
        return DebugSessionLinuxXe::handleVmBind(vmBindData);
    }

    int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) override {
        numThreadsPassedToThreadControl = threads.size();
        threadControlCallCount++;
        if (reachSteadyStateCount) {
            reachSteadyStateCount--;
            std::vector<EuThread::ThreadId> threads;
            uint32_t lastThread = threadControlCallCount;
            if (reachSteadyStateCount == 0) {
                lastThread = threadControlCallCount - 1;
            }

            for (uint32_t i = 0; i < lastThread; i++) {
                threads.push_back({0, 0, 0, 0, i});
            }

            std::unique_ptr<uint8_t[]> attBitmask;
            size_t attBitmaskSize = 0;
            auto &hwInfo = this->getConnectedDevice()->getNEODevice()->getHardwareInfo();
            auto &l0GfxCoreHelper = this->getConnectedDevice()->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

            l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, attBitmask, attBitmaskSize);
            bitmask = std::move(attBitmask);
            bitmaskSize = attBitmaskSize;
            return 0;
        }
        return L0::DebugSessionLinuxXe::threadControl(threads, tile, threadCmd, bitmask, bitmaskSize);
    }

    std::unique_ptr<uint64_t[]> getInternalEvent() override {
        getInternalEventCounter++;
        if (synchronousInternalEventRead) {
            readInternalEventsAsync();
        }
        return DebugSessionLinuxXe::getInternalEvent();
    }

    bool pushApiEventValidateAckEvents = false;
    bool pushApiEventAckEventsFound = false;
    void pushApiEvent(zet_debug_event_t &debugEvent, uint64_t moduleHandle) override {
        if (pushApiEventValidateAckEvents) {
            auto connection = clientHandleToConnection[clientHandle].get();
            for (auto module : connection->metaDataToModule) {
                if (module.second.ackEvents[0].size() > 0) {
                    pushApiEventAckEventsFound = true;
                    break;
                }
            }
        }
        return DebugSessionLinux::pushApiEvent(debugEvent, moduleHandle);
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

    void addThreadToNewlyStoppedFromRaisedAttention(EuThread::ThreadId threadId, uint64_t memoryHandle, const void *stateSaveArea) override {
        addThreadToNewlyStoppedFromRaisedAttentionCallCount++;
        return DebugSessionImp::addThreadToNewlyStoppedFromRaisedAttention(threadId, memoryHandle, stateSaveArea);
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

    void handlePageFaultEvent(PageFaultEvent &pfEvent) override {
        handlePageFaultEventCalled++;
    }

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        return 0x1000;
    }

    size_t getContextStateSaveAreaSize(uint64_t memoryHandle) override {
        return 0x1000;
    }

    uint32_t readSystemRoutineIdentCallCount = 0;
    uint32_t processPendingVmBindEventsCallCount = 0;
    uint32_t threadControlCallCount = 0;
    uint32_t handleVmBindCallCount = 0;
    uint32_t addThreadToNewlyStoppedFromRaisedAttentionCallCount = 0;
    uint32_t readSystemRoutineIdentFromMemoryCallCount = 0;
    size_t numThreadsPassedToThreadControl = 0;
    bool synchronousInternalEventRead = false;
    std::atomic<int> getInternalEventCounter = 0;
    ze_result_t initializeRetVal = ZE_RESULT_FORCE_UINT32;
    static constexpr uint64_t mockClientHandle = 1;
    std::unordered_map<uint64_t, uint8_t> stoppedThreads;
    uint32_t countToAddThreadToNewlyStoppedFromRaisedAttentionForTileSession = 0;
    int64_t returnTimeDiff = -1;
    uint32_t handleAttentionEventCalled = 0;
    uint32_t handlePageFaultEventCalled = 0;
    uint32_t reachSteadyStateCount = 0;
};

struct MockAsyncThreadDebugSessionLinuxXe : public MockDebugSessionLinuxXe {
    using MockDebugSessionLinuxXe::MockDebugSessionLinuxXe;
    static void *mockAsyncThreadFunction(void *arg) {
        DebugSessionLinuxXe::asyncThreadFunction(arg);
        reinterpret_cast<MockAsyncThreadDebugSessionLinuxXe *>(arg)->asyncThreadFinished = true;
        return nullptr;
    }

    void startAsyncThread() override {
        asyncThread.thread = NEO::Thread::createFunc(mockAsyncThreadFunction, reinterpret_cast<void *>(this));
    }

    std::atomic<bool> asyncThreadFinished{false};
};

} // namespace ult
} // namespace L0
