/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/test/common/test_macros/hw_test_base.h"
using namespace NEO;

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(CopyOffloadMultiTileL0AubTests, givenCopyOffloadCmdListWhenDispatchingThenDataIsCorrect_IsAtLeastXeCore, IGFX_CRI);
HWTEST_EXCLUDE_PRODUCT(SynchronizedDispatchMultiTileL0AubTests, givenFullSyncDispatchWhenExecutingThenDataIsCorrect, IGFX_CRI);
} // namespace ult
} // namespace L0
