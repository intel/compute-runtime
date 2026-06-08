/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip.h"
#include "shared/source/os_interface/windows/wddm_debug.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/debug/windows/debug_session.h"

#include <atomic>
#include <unordered_map>

namespace NEO {
struct WddmEuDebugInterfaceMock;
}

namespace L0 {
class EuThread;

namespace ult {

struct MockDebugSessionWindows : DebugSessionWindows {
    using DebugSessionWindows::acknowledgeEventImp;
    using DebugSessionWindows::allContexts;
    using DebugSessionWindows::allElfs;
    using DebugSessionWindows::allModules;
    using DebugSessionWindows::allThreads;
    using DebugSessionWindows::asyncThread;
    using DebugSessionWindows::attentionEventContext;
    using DebugSessionWindows::calculateThreadSlotOffset;
    using DebugSessionWindows::closeAsyncThread;
    using DebugSessionWindows::continueExecutionImp;
    using DebugSessionWindows::debugArea;
    using DebugSessionWindows::debugAreaVA;
    using DebugSessionWindows::debugHandle;
    using DebugSessionWindows::ElfRange;
    using DebugSessionWindows::eventsToAck;
    using DebugSessionWindows::getSbaBufferGpuVa;
    using DebugSessionWindows::initialize;
    using DebugSessionWindows::interruptContextImp;
    using DebugSessionWindows::interruptImp;
    using DebugSessionWindows::invalidHandle;
    using DebugSessionWindows::moduleDebugAreaCaptured;
    using DebugSessionWindows::processId;
    using DebugSessionWindows::pushApiEvent;
    using DebugSessionWindows::readAllocationDebugData;
    using DebugSessionWindows::readAndHandleEvent;
    using DebugSessionWindows::readGpuMemory;
    using DebugSessionWindows::readModuleDebugArea;
    using DebugSessionWindows::readSbaBuffer;
    using DebugSessionWindows::readStateSaveAreaHeader;
    using DebugSessionWindows::resumeContextImp;
    using DebugSessionWindows::resumeImp;
    using DebugSessionWindows::runEscape;
    using DebugSessionWindows::startAsyncThread;
    using DebugSessionWindows::stateSaveAreaCaptured;
    using DebugSessionWindows::stateSaveAreaSize;
    using DebugSessionWindows::stateSaveAreaVA;
    using DebugSessionWindows::wddm;
    using DebugSessionWindows::writeGpuMemory;
    using L0::DebugSessionImp::apiEvents;
    using L0::DebugSessionImp::attachTile;
    using L0::DebugSessionImp::cleanRootSessionAfterDetach;
    using L0::DebugSessionImp::detachTile;
    using L0::DebugSessionImp::getStateSaveAreaHeader;
    using L0::DebugSessionImp::interruptSent;
    using L0::DebugSessionImp::isValidGpuAddress;
    using L0::DebugSessionImp::sipSupportsSlm;
    using L0::DebugSessionImp::stateSaveAreaHeader;
    using L0::DebugSessionImp::triggerEvents;
    using L0::DebugSessionWindows::newlyStoppedThreads;
    using L0::DebugSessionWindows::pendingInterrupts;

    MockDebugSessionWindows(const zet_debug_config_t &config, L0::Device *device);
    ~MockDebugSessionWindows() override;

    ze_result_t initialize() override;
    ze_result_t readAndHandleEvent(uint64_t timeoutMs) override;
    NTSTATUS runEscape(KM_ESCAPE_INFO &escapeInfo) override;
    void ensureThreadStopped(ze_device_thread_t thread, uint64_t context);
    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) override;
    bool readSystemRoutineIdentFromMemory(EuThread *thread, const void *stateSaveArea, SIP::sr_ident &srIdent) override;

    ze_result_t resultInitialize = ZE_RESULT_FORCE_UINT32;
    ze_result_t resultReadAndHandleEvent = ZE_RESULT_FORCE_UINT32;
    static constexpr uint64_t mockDebugHandle = 1;
    bool shouldEscapeReturnStatusNotSuccess = false;
    bool shouldEscapeCallFail = false;
    std::unordered_map<uint64_t, uint8_t> stoppedThreads;

    uint32_t readSystemRoutineIdentFromMemoryCallCount = 0;
};

struct MockAsyncThreadDebugSessionWindows : public MockDebugSessionWindows {
    using MockDebugSessionWindows::MockDebugSessionWindows;

    static void *mockAsyncThreadFunction(void *arg);
    void startAsyncThread() override;

    std::atomic<bool> asyncThreadFinished{false};
};

struct DebugApiWindowsFixture : public DeviceFixture {
    void setUp();
    void tearDown();
    void copyBitmaskToEventParams(READ_EVENT_PARAMS_BUFFER *params, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize);

    static constexpr uint8_t bufferSize = 16;
    NEO::WddmEuDebugInterfaceMock *mockWddm = nullptr;
};

} // namespace ult
} // namespace L0
