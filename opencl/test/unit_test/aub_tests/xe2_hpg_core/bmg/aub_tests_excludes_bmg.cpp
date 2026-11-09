/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/test/common/test_macros/test_excludes.h"

HWTEST_EXCLUDE_PRODUCT(CompressionSystemXeHPAndLater, givenCompressedImageWhenReadingThenResultsAreCorrect, IGFX_BMG);
