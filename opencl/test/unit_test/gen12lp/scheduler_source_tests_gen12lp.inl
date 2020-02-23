/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "opencl/source/device_queue/device_queue_hw.h"

// Keep the order of device_enqueue.h and scheduler_definitions.h as the latter uses defines from the first one
#include "opencl/source/gen12lp/device_enqueue.h"
#include "opencl/source/gen12lp/scheduler_definitions.h"
#include "opencl/test/unit_test/scheduler/scheduler_source_tests.h"
// Keep this include below scheduler_definitions.h and device_enqueue.h headers as it depends on defines defined in them
#include "opencl/test/unit_test/scheduler/scheduler_source_tests.inl"

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
