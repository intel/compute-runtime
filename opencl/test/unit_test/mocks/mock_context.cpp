/*
 * Copyright (C) 2018-2022 Intel Corporation
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
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

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
        rootDeviceIndices.push_back(pClDevice->getRootDeviceIndex());
    }
    rootDeviceIndices.remove_duplicates();
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

BcsMockContext::BcsMockContext(ClDevice *device) : MockContext(device) {
    bcsOsContext.reset(OsContext::create(nullptr, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, device->getDeviceBitfield())));
    bcsCsr.reset(createCommandStream(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();
    bcsCsr->createGlobalFenceAllocation();

    auto mockBlitMemoryToAllocation = [this](const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                             Vec3<size_t> size) -> BlitOperationResult {
        auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                              *bcsCsr, memory, nullptr,
                                                                              hostPtr,
                                                                              memory->getGpuAddress(), 0,
                                                                              0, 0, size, 0, 0, 0, 0);

        BlitPropertiesContainer container;
        container.push_back(blitProperties);
        bcsCsr->flushBcsTask(container, true, false, const_cast<Device &>(device));

        return BlitOperationResult::Success;
    };
    blitMemoryToAllocationFuncBackup = mockBlitMemoryToAllocation;
}
BcsMockContext::~BcsMockContext() = default;
} // namespace NEO
