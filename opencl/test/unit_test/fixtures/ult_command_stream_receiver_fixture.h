/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"

namespace NEO {

struct UltCommandStreamReceiverTest
    : public ClDeviceFixture,
      public ClHardwareParse,
      ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::SetUp();
        ClHardwareParse::SetUp();

        size_t sizeStream = 512;
        size_t alignmentStream = 0x1000;
        cmdBuffer = alignedMalloc(sizeStream, alignmentStream);
        dshBuffer = alignedMalloc(sizeStream, alignmentStream);
        iohBuffer = alignedMalloc(sizeStream, alignmentStream);
        sshBuffer = alignedMalloc(sizeStream, alignmentStream);

        ASSERT_NE(nullptr, cmdBuffer);
        ASSERT_NE(nullptr, dshBuffer);
        ASSERT_NE(nullptr, iohBuffer);
        ASSERT_NE(nullptr, sshBuffer);

        commandStream.replaceBuffer(cmdBuffer, sizeStream);
        auto graphicsAllocation = new MockGraphicsAllocation(cmdBuffer, sizeStream);
        commandStream.replaceGraphicsAllocation(graphicsAllocation);

        dsh.replaceBuffer(dshBuffer, sizeStream);
        graphicsAllocation = new MockGraphicsAllocation(dshBuffer, sizeStream);
        dsh.replaceGraphicsAllocation(graphicsAllocation);

        ioh.replaceBuffer(iohBuffer, sizeStream);

        graphicsAllocation = new MockGraphicsAllocation(iohBuffer, sizeStream);
        ioh.replaceGraphicsAllocation(graphicsAllocation);

        ssh.replaceBuffer(sshBuffer, sizeStream);
        graphicsAllocation = new MockGraphicsAllocation(sshBuffer, sizeStream);
        ssh.replaceGraphicsAllocation(graphicsAllocation);

        flushTaskFlags.threadArbitrationPolicy = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily).getDefaultThreadArbitrationPolicy();

        pDevice->getGpgpuCommandStreamReceiver().setupContext(*pDevice->getDefaultEngine().osContext);
    }

    void TearDown() override {
        pDevice->getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
        delete dsh.getGraphicsAllocation();
        delete ioh.getGraphicsAllocation();
        delete ssh.getGraphicsAllocation();
        delete commandStream.getGraphicsAllocation();

        alignedFree(sshBuffer);
        alignedFree(iohBuffer);
        alignedFree(dshBuffer);
        alignedFree(cmdBuffer);
        ClHardwareParse::TearDown();
        ClDeviceFixture::TearDown();
    }

    template <typename CommandStreamReceiverType>
    CompletionStamp flushTask(CommandStreamReceiverType &commandStreamReceiver,
                              bool block = false,
                              size_t startOffset = 0,
                              bool requiresCoherency = false,
                              bool lowPriority = false) {

        flushTaskFlags.blocking = block;
        flushTaskFlags.requiresCoherency = requiresCoherency;
        flushTaskFlags.lowPriority = lowPriority;
        flushTaskFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

        return commandStreamReceiver.flushTask(
            commandStream,
            startOffset,
            &dsh,
            &ioh,
            &ssh,
            taskLevel,
            flushTaskFlags,
            *pDevice);
    }

    template <typename CommandStreamReceiverType>
    void flushSmallTask(CommandStreamReceiverType &commandStreamReceiver,
                        size_t startOffset = 0) {
        return commandStreamReceiver.flushSmallTask(
            commandStream,
            startOffset);
    }

    template <typename GfxFamily>
    void configureCSRHeapStatesToNonDirty() {
        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<GfxFamily>();
        commandStreamReceiver.dshState.updateAndCheck(&dsh);
        commandStreamReceiver.iohState.updateAndCheck(&ioh);
        commandStreamReceiver.sshState.updateAndCheck(&ssh);
    }

    template <typename GfxFamily>
    void configureCSRtoNonDirtyState(bool isL1CacheEnabled) {
        bool slmUsed = false;
        if (DebugManager.flags.ForceSLML3Config.get()) {
            slmUsed = true;
        }

        uint32_t L3Config = PreambleHelper<GfxFamily>::getL3Config(*defaultHwInfo, slmUsed);

        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<GfxFamily>();
        commandStreamReceiver.isPreambleSent = true;
        commandStreamReceiver.isEnginePrologueSent = true;
        commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
        commandStreamReceiver.setMediaVFEStateDirty(false);
        auto gmmHelper = pDevice->getGmmHelper();
        auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eDisplayCoreFamily);
        auto mocsIndex = hwHelper.getMocsIndex(*gmmHelper, true, isL1CacheEnabled);

        commandStreamReceiver.latestSentStatelessMocsConfig = mocsIndex;
        commandStreamReceiver.lastSentL3Config = L3Config;
        configureCSRHeapStatesToNonDirty<GfxFamily>();
        commandStreamReceiver.taskLevel = taskLevel;

        commandStreamReceiver.lastMediaSamplerConfig = 0;
        commandStreamReceiver.lastSentUseGlobalAtomics = false;
        commandStreamReceiver.streamProperties.stateComputeMode.setProperties(0, GrfConfig::DefaultGrfNumber,
                                                                              hwHelper.getDefaultThreadArbitrationPolicy(), *defaultHwInfo);
    }

    template <typename GfxFamily>
    UltCommandStreamReceiver<GfxFamily> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<GfxFamily> &>(pDevice->getGpgpuCommandStreamReceiver());
    }

    DispatchFlags flushTaskFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    uint32_t taskLevel = 42;
    LinearStream commandStream;
    IndirectHeap dsh = {nullptr};
    IndirectHeap ioh = {nullptr};
    IndirectHeap ssh = {nullptr};

    void *cmdBuffer = nullptr;
    void *dshBuffer = nullptr;
    void *iohBuffer = nullptr;
    void *sshBuffer = nullptr;

    uint32_t latestSentDcFlushTaskCount;
    uint32_t latestSentNonDcFlushTaskCount;
    uint32_t dcFlushRequiredTaskCount;
};
} // namespace NEO
