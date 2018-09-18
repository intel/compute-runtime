/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/scheduler/scheduler_binary.h"

namespace OCLRT {
#include "scheduler_igdrcl_built_in_cnl.h"

const auto gfxProductCnl = PRODUCT_FAMILY::IGFX_CANNONLAKE;

template <>
void populateSchedulerBinaryInfos<gfxProductCnl>() {
    schedulerBinaryInfos[gfxProductCnl] = {schedulerBinary_cnl, schedulerBinarySize_cnl};
}

struct EnableGen10Scheduler {
    EnableGen10Scheduler() {
        populateSchedulerBinaryInfos<gfxProductCnl>();
    }
};

static EnableGen10Scheduler enableGen10Scheduler;
} // namespace OCLRT
