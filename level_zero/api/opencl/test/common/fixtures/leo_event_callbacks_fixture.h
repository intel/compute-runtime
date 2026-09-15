/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_async_events_handler.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "CL/cl.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct WhiteBoxPlatform : public Platform {
    using Platform::asyncEventsHandler;
};

struct MockAsyncEventsHandler : public AsyncEventsHandler {
    using AsyncEventsHandler::allowAsyncProcess;
    using AsyncEventsHandler::asyncMtx;
    using AsyncEventsHandler::asyncProcess;
    using AsyncEventsHandler::list;
    using AsyncEventsHandler::processList;
    using AsyncEventsHandler::registerList;
    using AsyncEventsHandler::thread;
    using AsyncEventsHandler::transferRegisterList;

    explicit MockAsyncEventsHandler(bool allowThreadCreating) : allowThreadCreating(allowThreadCreating) {}

    ~MockAsyncEventsHandler() override {
        if (!allowThreadCreating) {
            asyncProcess(this);
        }
    }

    void openThread() override {
        openThreadCalled = true;
        if (allowThreadCreating) {
            AsyncEventsHandler::openThread();
        }
    }

    Event *process() {
        transferRegisterList();
        return processList();
    }

    bool peekIsListEmpty() { return list.empty(); }
    bool peekIsRegisterListEmpty() { return registerList.empty(); }

    bool openThreadCalled = false;
    bool allowThreadCreating = false;
};

struct CallbackTracker {
    static void CL_CALLBACK callback(cl_event event, cl_int status, void *userData) {
        auto tracker = static_cast<CallbackTracker *>(userData);
        tracker->lastEvent = event;
        tracker->lastStatus = status;
        tracker->count++;
    }

    bool waitForCount(uint32_t expectedCount, uint32_t timeoutMs = 30000) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (count < expectedCount) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    std::atomic<uint32_t> count{0};
    std::atomic<cl_event> lastEvent{nullptr};
    std::atomic<cl_int> lastStatus{CL_SUCCESS};
};

struct EventCallbacksFixture : public OclFixture {
    void setUp() {
        OclFixture::setUp();

        clDeviceId = platform->getDevices()[0].get();
        clDevice = castToObject<ClDevice>(clDeviceId);

        cl_int errcode = CL_SUCCESS;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);
        context = castToObject<Context>(clContext);

        commandQueue = new CommandQueue(context, clDevice, nullptr, mockCmdList.toHandle());
    }

    void tearDown() {
        installMockHandler(false);

        commandQueue->decRefApi();
        clReleaseContext(clContext);
        OclFixture::tearDown();
    }

    template <typename HandlerT = MockAsyncEventsHandler>
    HandlerT *installMockHandler(bool allowThreadCreating) {
        auto mockHandler = new HandlerT(allowThreadCreating);
        static_cast<WhiteBoxPlatform *>(platform)->asyncEventsHandler.reset(mockHandler);
        return mockHandler;
    }

    CallbackTracker &createTracker() {
        trackers.push_back(std::make_unique<CallbackTracker>());
        return *trackers.back();
    }

    static void signalEvent(Event *event) {
        L0::Event::fromHandle(event->getL0Handle())->hostSignal(false);
    }

    cl_device_id clDeviceId = nullptr;
    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *context = nullptr;
    L0::ult::Mock<L0::ult::CommandList> mockCmdList{};
    CommandQueue *commandQueue = nullptr;
    std::vector<std::unique_ptr<CallbackTracker>> trackers;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
