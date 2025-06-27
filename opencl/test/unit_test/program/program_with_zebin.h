/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/program/program_tests.h"

namespace NEO {
struct KernelInfo;
class MockGraphicsAllocation;
class MockProgram;
} // namespace NEO

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
    void SetUp() override;
    void TearDown() override;
    void addEmptyZebin(MockProgram *program);
    void populateProgramWithSegments(MockProgram *program);
    ~ProgramWithZebinFixture() override;
    ProgramWithZebinFixture();
};
