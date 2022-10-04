/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {
DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd);

namespace ult {

using CreateDebugSessionHelperFunc = decltype(&L0::createDebugSessionHelper);

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

    DebugSessionMock(const zet_debug_config_t &config, L0::Device *device) : DebugSession(config, device), config(config) {
        topologyMap = buildMockTopology(device);
    }

    bool closeConnection() override {
        closeConnectionCalled = true;
        return true;
    }

    static NEO::TopologyMap buildMockTopology(L0::Device *device) {
        NEO::TopologyMap topologyMap;
        auto &hwInfo = device->getHwInfo();
        uint32_t tileCount;
        if (hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid) {
            tileCount = hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount;
        } else {
            tileCount = 1;
        }

        for (uint32_t deviceIdx = 0; deviceIdx < tileCount; deviceIdx++) {
            NEO::TopologyMapping mapping;
            for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SliceCount; i++) {
                mapping.sliceIndices.push_back(static_cast<int>(i));
            }
            if (hwInfo.gtSystemInfo.SliceCount == 1) {
                for (uint32_t i = 0; i < hwInfo.gtSystemInfo.SubSliceCount; i++) {
                    mapping.subsliceIndices.push_back(static_cast<int>(i));
                }
            }
            topologyMap[deviceIdx] = mapping;
        }
        return topologyMap;
    }

    ze_result_t initialize() override {
        if (config.pid == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        createEuThreads();
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
    ze_result_t readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    void startAsyncThread() override {
        asyncThreadStarted = true;
    }

    bool readModuleDebugArea() override {
        return true;
    }

    void detachTileDebugSession(DebugSession *tileSession) override {}
    bool areAllTileDebugSessionDetached() override { return true; }

    L0::DebugSession *attachTileDebugSession(L0::Device *device) override {
        return nullptr;
    }

    void setAttachMode(bool isRootAttach) override {}

    const NEO::TopologyMap &getTopologyMap() override {
        return this->topologyMap;
    }

    NEO::TopologyMap topologyMap;
    zet_debug_config_t config;
    bool asyncThreadStarted = false;
    bool closeConnectionCalled = false;
};

} // namespace ult
} // namespace L0
