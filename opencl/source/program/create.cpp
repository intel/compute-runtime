/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/create.inl"

#include "shared/source/compiler_interface/external_functions.h"

#include "opencl/source/program/program.h"

namespace NEO {

namespace ProgramFunctions {
CreateFromILFunc createFromIL = Program::createFromIL<Program>;
} // namespace ProgramFunctions

template Program *Program::create<Program>(Context *, const ClDeviceVector &, const size_t *, const unsigned char **, cl_int *, cl_int &);
template Program *Program::create<Program>(Context *, cl_uint, const char **, const size_t *, cl_int &);
template Program *Program::createBuiltInFromSource<Program>(const char *, Context *, const ClDeviceVector &, cl_int *);
template Program *Program::createFromIL<Program>(Context *, const void *, size_t length, cl_int &);
template Program *Program::createBuiltInFromGenBinary<Program>(Context *context, const ClDeviceVector &, const void *binary, size_t size, cl_int *errcodeRet);
} // namespace NEO
