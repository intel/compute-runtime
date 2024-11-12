/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(CompressionSystemXeHPAndLater, GENERATEONLY_givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect, IGFX_LUNARLAKE);
HWTEST_EXCLUDE_PRODUCT(CompressionSystemXeHPAndLater, GENERATEONLY_givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect, IGFX_LUNARLAKE);
HWTEST_EXCLUDE_PRODUCT(CompressionSystemXeHPAndLater, givenCompressedImageWhenReadingThenResultsAreCorrect, IGFX_LUNARLAKE);
