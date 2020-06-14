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
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "d3d_sharing_functions.h"

namespace NEO {

MockContext::MockContext(ClDevice *pDevice, bool noSpecialQueue) {
    cl_device_id deviceId = pDevice;
    initializeWithDevices(ClDeviceVector{&deviceId, 1}, noSpecialQueue);
}

MockContext::MockContext(const ClDeviceVector &clDeviceVector) {
    initializeWithDevices(clDeviceVector, true);
}

MockContext::MockContext(
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *data) {
    pDevice = nullptr;
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
    if (memoryManager && memoryManager->isAsyncDeleterEnabled()) {
        memoryManager->getDeferredDeleter()->removeClient();
    }
    memoryManager = nullptr;
}

MockContext::MockContext() {
    pDevice = new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    cl_device_id deviceId = pDevice;
    initializeWithDevices(ClDeviceVector{&deviceId, 1}, false);
    pDevice->decRefInternal();
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

void MockContext::initializeWithDevices(const ClDeviceVector &devices, bool noSpecialQueue) {
    for (auto &pClDevice : devices) {
        pClDevice->incRefInternal();
    }
    this->devices = devices;
    memoryManager = devices[0]->getMemoryManager();
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!noSpecialQueue) {
        auto commandQueue = CommandQueue::create(this, devices[0], nullptr, false, retVal);
        assert(retVal == CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue);
    }
}

MockDefaultContext::MockDefaultContext() : MockContext(nullptr, nullptr) {
    pRootDevice0 = ultClDeviceFactory.rootDevices[0];
    pRootDevice1 = ultClDeviceFactory.rootDevices[1];
    cl_device_id deviceIds[] = {pRootDevice0, pRootDevice1};
    initializeWithDevices(ClDeviceVector{deviceIds, 2}, true);
}

MockSpecializedContext::MockSpecializedContext() : MockContext(nullptr, nullptr) {
    pRootDevice = ultClDeviceFactory.rootDevices[0];
    pSubDevice0 = ultClDeviceFactory.subDevices[0];
    pSubDevice1 = ultClDeviceFactory.subDevices[1];
    cl_device_id deviceIds[] = {pSubDevice0, pSubDevice1};
    initializeWithDevices(ClDeviceVector{deviceIds, 2}, true);
}

MockUnrestrictiveContext::MockUnrestrictiveContext() : MockContext(nullptr, nullptr) {
    pRootDevice = ultClDeviceFactory.rootDevices[0];
    pSubDevice0 = ultClDeviceFactory.subDevices[0];
    pSubDevice1 = ultClDeviceFactory.subDevices[1];
    cl_device_id deviceIds[] = {pRootDevice, pSubDevice0, pSubDevice1};
    initializeWithDevices(ClDeviceVector{deviceIds, 3}, true);
}

MockUnrestrictiveContextMultiGPU::MockUnrestrictiveContextMultiGPU() : MockContext(nullptr, nullptr) {
    pRootDevice0 = ultClDeviceFactory.rootDevices[0];
    pSubDevice00 = ultClDeviceFactory.subDevices[0];
    pSubDevice01 = ultClDeviceFactory.subDevices[1];
    pRootDevice1 = ultClDeviceFactory.rootDevices[1];
    pSubDevice10 = ultClDeviceFactory.subDevices[2];
    pSubDevice11 = ultClDeviceFactory.subDevices[3];
    cl_device_id deviceIds[] = {pRootDevice0, pSubDevice00, pSubDevice01,
                                pRootDevice1, pSubDevice10, pSubDevice11};
    initializeWithDevices(ClDeviceVector{deviceIds, 6}, true);
}

} // namespace NEO
