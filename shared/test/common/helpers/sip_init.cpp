/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_sip.h"

namespace NEO {

namespace MockSipData {
std::unique_ptr<MockSipKernel> mockSipKernel;
SipKernelType calledType = SipKernelType::count;
bool called = false;
bool returned = true;
bool useMockSip = false;
bool uninitializedSipRequested = false;

void clearUseFlags() {
    calledType = SipKernelType::count;
    called = false;
}
} // namespace MockSipData

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    if (MockSipData::useMockSip) {
        MockSipData::calledType = type;
        MockSipData::called = true;

        MockSipData::mockSipKernel->mockSipMemoryAllocation->clearUsageInfo();
        MockSipData::mockSipKernel->createTempSipAllocation(MemoryManager::maxOsContextCount);

        return MockSipData::returned;
    } else {
        return SipKernel::initSipKernelImpl(type, device, nullptr);
    }
}

void SipKernel::freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager) {

    if (MockSipData::mockSipKernel) {
        MockSipData::mockSipKernel->tempSipMemoryAllocation.reset(nullptr);
    }

    for (auto &sipKernel : rootDeviceEnvironment->sipKernels) {
        if (sipKernel.get()) {
            memoryManager->freeGraphicsMemory(sipKernel->getSipAllocation());
            sipKernel.reset();
        }
    }
}

const SipKernel &SipKernel::getSipKernel(Device &device, OsContext *context) {
    if (MockSipData::useMockSip) {
        if (!MockSipData::called) {
            MockSipData::uninitializedSipRequested = true;
        }
        return *MockSipData::mockSipKernel.get();
    } else {
        if (context && device.getExecutionEnvironment()->getDebuggingMode() != NEO::DebuggingMode::disabled &&
            device.getGfxCoreHelper().isPerContextDebugSipRequired()) {
            return SipKernel::getDebugSipKernel(device, context);
        }
        auto sipType = SipKernel::getSipKernelType(device);
        SipKernel::initSipKernel(sipType, device);
        return SipKernel::getSipKernelImpl(device);
    }
}

} // namespace NEO
