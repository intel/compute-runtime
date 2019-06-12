/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "test.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef Test<ExecutionModelSchedulerTest> BdwSchedulerTest;

BDWTEST_F(BdwSchedulerTest, givenCallToDispatchSchedulerWhenPipeControlWithCSStallIsAddedThenDCFlushEnabledIsSet) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pDevice->getSupportedClVersion() >= 20) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);
        SchedulerKernel &scheduler = pDevice->getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(*context);

        size_t minRequiredSizeForSchedulerSSH = HardwareCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        // Setup heaps in pCmdQ
        LinearStream &commandStream = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, false, false, &scheduler);
        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, minRequiredSizeForSchedulerSSH);

        GpgpuWalkerHelper<FamilyType>::dispatchScheduler(
            commandStream,
            *pDevQueueHw,
            pDevice->getPreemptionMode(),
            scheduler,
            &pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
            pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE));

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream, 0);
        hwParser.findHardwareCommands<FamilyType>();

        ASSERT_NE(hwParser.cmdList.end(), hwParser.itorWalker);

        GenCmdList pcList = hwParser.getCommandsList<PIPE_CONTROL>();

        EXPECT_NE(0u, pcList.size());

        for (GenCmdList::iterator it = pcList.begin(); it != pcList.end(); it++) {
            PIPE_CONTROL *pc = (PIPE_CONTROL *)*it;
            ASSERT_NE(nullptr, pc);
            if (pc->getCommandStreamerStallEnable()) {
                EXPECT_TRUE(pc->getDcFlushEnable());
            }
        }
    }
}
