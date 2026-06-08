/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/windows/debug_session_fixtures_windows.h"

#include "shared/test/common/mocks/windows/mock_wddm_eudebug.h"

#include "level_zero/tools/source/debug/eu_thread.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

MockDebugSessionWindows::MockDebugSessionWindows(const zet_debug_config_t &config, L0::Device *device)
    : DebugSessionWindows(config, device) {}

MockDebugSessionWindows::~MockDebugSessionWindows() {
    closeAsyncThread();
}

ze_result_t MockDebugSessionWindows::initialize() {
    if (resultInitialize != ZE_RESULT_FORCE_UINT32) {
        return resultInitialize;
    }
    return DebugSessionWindows::initialize();
}

ze_result_t MockDebugSessionWindows::readAndHandleEvent(uint64_t timeoutMs) {
    if (resultReadAndHandleEvent != ZE_RESULT_FORCE_UINT32) {
        return resultReadAndHandleEvent;
    }
    return DebugSessionWindows::readAndHandleEvent(timeoutMs);
}

NTSTATUS MockDebugSessionWindows::runEscape(KM_ESCAPE_INFO &escapeInfo) {
    if (shouldEscapeReturnStatusNotSuccess) {
        escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus = DBGUMD_RETURN_DEBUGGER_ATTACH_DEVICE_BUSY;
        return STATUS_SUCCESS;
    }
    if (shouldEscapeCallFail) {
        return STATUS_WAIT_1;
    }

    return L0::DebugSessionWindows::runEscape(escapeInfo);
}

void MockDebugSessionWindows::ensureThreadStopped(ze_device_thread_t thread, uint64_t context) {
    auto threadId = convertToThreadId(thread);
    if (allThreads.find(threadId) == allThreads.end()) {
        allThreads[threadId] = std::make_unique<EuThread>(threadId);
    }
    allThreads[threadId]->stopThread(context);
    allThreads[threadId]->reportAsStopped();
}

bool MockDebugSessionWindows::readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) {
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

bool MockDebugSessionWindows::readSystemRoutineIdentFromMemory(EuThread *thread, const void *stateSaveArea, SIP::sr_ident &srIdent) {
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

void *MockAsyncThreadDebugSessionWindows::mockAsyncThreadFunction(void *arg) {
    DebugSessionWindows::asyncThreadFunction(arg);
    reinterpret_cast<MockAsyncThreadDebugSessionWindows *>(arg)->asyncThreadFinished = true;
    return nullptr;
}

void MockAsyncThreadDebugSessionWindows::startAsyncThread() {
    asyncThread.thread = NEO::Thread::createFunc(mockAsyncThreadFunction, reinterpret_cast<void *>(this));
}

void DebugApiWindowsFixture::setUp() {
    DeviceFixture::setUp();
    mockWddm = new WddmEuDebugInterfaceMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));
}

void DebugApiWindowsFixture::tearDown() {
    DeviceFixture::tearDown();
}

void DebugApiWindowsFixture::copyBitmaskToEventParams(READ_EVENT_PARAMS_BUFFER *params, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) {
    auto bitsetParams = reinterpret_cast<DBGUMD_READ_EVENT_EU_ATTN_BIT_SET_PARAMS *>(params);
    auto bitmapDst = reinterpret_cast<uint8_t *>(&bitsetParams->BitmaskArrayPtr);
    bitsetParams->BitMaskSizeInBytes = static_cast<uint32_t>(bitmaskSize);
    memcpy_s(bitmapDst, bitmaskSize, bitmask.get(), bitmaskSize);
}

} // namespace ult
} // namespace L0
