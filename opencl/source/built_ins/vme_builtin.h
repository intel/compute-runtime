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
class Device;
class Context;
class BuiltIns;
class BuiltinDispatchInfoBuilder;
namespace Vme {

Program *createBuiltInProgram(
    Context &context,
    Device &device,
    const char *kernelNames,
    int &errcodeRet);

BuiltinDispatchInfoBuilder &getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, Device &device);

} // namespace Vme
} // namespace NEO