/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"

#include "opencl/source/device_queue/device_queue_hw.h"

// Keep the order of device_enqueue.h and scheduler_definitions.h as the latter uses defines from the first one
#include "opencl/source/gen11/device_enqueue.h"
#include "opencl/source/gen11/scheduler_definitions.h"
#include "opencl/test/unit_test/scheduler/scheduler_source_tests.h"
// Keep this include below scheduler_definitions.h and device_enqueue.h headers as it depends on defines defined in them
#include "opencl/test/unit_test/scheduler/scheduler_source_tests.inl"

using namespace NEO;

static_assert((SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE_GEN11 & (MemoryConstants::cacheLineSize - 1)) == 0, "Second level buffer space incorrect for gen11");

typedef SchedulerSourceTest SchedulerSourceTestGen11;
GEN11TEST_F(SchedulerSourceTestGen11, GivenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCode) {
    givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest<FamilyType>();
}

GEN11TEST_F(SchedulerSourceTestGen11, GivenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrect) {
    givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest<FamilyType>();
}

GEN11TEST_F(SchedulerSourceTestGen11, GivenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCode) {
    givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest<FamilyType>();
}
