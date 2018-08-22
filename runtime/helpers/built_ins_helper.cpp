/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/built_ins_helper.h"
#include "runtime/program/program.h"

namespace OCLRT {
const SipKernel &initSipKernel(SipKernelType type, Device &device) {
    return device.getExecutionEnvironment()->getBuiltIns()->getSipKernel(type, device);
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
} // namespace OCLRT
