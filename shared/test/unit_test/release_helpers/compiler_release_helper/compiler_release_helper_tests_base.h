/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_ip_version.h"

#include "gtest/gtest.h"

#include <memory>
#include <vector>

namespace NEO {
class CompilerReleaseHelper;
}

using namespace NEO;

struct CompilerReleaseHelperTestsBase : public ::testing::Test {

    CompilerReleaseHelperTestsBase();
    ~CompilerReleaseHelperTestsBase() override;
    void whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned();
    void whenIsForceEmuInt32DivRemSPRequiredCalledThenTrueReturned();
    virtual std::vector<uint32_t> getRevisions() = 0;

    std::unique_ptr<CompilerReleaseHelper> compilerReleaseHelper;
    HardwareIpVersion ipVersion{};
};

template <uint32_t architecture, uint32_t release>
struct CompilerReleaseHelperTests : public CompilerReleaseHelperTestsBase {
    void SetUp() override {
        ipVersion.architecture = architecture;
        ipVersion.release = release;
    }
};
