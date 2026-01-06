/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<XeHpgCoreFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = threadGroupMode;
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = midBatchMode;
    ret.modeToRegValueMap[PreemptionMode::MidThread] = midThreadMode;
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}
