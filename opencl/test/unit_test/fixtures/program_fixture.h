/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
#include <string>

namespace NEO {
class Context;
class ClDeviceVector;
class MockContext;
class MockProgram;
class ClDevice;
class MockNeoProgram;

using cl_int = int;

class NEOProgramFixture {
  public:
    NEOProgramFixture();
    ~NEOProgramFixture();

  protected:
    void setUp();
    void tearDown();
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockNeoProgram> program;
};

class ProgramFixture {
  public:
    void createProgramFromBinary(Context *pContext,
                                 const ClDeviceVector &deviceVector,
                                 const unsigned char **binary,
                                 const size_t *binarySize);

    void createProgramFromBinary(Context *pContext,
                                 const ClDeviceVector &deviceVector,
                                 const std::string &binaryFileName,
                                 cl_int &retVal,
                                 const std::string &options = "");

    void createProgramFromBinary(Context *pContext,
                                 const ClDeviceVector &deviceVector,
                                 const std::string &binaryFileName,
                                 const std::string &options = "");

    void createProgramWithSource(Context *pContext,
                                 const std::string &sourceFileName);

  protected:
    void setUp() {
    }

    void tearDown() {
        cleanup();
    }

    void cleanup();

    MockProgram *pProgram = nullptr;
    std::unique_ptr<char[]> knownSource;
    size_t knownSourceSize = 0u;
};
} // namespace NEO
