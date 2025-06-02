/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xe2_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xe_hpc_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"

using Family = NEO::Xe2HpgCoreFamily;

namespace NEO {

template struct UnitTestHelper<Family>;
template struct UnitTestHelperWithHeap<Family>;

template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER *walkerCmd);
} // namespace NEO
