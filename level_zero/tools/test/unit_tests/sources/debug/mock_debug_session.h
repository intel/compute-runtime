/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {
namespace ult {

class OsInterfaceWithDebugAttach : public NEO::OSInterface {
  public:
    OsInterfaceWithDebugAttach() : OSInterface() {}
    bool isDebugAttachAvailable() const override {
        return debugAttachAvailable;
    }

    bool debugAttachAvailable = true;
};

struct DebugSessionMock : public L0::DebugSession {
    using L0::DebugSession::allThreads;
    using L0::DebugSession::debugArea;
    using L0::DebugSession::fillDevicesFromThread;
    using L0::DebugSession::getPerThreadScratchOffset;
    using L0::DebugSession::getSingleThreadsForDevice;
    using L0::DebugSession::isBindlessSystemRoutine;

    DebugSessionMock(const zet_debug_config_t &config, L0::Device *device) : DebugSession(config, device), config(config){};
    bool closeConnection() override { return true; }
    ze_result_t initialize() override {
        if (config.pid == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t interrupt(ze_device_thread_t thread) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t resume(ze_device_thread_t thread) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t readSbaBuffer(EuThread::ThreadId threadId, SbaTrackedAddresses &sbaBuffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    void startAsyncThread() override {
        asyncThreadStarted = true;
    }

    bool readModuleDebugArea() override {
        return true;
    }

    zet_debug_config_t config;
    bool asyncThreadStarted = false;
};

} // namespace ult
} // namespace L0
