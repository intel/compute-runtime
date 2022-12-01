/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterImageTests, givenCompressionWhenAppendingImageFromBufferThenTwoIsSetAsCompressionFormat, IGFX_METEORLAKE);
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterImageTests, givenImageFromBufferWhenSettingSurfaceStateThenPickCompressionFormatFromDebugVariable, IGFX_METEORLAKE);