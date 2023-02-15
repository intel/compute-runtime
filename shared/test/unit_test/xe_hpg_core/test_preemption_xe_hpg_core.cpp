/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<XeHpgCoreFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}
