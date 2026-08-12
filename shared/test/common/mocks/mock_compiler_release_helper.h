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
    ADDMETHOD_CONST_NOBASE(isForceEmuInt32DivRemSPRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isBindlessAddressingDisabled, bool, true, ());
    ADDMETHOD_CONST_NOBASE(isMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSplitMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getAdditionalFp16Caps, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(getAdditionalExtraCaps, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(getFtrXe2Compression, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isAvailableSemaphore64Base, bool, false, ());

    bool isBFloat16ConversionSupported() const override {
        return bFloat16Support;
    }
    bool bFloat16Support = false;
};

} // namespace NEO
