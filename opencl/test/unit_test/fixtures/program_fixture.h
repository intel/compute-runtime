/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/file_io.h"

#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/helpers/test_files.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

namespace NEO {

class ProgramFixture {
  public:
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
                                 const std::string &sourceFileName);

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

    MockProgram *pProgram = nullptr;
    std::unique_ptr<char[]> knownSource;
    size_t knownSourceSize = 0u;
};
} // namespace NEO
