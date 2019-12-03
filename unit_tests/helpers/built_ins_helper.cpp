/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/built_ins_helper.h"

#include "runtime/compiler_interface/default_cl_cache_config.h"
#include "runtime/device/device.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_sip.h"

namespace NEO {

namespace MockSipData {
static MockSipKernel mockSipKernel;
SipKernelType calledType = SipKernelType::COUNT;
bool called = false;
} // namespace MockSipData

const SipKernel &initSipKernel(SipKernelType type, Device &device) {
    MockSipData::calledType = type;
    MockSipData::mockSipKernel.type = type;
    MockSipData::called = true;
    return MockSipData::mockSipKernel;
}
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             cl_int *errcodeRet) {
    GlobalMockSipProgram::sipProgram->incRefApi();
    GlobalMockSipProgram::sipProgram->resetAllocationState();
    return GlobalMockSipProgram::sipProgram;
}
} // namespace NEO
