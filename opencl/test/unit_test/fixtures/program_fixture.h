/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

namespace NEO {

class ProgramFixture {
  public:
    void CreateProgramFromBinary(Context *pContext, // NOLINT(readability-identifier-naming)
                                 const ClDeviceVector &deviceVector,
                                 const std::string &binaryFileName,
                                 cl_int &retVal,
                                 const std::string &options = "");

    void CreateProgramFromBinary(Context *pContext, // NOLINT(readability-identifier-naming)
                                 const ClDeviceVector &deviceVector,
                                 const std::string &binaryFileName,
                                 const std::string &options = "");

    void CreateProgramWithSource(Context *pContext, // NOLINT(readability-identifier-naming)
                                 const std::string &sourceFileName);

  protected:
    virtual void SetUp() { // NOLINT(readability-identifier-naming)
    }

    virtual void TearDown() { // NOLINT(readability-identifier-naming)
        Cleanup();
    }

    void Cleanup() { // NOLINT(readability-identifier-naming)
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
