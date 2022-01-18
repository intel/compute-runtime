/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_tests.h"

using namespace NEO;

class MockBuffer;
class ProgramWithZebinFixture : public ProgramTests {
  public:
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<KernelInfo> kernelInfo;
    std::unique_ptr<MockGraphicsAllocation> mockAlloc;
    std::unique_ptr<MockBuffer> globalSurface;
    std::unique_ptr<MockBuffer> constantSurface;
    const char strings[12] = "Hello olleH";
    const char kernelName[8] = "kernel1";
    void SetUp() override;
    void TearDown() override;
    void addEmptyZebin(MockProgram *program);
    void populateProgramWithSegments(MockProgram *program);
    ~ProgramWithZebinFixture() = default;
};

class ProgramWithDebugDataCreationFixture : public ProgramWithZebinFixture {

    class MockProgramWithDebugDataCreation : public MockProgram {
      public:
        using Program::createDebugData;
        MockProgramWithDebugDataCreation(const ClDeviceVector &deviceVector) : MockProgram(deviceVector) {
        }
        ~MockProgramWithDebugDataCreation() {
        }
        bool wasProcessDebugDataCalled = false;
        void processDebugData(uint32_t rootDeviceIndex) override {
            wasProcessDebugDataCalled = true;
        }
        bool wasCreateDebugZebinCalled = false;
        void createDebugZebin(uint32_t rootDeviceIndex) override {
            MockProgram::createDebugZebin(rootDeviceIndex);
            wasCreateDebugZebinCalled = true;
        }
    };

  public:
    std::unique_ptr<MockProgramWithDebugDataCreation> programWithDebugDataCreation;
    void SetUp() override;
    void TearDown() override;
};