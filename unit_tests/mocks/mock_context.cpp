/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/sharing.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/built_ins/built_ins.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "d3d_sharing_functions.h"

namespace OCLRT {

MockContext::MockContext(Device *device, bool noSpecialQueue) {
    memoryManager = device->getMemoryManager();
    devices.push_back(device);
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!specialQueue && !noSpecialQueue) {
        auto commandQueue = CommandQueue::create(this, device, nullptr, retVal);
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
    device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    devices.push_back(device);
    memoryManager = device->getMemoryManager();
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!specialQueue) {
        auto commandQueue = CommandQueue::create(this, device, nullptr, retVal);
        assert(retVal == CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue);
    }
}

void MockContext::setSharingFunctions(SharingFunctions *sharingFunctions) {
    this->sharingFunctions[sharingFunctions->getId()].reset(sharingFunctions);
}

void MockContext::setContextType(ContextType contextType) {
    this->contextType = contextType;
}

void MockContext::releaseSharingFunctions(SharingType sharing) {
    this->sharingFunctions[sharing].release();
}

void MockContext::registerSharingWithId(SharingFunctions *sharing, SharingType sharingId) {
    this->sharingFunctions[sharingId].reset(sharing);
}

void MockContext::clearSharingFunctions() {
    std::vector<decltype(this->sharingFunctions)::value_type> v;
    this->sharingFunctions.swap(v);
}
} // namespace OCLRT
