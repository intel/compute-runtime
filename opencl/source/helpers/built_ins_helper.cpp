/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/built_ins_helper.h"

#include "shared/source/device/device.h"

#include "opencl/source/program/program.h"

namespace NEO {
const SipKernel &initSipKernel(SipKernelType type, Device &device) {
    return device.getBuiltIns()->getSipKernel(type, device);
}
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             cl_int *errcodeRet,
                             Device *device) {

    cl_int retVal = 0;
    auto program = Program::createFromGenBinary(executionEnvironment,
                                                context,
                                                binary.data(),
                                                size,
                                                true,
                                                &retVal,
                                                device);
    DEBUG_BREAK_IF(retVal != 0);
    return program;
}
} // namespace NEO
