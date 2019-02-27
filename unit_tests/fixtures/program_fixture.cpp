/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/program_fixture.inl"

#include "runtime/program/create.inl"
#include "unit_tests/mocks/mock_program.h"

namespace OCLRT {

template OCLRT::MockProgram *Program::create<MockProgram>(cl_context, cl_uint, const char **, const size_t *, cl_int &);
template OCLRT::MockProgram *Program::create<MockProgram>(cl_context, cl_uint, const cl_device_id *, const size_t *, const unsigned char **, cl_int *, cl_int &);
template OCLRT::MockProgram *Program::create<MockProgram>(const char *, Context *, Device &, bool, cl_int *);
template void ProgramFixture::CreateProgramFromBinary<Program>(cl_context, cl_device_id *, const std::string &, const std::string &);
template void ProgramFixture::CreateProgramFromBinary<Program>(cl_context, cl_device_id *, const std::string &, cl_int &, const std::string &);
template void ProgramFixture::CreateProgramFromBinary<MockProgram>(cl_context, cl_device_id *, const std::string &, const std::string &);
template void ProgramFixture::CreateProgramFromBinary<MockProgram>(cl_context, cl_device_id *, const std::string &, cl_int &, const std::string &);
} // namespace OCLRT
