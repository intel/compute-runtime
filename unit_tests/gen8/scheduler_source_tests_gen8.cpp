/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device_queue/device_queue_hw.h"

#include "hw_cmds.h"

// Keep the order of device_enqueue.h and scheduler_definitions.h as the latter uses defines from the first one
#include "runtime/gen8/device_enqueue.h"
#include "runtime/gen8/scheduler_definitions.h"
#include "unit_tests/scheduler/scheduler_source_tests.h"
// Keep this include below scheduler_definitions.h and device_enqueue.h headers as it depends on defines defined in them
#include "unit_tests/scheduler/scheduler_source_tests.inl"

using namespace OCLRT;

typedef SchedulerSourceTest SchedulerSourceTestGen8;
GEN8TEST_F(SchedulerSourceTestGen8, GivenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCode) {
    givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest<FamilyType>();
}

GEN8TEST_F(SchedulerSourceTestGen8, GivenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrect) {
    givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest<FamilyType>();
}

GEN8TEST_F(SchedulerSourceTestGen8, GivenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCode) {
    givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest<FamilyType>();
}
