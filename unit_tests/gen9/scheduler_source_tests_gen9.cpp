/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device_queue/device_queue_hw.h"

#include "hw_cmds.h"

// Keep the order of device_enqueue.h and scheduler_definitions.h as the latter uses defines from the first one
#include "runtime/gen9/device_enqueue.h"
#include "runtime/gen9/scheduler_definitions.h"
#include "unit_tests/scheduler/scheduler_source_tests.h"
// Keep this include below scheduler_definitions.h and device_enqueue.h headers as it depends on defines defined in them
#include "unit_tests/scheduler/scheduler_source_tests.inl"

using namespace OCLRT;

typedef SchedulerSourceTest SchedulerSourceTestGen9;
GEN9TEST_F(SchedulerSourceTestGen9, GivenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCode) {
    givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest<FamilyType>();
}

GEN9TEST_F(SchedulerSourceTestGen9, GivenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrect) {
    givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest<FamilyType>();
}

GEN9TEST_F(SchedulerSourceTestGen9, GivenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCode) {
    givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest<FamilyType>();
}
