/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_context.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_svm_manager.h"

#include "d3d_sharing_functions.h"

namespace NEO {

MockContext::MockContext(ClDevice *pDevice, bool noSpecialQueue) {
    cl_device_id deviceId = pDevice;
    initializeWithDevices(ClDeviceVector{&deviceId, 1}, noSpecialQueue);
}

MockContext::MockContext(const ClDeviceVector &clDeviceVector, bool noSpecialQueue) {
    initializeWithDevices(clDeviceVector, noSpecialQueue);
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
    defaultDeviceQueue = nullptr;
    driverDiagnostics = nullptr;
    rootDeviceIndices = {};
    maxRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    deviceBitfields = {};
}

MockContext::~MockContext() {
    for (auto &rootDeviceIndex : rootDeviceIndices) {
        if (specialQueues[rootDeviceIndex]) {
            specialQueues[rootDeviceIndex]->release();
            specialQueues[rootDeviceIndex] = nullptr;
        }
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
        rootDeviceIndices.insert(pClDevice->getRootDeviceIndex());
    }
    maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
    specialQueues.resize(maxRootDeviceIndex + 1u);

    this->devices = devices;
    memoryManager = devices[0]->getMemoryManager();
    svmAllocsManager = new MockSVMAllocsManager(memoryManager,
                                                true);

    for (auto &rootDeviceIndex : rootDeviceIndices) {
        DeviceBitfield deviceBitfield{};
        for (const auto &pDevice : devices) {
            if (pDevice->getRootDeviceIndex() == rootDeviceIndex) {
                deviceBitfield |= pDevice->getDeviceBitfield();
            }
        }
        deviceBitfields.insert({rootDeviceIndex, deviceBitfield});
    }

    cl_int retVal;
    if (!noSpecialQueue) {
        for (auto &device : devices) {
            if (!specialQueues[device->getRootDeviceIndex()]) {
                auto commandQueue = CommandQueue::create(this, device, nullptr, false, retVal);
                assert(retVal == CL_SUCCESS);
                overrideSpecialQueueAndDecrementRefCount(commandQueue, device->getRootDeviceIndex());
            }
        }
    }

    setupContextType();
}

SchedulerKernel &MockContext::getSchedulerKernel() {
    if (schedulerBuiltIn->pKernel) {
        return *static_cast<SchedulerKernel *>(schedulerBuiltIn->pKernel);
    }

    auto initializeSchedulerProgramAndKernel = [&] {
        cl_int retVal = CL_SUCCESS;
        auto clDevice = getDevice(0);
        auto src = SchedulerKernel::loadSchedulerKernel(&clDevice->getDevice());

        auto program = Program::createBuiltInFromGenBinary(this,
                                                           devices,
                                                           src.resource.data(),
                                                           src.resource.size(),
                                                           &retVal);
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        DEBUG_BREAK_IF(!program);

        retVal = program->processGenBinary(*clDevice);
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);

        schedulerBuiltIn->pProgram = program;

        auto kernelInfo = schedulerBuiltIn->pProgram->getKernelInfo(SchedulerKernel::schedulerName, clDevice->getRootDeviceIndex());
        DEBUG_BREAK_IF(!kernelInfo);

        schedulerBuiltIn->pKernel = Kernel::create<MockSchedulerKernel>(
            schedulerBuiltIn->pProgram,
            *kernelInfo,
            *clDevice,
            &retVal);

        UNRECOVERABLE_IF(schedulerBuiltIn->pKernel->getScratchSize() != 0);

        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
    };
    std::call_once(schedulerBuiltIn->programIsInitialized, initializeSchedulerProgramAndKernel);

    UNRECOVERABLE_IF(schedulerBuiltIn->pKernel == nullptr);
    return *static_cast<SchedulerKernel *>(schedulerBuiltIn->pKernel);
}

MockDefaultContext::MockDefaultContext() : MockDefaultContext(false) {}

MockDefaultContext::MockDefaultContext(bool initSpecialQueues) : MockContext(nullptr, nullptr) {
    pRootDevice0 = ultClDeviceFactory.rootDevices[0];
    pRootDevice1 = ultClDeviceFactory.rootDevices[1];
    pRootDevice2 = ultClDeviceFactory.rootDevices[2];
    cl_device_id deviceIds[] = {pRootDevice0, pRootDevice1, pRootDevice2};
    initializeWithDevices(ClDeviceVector{deviceIds, 3}, !initSpecialQueues);
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
