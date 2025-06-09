/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_registers_access.h"

#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
namespace L0 {
namespace ult {
DebugSessionRegistersAccess::DebugSessionRegistersAccess() = default;
DebugSessionRegistersAccess::~DebugSessionRegistersAccess() = default;

void DebugSessionRegistersAccessV3::setUp() {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);
    deviceImp = std::make_unique<MockDeviceImp>(neoDevice);

    session = std::make_unique<MockDebugSession>(config, deviceImp.get(), true, 3);

    session->allThreads[stoppedThreadId]->stopThread(1u);
    session->allThreads[stoppedThreadId]->reportAsStopped();
}

void DebugSessionRegistersAccess::setUp() {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto hwInfo = *NEO::defaultHwInfo.get();

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);
    deviceImp = std::make_unique<MockDeviceImp>(neoDevice);

    session = std::make_unique<MockDebugSession>(config, deviceImp.get());

    session->allThreads[stoppedThreadId]->stopThread(1u);
    session->allThreads[stoppedThreadId]->reportAsStopped();
}

void DebugSessionRegistersAccess::tearDown() {
}

void DebugSessionRegistersAccess::dumpRegisterState() {
    if (session->stateSaveAreaHeader.size() == 0) {
        return;
    }
    auto pStateSaveAreaHeader = reinterpret_cast<NEO::StateSaveAreaHeader *>(session->stateSaveAreaHeader.data());

    if (pStateSaveAreaHeader->versionHeader.version.major >= 3) {
        for (uint32_t thread = 0; thread < pStateSaveAreaHeader->regHeaderV3.num_threads_per_eu; thread++) {
            EuThread::ThreadId threadId(0, 0, 0, 0, thread);
            auto threadSlotOffset = session->calculateThreadSlotOffset(threadId);

            auto srMagicOffset = threadSlotOffset + pStateSaveAreaHeader->regHeaderV3.sr_magic_offset;
            SIP::sr_ident srMagic;
            srMagic.count = 1;
            srMagic.version.major = pStateSaveAreaHeader->versionHeader.version.major;

            session->writeGpuMemory(0, reinterpret_cast<char *>(&srMagic), sizeof(srMagic), reinterpret_cast<uint64_t>(pStateSaveAreaHeader) + srMagicOffset);
        }
    } else {
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
}

} // namespace ult
} // namespace L0
