/*
 * Copyright (C) 2022-2024 Intel Corporation
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
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/linux/xe/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_mock_drm_xe.h"

#include "common/StateSaveAreaHeader.h"
#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

#include <atomic>
#include <queue>
#include <type_traits>
#include <unordered_map>

namespace L0 {
namespace ult {

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

struct MockIoctlHandlerXe : public L0::DebugSessionLinuxXe::IoctlHandlerXe {

    using EventPair = std::pair<char *, uint64_t>;
    using EventQueue = std::queue<EventPair>;

    int ioctl(int fd, unsigned long request, void *arg) override {
        ioctlCalled++;

        if ((request == DRM_XE_EUDEBUG_IOCTL_READ_EVENT) && (arg != nullptr)) {
            auto debugEvent = reinterpret_cast<drm_xe_eudebug_event *>(arg);
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

    drm_xe_eudebug_event debugEventInput = {};

    EventQueue eventQueue;
    int ioctlRetVal = 0;
    int debugEventRetVal = 0;
    int ioctlCalled = 0;
    std::atomic<int> pollCounter = 0;
    int pollRetVal = 0;
    int passedTimeout = 0;
};

struct MockDebugSessionLinuxXe : public L0::DebugSessionLinuxXe {
    using L0::DebugSessionImp::apiEvents;
    using L0::DebugSessionLinuxXe::internalEventQueue;
    using L0::DebugSessionLinuxXe::internalEventThread;

    MockDebugSessionLinuxXe(const zet_debug_config_t &config, L0::Device *device, int debugFd, void *params) : DebugSessionLinuxXe(config, device, debugFd, params) {
        clientHandleToConnection[mockClientHandle].reset(new ClientConnection);
        clientHandle = mockClientHandle;
        createEuThreads();
    }
    MockDebugSessionLinuxXe(const zet_debug_config_t &config, L0::Device *device, int debugFd) : MockDebugSessionLinuxXe(config, device, debugFd, nullptr) {}

    ze_result_t initialize() override {
        if (initializeRetVal != ZE_RESULT_FORCE_UINT32) {
            createEuThreads();
            clientHandle = mockClientHandle;
            return initializeRetVal;
        }
        return DebugSessionLinuxXe::initialize();
    }
    ze_result_t initializeRetVal = ZE_RESULT_FORCE_UINT32;
    static constexpr uint64_t mockClientHandle = 1;
};

} // namespace ult
} // namespace L0