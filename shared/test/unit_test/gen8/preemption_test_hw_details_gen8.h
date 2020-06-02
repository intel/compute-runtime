/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<BDWFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = 0;
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = (1 << 2);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2248u;
    return ret;
}
