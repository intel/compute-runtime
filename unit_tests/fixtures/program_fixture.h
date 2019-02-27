/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/file_io.h"
#include "runtime/program/program.h"
#include "unit_tests/helpers/test_files.h"

#include "gtest/gtest.h"

namespace OCLRT {

class ProgramFixture {
  public:
    ProgramFixture() : pProgram(nullptr),
                       knownSource(nullptr),
                       knownSourceSize(0) {}

    template <typename T>
    void CreateProgramFromBinary(cl_context context,
                                 cl_device_id *pDeviceList,
                                 const std::string &binaryFileName,
                                 cl_int &retVal,
                                 const std::string &options = "");

    template <typename T>
    void CreateProgramFromBinary(cl_context pContext,
                                 cl_device_id *pDeviceList,
                                 const std::string &binaryFileName,
                                 const std::string &options = "");

    template <typename T = Program>
    void CreateProgramWithSource(cl_context pContext,
                                 cl_device_id *pDeviceList,
                                 const std::string &SourceFileName) {
        Cleanup();
        cl_int retVal = CL_SUCCESS;
        std::string testFile;

        testFile.append(clFiles);
        testFile.append(SourceFileName);
        ASSERT_EQ(true, fileExists(testFile));

        knownSourceSize = loadDataFromFile(
            testFile.c_str(),
            knownSource);

        ASSERT_NE(0u, knownSourceSize);
        ASSERT_NE(nullptr, knownSource);

        pProgram = Program::create<T>(
            pContext,
            1,
            (const char **)(&knownSource),
            &knownSourceSize,
            retVal);

        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

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

        if (knownSource != nullptr) {
            deleteDataReadFromFile(knownSource);
            knownSource = nullptr;
        }
    }

    Program *pProgram;
    void *knownSource;
    size_t knownSourceSize;
};
} // namespace OCLRT
