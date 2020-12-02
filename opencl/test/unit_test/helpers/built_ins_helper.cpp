/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/built_ins_helper.h"

#include "shared/source/device/device.h"

#include "opencl/source/compiler_interface/default_cl_cache_config.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_compilers.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sip.h"

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
