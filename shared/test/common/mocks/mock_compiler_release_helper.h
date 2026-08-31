/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {

class MockCompilerReleaseHelper : public CompilerReleaseHelper {
  public:
    MockCompilerReleaseHelper() : CompilerReleaseHelper(0) {}
    ADDMETHOD_CONST_NOBASE(getAdditionalFp16Caps, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(getAdditionalExtraCaps, uint32_t, {}, ());
};

} // namespace NEO
