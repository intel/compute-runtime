/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/file_io.h"
#include "runtime/program/program.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_program.h"

#include "gtest/gtest.h"

namespace NEO {

class ProgramFixture {
  public:
    ProgramFixture() : pProgram(nullptr),
                       knownSource(nullptr),
                       knownSourceSize(0) {}

    void CreateProgramFromBinary(cl_context context,
                                 cl_device_id *pDeviceList,
                                 const std::string &binaryFileName,
                                 cl_int &retVal,
                                 const std::string &options = "");

    void CreateProgramFromBinary(cl_context pContext,
                                 cl_device_id *pDeviceList,
                                 const std::string &binaryFileName,
                                 const std::string &options = "");

    void CreateProgramWithSource(cl_context pContext,
                                 cl_device_id *pDeviceList,
                                 const std::string &SourceFileName);

  protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
        Cleanup();
    }

    void Cleanup() {
        if (pProgram != nullptr) {
            pProgram->release();
        }
        knownSource.reset();
    }

    MockProgram *pProgram;
    std::unique_ptr<char[]> knownSource;
    size_t knownSourceSize;
};
} // namespace NEO
