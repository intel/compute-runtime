/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTest, whenAdjustPipelineSelectIsCalledThenNothingHappens, IGFX_GEN12LP_CORE);
