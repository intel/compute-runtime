/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_context.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "d3d_sharing_functions.h"

namespace NEO {

MockContext::MockContext(ClDevice *device, bool noSpecialQueue) {
    memoryManager = device->getMemoryManager();
    devices.push_back(device);
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!specialQueue && !noSpecialQueue) {
        auto commandQueue = CommandQueue::create(this, device, nullptr, false, retVal);
        assert(retVal == CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue);
    }
    device->incRefInternal();
}

MockContext::MockContext(
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *data) {
    device = nullptr;
    properties = nullptr;
    numProperties = 0;
    contextCallback = funcNotify;
    userData = data;
    memoryManager = nullptr;
    specialQueue = nullptr;
    defaultDeviceQueue = nullptr;
    driverDiagnostics = nullptr;
}

MockContext::~MockContext() {
    if (specialQueue) {
        specialQueue->release();
        specialQueue = nullptr;
    }
    if (memoryManager->isAsyncDeleterEnabled()) {
        memoryManager->getDeferredDeleter()->removeClient();
    }
    memoryManager = nullptr;
}

MockContext::MockContext() {
    device = new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    devices.push_back(device);
    memoryManager = device->getMemoryManager();
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!specialQueue) {
        auto commandQueue = CommandQueue::create(this, device, nullptr, false, retVal);
        assert(retVal == CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue);
    }
}

void MockContext::setSharingFunctions(SharingFunctions *sharingFunctions) {
    this->sharingFunctions[sharingFunctions->getId()].reset(sharingFunctions);
}

void MockContext::releaseSharingFunctions(SharingType sharing) {
    this->sharingFunctions[sharing].release();
}

void MockContext::resetSharingFunctions(SharingType sharing) {
    this->sharingFunctions[sharing].reset();
}

void MockContext::registerSharingWithId(SharingFunctions *sharing, SharingType sharingId) {
    this->sharingFunctions[sharingId].reset(sharing);
}

void MockContext::clearSharingFunctions() {
    std::vector<decltype(this->sharingFunctions)::value_type> v;
    this->sharingFunctions.swap(v);
}

std::unique_ptr<AsyncEventsHandler> &MockContext::getAsyncEventsHandlerUniquePtr() {
    return static_cast<MockClExecutionEnvironment *>(devices[0]->getExecutionEnvironment())->asyncEventsHandler;
}

} // namespace NEO
