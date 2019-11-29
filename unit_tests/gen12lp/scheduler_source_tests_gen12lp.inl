/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "runtime/device_queue/device_queue_hw.h"

// Keep the order of device_enqueue.h and scheduler_definitions.h as the latter uses defines from the first one
#include "runtime/gen12lp/device_enqueue.h"
#include "runtime/gen12lp/scheduler_definitions.h"
#include "unit_tests/scheduler/scheduler_source_tests.h"
// Keep this include below scheduler_definitions.h and device_enqueue.h headers as it depends on defines defined in them
#include "unit_tests/scheduler/scheduler_source_tests.inl"

using namespace NEO;

typedef SchedulerSourceTest SchedulerSourceTestGen12;
GEN12LPTEST_F(SchedulerSourceTestGen12, GivenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCode) {
    givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest<FamilyType>();
}

GEN12LPTEST_F(SchedulerSourceTestGen12, GivenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrect) {
    givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest<FamilyType>();
}

GEN12LPTEST_F(SchedulerSourceTestGen12, GivenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCode) {
    givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest<FamilyType>();
}
