/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

typedef Test<ExecutionModelSchedulerTest> BdwSchedulerTest;

BDWTEST_F(BdwSchedulerTest, givenCallToDispatchSchedulerWhenPipeControlWithCSStallIsAddedThenDCFlushEnabledIsSet) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pDevice->getSupportedClVersion() >= 20) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);
        SchedulerKernel &scheduler = pDevice->getBuiltIns().getSchedulerKernel(*context);

        size_t minRequiredSizeForSchedulerSSH = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        // Setup heaps in pCmdQ
        LinearStream &commandStream = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, false, false, &scheduler);
        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, minRequiredSizeForSchedulerSSH);

        GpgpuWalkerHelper<FamilyType>::dispatchScheduler(
            *pCmdQ,
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
