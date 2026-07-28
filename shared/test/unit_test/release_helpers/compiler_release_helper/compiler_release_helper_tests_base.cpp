/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

CompilerReleaseHelperTestsBase::CompilerReleaseHelperTestsBase() = default;
CompilerReleaseHelperTestsBase::~CompilerReleaseHelperTestsBase() = default;

void CompilerReleaseHelperTestsBase::whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);
        EXPECT_FALSE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
    }
}

void CompilerReleaseHelperTestsBase::whenIsForceEmuInt32DivRemSPRequiredCalledThenTrueReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);
        EXPECT_TRUE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
    }
}
