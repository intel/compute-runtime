/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"

namespace NEO {
template <PRODUCT_FAMILY productFamily>
class MockCompilerProductHelperHeaplessHw : public CompilerProductHelperHw<productFamily> {
  public:
    bool isHeaplessModeEnabled() const override {
        return heaplessModeEnabled;
    }

    MockCompilerProductHelperHeaplessHw(bool heaplessModeEnabled) : CompilerProductHelperHw<productFamily>(), heaplessModeEnabled(heaplessModeEnabled) {}
    bool heaplessModeEnabled = false;
};

} // namespace NEO
