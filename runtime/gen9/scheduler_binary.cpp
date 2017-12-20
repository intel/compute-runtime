/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/scheduler/scheduler_binary.h"

namespace OCLRT {
#include "scheduler_igdrcl_built_in_kbl.h"
#include "scheduler_igdrcl_built_in_skl.h"

const auto gfxProductSkl = PRODUCT_FAMILY::IGFX_SKYLAKE;
const auto gfxProductKbl = PRODUCT_FAMILY::IGFX_KABYLAKE;

template <>
void populateSchedulerBinaryInfos<gfxProductSkl>() {
    schedulerBinaryInfos[gfxProductSkl] = {schedulerBinary_skl, schedulerBinarySize_skl};
}

template <>
void populateSchedulerBinaryInfos<gfxProductKbl>() {
    schedulerBinaryInfos[gfxProductKbl] = {schedulerBinary_kbl, schedulerBinarySize_kbl};
}

struct EnableGen9Scheduler {
    EnableGen9Scheduler() {
        populateSchedulerBinaryInfos<gfxProductSkl>();
        populateSchedulerBinaryInfos<gfxProductKbl>();
    }
};

static EnableGen9Scheduler enableGen9Scheduler;
}
