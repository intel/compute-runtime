/*
 * Copyright (C) 2022-2026 Intel Corporation
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
    std::unique_ptr<MockDeviceImp> mockDevice;
    NEO::MockDevice *neoDevice = nullptr;
};

struct DebugSessionRegistersAccessV3 : public DebugSessionRegistersAccess {
    void setUp() override;
};

struct DebugSessionRegistersAccessScratch : public DebugSessionRegistersAccess {
    void setUp() override;
};

struct DebugSessionRegistersAccessScratchV3 : public DebugSessionRegistersAccessV3 {
    void setUp() override;
};

void setIsScratchInGrf(NEO::MockDevice *neoDevice, bool value);

} // namespace ult
} // namespace L0
