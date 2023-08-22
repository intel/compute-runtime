/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include "common/StateSaveAreaHeader.h"

namespace L0 {
namespace ult {

struct DebugSessionRegistersAccess {
    void setUp() {
        zet_debug_config_t config = {};
        config.pid = 0x1234;
        auto hwInfo = *NEO::defaultHwInfo.get();

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0);
        deviceImp = std::make_unique<Mock<L0::DeviceImp>>(neoDevice, neoDevice->getExecutionEnvironment());

        session = std::make_unique<MockDebugSession>(config, deviceImp.get());

        session->allThreads[stoppedThreadId]->stopThread(1u);
        session->allThreads[stoppedThreadId]->reportAsStopped();
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

} // namespace ult
} // namespace L0
