/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "opencl/source/event/event.h"
#include "opencl/source/event/event_builder.h"

namespace NEO {
void MockCommandQueue::insertBcsEngine(aub_stream::EngineType bcsEngineType) {
    const auto index = NEO::EngineHelpers::getBcsIndex(bcsEngineType);
    const auto engine = &getDevice().getEngine(bcsEngineType, EngineUsage::regular);
    bcsEngines[index] = engine;
    bcsQueueEngineType = bcsEngineType;
    bcsInitialized = true;
}

size_t MockCommandQueue::countBcsEngines() const {
    return std::count_if(bcsEngines.begin(), bcsEngines.end(), [](const EngineControl *engine) {
        return engine != nullptr && engine->getEngineUsage() == EngineUsage::regular;
    });
}

MockCommandQueue::MockCommandQueue() : CommandQueue(nullptr, nullptr, 0, false) {}
MockCommandQueue::MockCommandQueue(Context &context) : MockCommandQueue(&context, context.getDevice(0), nullptr, false) {}
MockCommandQueue::MockCommandQueue(Context *context, ClDevice *device, const cl_queue_properties *props, bool internalUsage)
    : CommandQueue(context, device, props, internalUsage) {
}

cl_int MockCommandQueue::enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    if (event) {
        EventBuilder eventBuilder;
        eventBuilder.create<Event>(this, CL_COMMAND_MARKER, CompletionStamp::notReady, 0);
        auto eventObj = eventBuilder.getEvent();
        *event = eventObj;
    }
    enqueueMarkerWithWaitListCalled = true;
    return CL_SUCCESS;
}

bool MockCommandQueue::isCompleted(TaskCountType gpgpuTaskCount, std::span<const CopyEngineState> bcsStates) {
    isCompletedCalled++;

    if (!device || !getGpgpuCommandStreamReceiver().getTagAddress()) {
        return true;
    }

    return CommandQueue::isCompleted(gpgpuTaskCount, bcsStates);
}
} // namespace NEO
