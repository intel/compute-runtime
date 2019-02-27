/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/built_ins_helper.h"

#include "runtime/execution_environment/execution_environment.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_program.h"

namespace OCLRT {

const SipKernel &initSipKernel(SipKernelType type, Device &device) {
    auto mockCompilerInterface = new MockCompilerInterface();
    mockCompilerInterface->initialize();

    device.getExecutionEnvironment()->compilerInterface.reset(mockCompilerInterface);
    mockCompilerInterface->sipKernelBinaryOverride = mockCompilerInterface->getDummyGenBinary();
    return device.getExecutionEnvironment()->getBuiltIns()->getSipKernel(type, device);
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
} // namespace OCLRT
