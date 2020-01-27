/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/built_ins_helper.h"

#include "runtime/device/cl_device.h"
#include "runtime/program/program.h"

namespace NEO {
const SipKernel &initSipKernel(SipKernelType type, ClDevice &device) {
    return device.getExecutionEnvironment()->getBuiltIns()->getSipKernel(type, device.getDevice());
}
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             cl_int *errcodeRet) {

    cl_int retVal = 0;
    auto program = Program::createFromGenBinary(executionEnvironment,
                                                nullptr,
                                                binary.data(),
                                                size,
                                                true,
                                                &retVal);
    DEBUG_BREAK_IF(retVal != 0);
    return program;
}
} // namespace NEO
