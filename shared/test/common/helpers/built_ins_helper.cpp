/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/built_ins_helper.h"

#include "shared/source/device/device.h"
#include "shared/test/common/mocks/mock_sip.h"

namespace NEO {

namespace MockSipData {
std::unique_ptr<MockSipKernel> mockSipKernel;
SipKernelType calledType = SipKernelType::COUNT;
bool called = false;
} // namespace MockSipData

void initSipKernel(SipKernelType type, Device &device) {
    MockSipData::calledType = type;
    MockSipData::called = true;
}

} // namespace NEO
