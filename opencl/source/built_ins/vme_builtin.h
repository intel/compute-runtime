/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/built_ins/built_in_ops_vme.h"

namespace NEO {
class Program;
class ClDevice;
class ClDeviceVector;
class Context;
class BuiltIns;
class BuiltinDispatchInfoBuilder;
namespace Vme {

Program *createBuiltInProgram(
    Context &context,
    const ClDeviceVector &deviceVector,
    const char *kernelNames,
    int &errcodeRet);

BuiltinDispatchInfoBuilder &getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, ClDevice &device);

} // namespace Vme
} // namespace NEO