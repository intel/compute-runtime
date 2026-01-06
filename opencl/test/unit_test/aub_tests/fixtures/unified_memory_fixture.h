/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

namespace NEO {
namespace PagaFaultManagerTestConfig {
extern bool disabled;
}

class UnifiedMemoryAubFixture : public AUBFixture {
  public:
    using AUBFixture::tearDown;

    cl_int retVal = CL_SUCCESS;
    const size_t dataSize = 2 * MemoryConstants::pageSize64k;
    bool skipped = false;

    void setUp();

    void *allocateUSM(InternalMemoryType type);

    void freeUSM(void *ptr, InternalMemoryType type);

    void writeToUsmMemory(std::vector<char> data, void *ptr, InternalMemoryType type);
};
} // namespace NEO
