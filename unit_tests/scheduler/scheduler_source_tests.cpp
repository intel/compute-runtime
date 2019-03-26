/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/scheduler/scheduler_source_tests.h"

#include "runtime/device_queue/device_queue_hw.h"
#include "test.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device_queue.h"

#include "gtest/gtest.h"
#include "hw_cmds.h"
// Keep this include after execution_model_fixture.h otherwise there is high chance of conflict with macros
#include "runtime/builtin_kernels_simulation/opencl_c.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"

extern PRODUCT_FAMILY defaultProductFamily;

using namespace NEO;
using namespace BuiltinKernelsSimulation;

HWCMDTEST_F(IGFX_GEN8_CORE, SchedulerSourceTest, PatchGpgpuWalker) {
    using MEDIA_STATE_FLUSH = typename FamilyType::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t msfOffset = 0;
    size_t miArbCheckOffset = 0;
    size_t miAtomicOffset = 0;
    size_t mediaIDLoadOffset = 0;
    size_t miLoadRegOffset = 0;
    size_t pipeControlOffset = 0;
    size_t gpgpuOffset = 0;
    size_t msfOffset2 = 0;
    size_t miArbCheckOffset2 = 0;

    size_t msfOffsetAfter = 0;
    size_t miArbCheckOffsetAfter = 0;
    size_t miAtomicOffsetAfter = 0;
    size_t mediaIDLoadOffsetAfter = 0;
    size_t miLoadRegOffsetAfter = 0;
    size_t pipeControlOffsetAfter = 0;
    size_t gpgpuOffsetAfter = 0;
    size_t msfOffsetAfter2 = 0;
    size_t miArbCheckOffsetAfter2 = 0;

    auto pDevQueueHw = new MockDeviceQueueHw<FamilyType>(&context, pDevice, DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);

    // Prepopulate SLB with commands
    pDevQueueHw->buildSlbDummyCommands();
    LinearStream *slb = pDevQueueHw->getSlbCS();
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*slb, 0);

    // Parse commands and save offsets of first enqueue space
    auto itorMediaStateFlush = find<MEDIA_STATE_FLUSH *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *msf = (MEDIA_STATE_FLUSH *)*itorMediaStateFlush;

    EXPECT_EQ((void *)slb->getCpuBase(), (void *)msf);

    auto itorArbCheck = find<MI_ARB_CHECK *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *arbCheck = itorArbCheck != hwParser.cmdList.end() ? (MI_ARB_CHECK *)*itorArbCheck : nullptr;

    auto itorMiAtomic = find<MI_ATOMIC *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *miAtomic = itorMiAtomic != hwParser.cmdList.end() ? (MI_ATOMIC *)*itorMiAtomic : nullptr;

    auto itorIDLoad = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *idLoad = itorIDLoad != hwParser.cmdList.end() ? (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorIDLoad : nullptr;

    auto itorMiLoadReg = find<MI_LOAD_REGISTER_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *miLoadReg = itorMiLoadReg != hwParser.cmdList.end() ? (MI_LOAD_REGISTER_IMM *)*itorMiLoadReg : nullptr;

    auto itorPipeControl = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *pipeControl = itorPipeControl != hwParser.cmdList.end() ? (PIPE_CONTROL *)*itorPipeControl : nullptr;

    auto itorWalker = find<GPGPU_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto *walker = itorWalker != hwParser.cmdList.end() ? (GPGPU_WALKER *)*itorWalker : nullptr;

    auto itorMediaStateFlush2 = find<MEDIA_STATE_FLUSH *>(itorWalker, hwParser.cmdList.end());
    auto *msf2 = itorMediaStateFlush2 != hwParser.cmdList.end() ? (MEDIA_STATE_FLUSH *)*itorMediaStateFlush2 : nullptr;

    auto itorArbCheck2 = find<MI_ARB_CHECK *>(itorWalker, hwParser.cmdList.end());
    auto *arbCheck2 = itorArbCheck2 != hwParser.cmdList.end() ? (MI_ARB_CHECK *)*itorArbCheck2 : nullptr;

    if (msf)
        msfOffset = ptrDiff(msf, slb->getCpuBase());

    if (arbCheck)
        miArbCheckOffset = ptrDiff(arbCheck, slb->getCpuBase());

    if (miAtomic)
        miAtomicOffset = ptrDiff(miAtomic, slb->getCpuBase());

    if (idLoad)
        mediaIDLoadOffset = ptrDiff(idLoad, slb->getCpuBase());

    if (miLoadReg)
        miLoadRegOffset = ptrDiff(miLoadReg, slb->getCpuBase());

    if (pipeControl)
        pipeControlOffset = ptrDiff(pipeControl, slb->getCpuBase());

    if (walker)
        gpgpuOffset = ptrDiff(walker, slb->getCpuBase());

    if (msf2)
        msfOffset2 = ptrDiff(msf2, slb->getCpuBase());

    if (arbCheck2)
        miArbCheckOffset2 = ptrDiff(arbCheck2, slb->getCpuBase());

    uint32_t *slbBuffer = (uint32_t *)slb->getCpuBase();
    uint32_t secondLevelBatchOffset = 0;
    uint32_t InterfaceDescriptorOffset = 3;
    uint32_t SIMDSize = 16;
    uint32_t TotalLocalWorkSize = 24;
    uint3 DimSize = {6, 4, 1};
    uint3 StartPoint = {4, 4, 0};
    uint32_t NumberOfHWThreadsPerWG = 3;
    uint32_t IndirectPayloadSize = 10;
    uint32_t IOHoffset = 256;

    SchedulerSimulation<FamilyType>::patchGpGpuWalker(secondLevelBatchOffset, slbBuffer, InterfaceDescriptorOffset, SIMDSize, TotalLocalWorkSize, DimSize, StartPoint, NumberOfHWThreadsPerWG, IndirectPayloadSize, IOHoffset);

    size_t commandsSize = pDevQueueHw->getMinimumSlbSize() + pDevQueueHw->getWaCommandsSize();

    // Parse again
    LinearStream slbTested(slbBuffer, commandsSize);
    hwParser.cmdList.clear();
    slbTested.getSpace(commandsSize);
    hwParser.parseCommands<FamilyType>(slbTested, 0);

    itorMediaStateFlush = find<MEDIA_STATE_FLUSH *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    msf = (MEDIA_STATE_FLUSH *)*itorMediaStateFlush;

    EXPECT_EQ((void *)slb->getCpuBase(), (void *)msf);

    itorArbCheck = find<MI_ARB_CHECK *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    arbCheck = itorArbCheck != hwParser.cmdList.end() ? (MI_ARB_CHECK *)*itorArbCheck : nullptr;

    itorMiAtomic = find<MI_ATOMIC *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    miAtomic = itorMiAtomic != hwParser.cmdList.end() ? (MI_ATOMIC *)*itorMiAtomic : nullptr;

    itorIDLoad = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    idLoad = itorIDLoad != hwParser.cmdList.end() ? (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorIDLoad : nullptr;

    itorMiLoadReg = find<MI_LOAD_REGISTER_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    miLoadReg = itorMiLoadReg != hwParser.cmdList.end() ? (MI_LOAD_REGISTER_IMM *)*itorMiLoadReg : nullptr;

    itorPipeControl = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    pipeControl = itorPipeControl != hwParser.cmdList.end() ? (PIPE_CONTROL *)*itorPipeControl : nullptr;

    itorWalker = find<GPGPU_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    walker = itorWalker != hwParser.cmdList.end() ? (GPGPU_WALKER *)*itorWalker : nullptr;

    itorMediaStateFlush2 = find<MEDIA_STATE_FLUSH *>(itorWalker, hwParser.cmdList.end());
    msf2 = itorMediaStateFlush2 != hwParser.cmdList.end() ? (MEDIA_STATE_FLUSH *)*itorMediaStateFlush2 : nullptr;

    itorArbCheck2 = find<MI_ARB_CHECK *>(itorWalker, hwParser.cmdList.end());
    arbCheck2 = itorArbCheck2 != hwParser.cmdList.end() ? (MI_ARB_CHECK *)*itorArbCheck2 : nullptr;

    if (msf)
        msfOffsetAfter = ptrDiff(msf, slbTested.getCpuBase());

    if (arbCheck)
        miArbCheckOffsetAfter = ptrDiff(arbCheck, slbTested.getCpuBase());

    if (miAtomic)
        miAtomicOffsetAfter = ptrDiff(miAtomic, slbTested.getCpuBase());

    if (idLoad)
        mediaIDLoadOffsetAfter = ptrDiff(idLoad, slbTested.getCpuBase());

    if (miLoadReg)
        miLoadRegOffsetAfter = ptrDiff(miLoadReg, slbTested.getCpuBase());

    if (pipeControl)
        pipeControlOffsetAfter = ptrDiff(pipeControl, slbTested.getCpuBase());

    if (walker)
        gpgpuOffsetAfter = ptrDiff(walker, slbTested.getCpuBase());

    if (msf2)
        msfOffsetAfter2 = ptrDiff(msf2, slbTested.getCpuBase());

    if (arbCheck2)
        miArbCheckOffsetAfter2 = ptrDiff(arbCheck2, slbTested.getCpuBase());

    EXPECT_EQ(msfOffset, msfOffsetAfter);
    EXPECT_EQ(miArbCheckOffset, miArbCheckOffsetAfter);
    EXPECT_EQ(miAtomicOffset, miAtomicOffsetAfter);
    EXPECT_EQ(mediaIDLoadOffset, mediaIDLoadOffsetAfter);
    EXPECT_EQ(miLoadRegOffset, miLoadRegOffsetAfter);
    EXPECT_EQ(pipeControlOffset, pipeControlOffsetAfter);
    EXPECT_EQ(gpgpuOffset, gpgpuOffsetAfter);
    EXPECT_EQ(msfOffset2, msfOffsetAfter2);
    EXPECT_EQ(miArbCheckOffset2, miArbCheckOffsetAfter2);

    if (walker) {
        EXPECT_EQ(InterfaceDescriptorOffset, walker->getInterfaceDescriptorOffset());
        EXPECT_EQ(NumberOfHWThreadsPerWG, walker->getThreadWidthCounterMaximum());

        EXPECT_EQ(16u, SIMDSize);
        typename GPGPU_WALKER::SIMD_SIZE simd = GPGPU_WALKER::SIMD_SIZE::SIMD_SIZE_SIMD16;
        EXPECT_EQ(simd, walker->getSimdSize());

        EXPECT_EQ(StartPoint.x, walker->getThreadGroupIdStartingX());
        EXPECT_EQ(StartPoint.y, walker->getThreadGroupIdStartingY());
        //EXPECT_EQ(StartPoint.z, walker->GetThreadGroupIdStartingZ());

        EXPECT_EQ(DimSize.x, walker->getThreadGroupIdXDimension());
        EXPECT_EQ(DimSize.y, walker->getThreadGroupIdYDimension());
        //EXPECT_EQ(DimSize.z, walker->getThreadGroupIdZDimension());

        uint32_t mask = (1 << (TotalLocalWorkSize % SIMDSize)) - 1;
        if (mask == 0)
            mask = ~0;
        uint32_t yMask = 0xffffffff;

        EXPECT_EQ(mask, walker->getRightExecutionMask());
        EXPECT_EQ(yMask, walker->getBottomExecutionMask());

        EXPECT_EQ(IndirectPayloadSize, walker->getIndirectDataLength());

        EXPECT_EQ(IOHoffset, walker->getIndirectDataStartAddress());
    } else {
        EXPECT_TRUE(false) << "GPGPU_WALKER commandnot found, patchGpGpuWalker could have corrupted prepopulated commands\n";
    }

    delete pDevQueueHw;
}
