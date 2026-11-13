/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "per_product_test_definitions.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<Xe2HpgCoreFamily>() {
    PreemptionTestHwDetails ret;
    return ret;
}
