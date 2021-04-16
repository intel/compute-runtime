/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/mocks/mock_sip.h"

#include <cassert>

namespace NEO {

namespace MockSipData {
std::unique_ptr<MockSipKernel> mockSipKernel;
SipKernelType calledType = SipKernelType::COUNT;
bool called = false;
bool returned = true;
bool useMockSip = false;

void clearUseFlags() {
    calledType = SipKernelType::COUNT;
    called = false;
}
} // namespace MockSipData

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    if (MockSipData::useMockSip) {
        SipKernel::classType = SipClassType::Builtins;
        MockSipData::calledType = type;
        MockSipData::called = true;

        MockSipData::mockSipKernel->mockSipMemoryAllocation->clearUsageInfo();
        return MockSipData::returned;
    } else {
        return SipKernel::initSipKernelImpl(type, device);
    }
}

const SipKernel &SipKernel::getSipKernel(Device &device) {
    if (MockSipData::useMockSip) {
        return *MockSipData::mockSipKernel.get();
    } else {
        return SipKernel::getSipKernelImpl(device);
    }
}

} // namespace NEO
