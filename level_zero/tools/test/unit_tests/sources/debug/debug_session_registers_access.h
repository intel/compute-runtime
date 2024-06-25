/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/debug/eu_thread.h"

namespace NEO {
class MockDevice;
}

namespace L0 {
namespace ult {
struct MockDeviceImp;
struct MockDebugSession;

struct DebugSessionRegistersAccess {
    DebugSessionRegistersAccess();
    ~DebugSessionRegistersAccess();
    virtual void setUp();

    void tearDown();

    void dumpRegisterState();

    ze_device_thread_t stoppedThread = {0, 0, 0, 0};
    EuThread::ThreadId stoppedThreadId{0, stoppedThread};
    std::unique_ptr<MockDebugSession> session;
    std::unique_ptr<MockDeviceImp> deviceImp;
    NEO::MockDevice *neoDevice = nullptr;
};

struct DebugSessionRegistersAccessV3 : public DebugSessionRegistersAccess {
    void setUp() override;
};

} // namespace ult
} // namespace L0
