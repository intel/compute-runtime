/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/test_macros/hw_test_base.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(MultiContextLargeGrfKernelAubTest, givenLargeAndSmallGrfWhenParallelRunOnCcsAndRcsThenResultsAreOk, IGFX_XE2_HPG_CORE);
