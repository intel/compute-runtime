/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"

namespace NEO {

struct UltCommandStreamReceiverTest
    : public ClDeviceFixture,
      public ClHardwareParse,
      ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
        ClHardwareParse::setUp();

        cmdBuffer = alignedMalloc(sizeStream, alignmentStream);
        dshBuffer = alignedMalloc(sizeStream, alignmentStream);
        iohBuffer = alignedMalloc(sizeStream, alignmentStream);
        sshBuffer = alignedMalloc(sizeStream, alignmentStream);

        ASSERT_NE(nullptr, cmdBuffer);
        ASSERT_NE(nullptr, dshBuffer);
        ASSERT_NE(nullptr, iohBuffer);
        ASSERT_NE(nullptr, sshBuffer);

        initHeaps();
        auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
        flushTaskFlags.threadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();

        pDevice->getGpgpuCommandStreamReceiver().setupContext(*pDevice->getDefaultEngine().osContext);

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();

        auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        this->heaplessStateEnabled = compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled);
    }

    void initHeaps() {
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
    }

    void cleanupHeaps() {
        delete dsh.getGraphicsAllocation();
        delete ioh.getGraphicsAllocation();
        delete ssh.getGraphicsAllocation();
        delete commandStream.getGraphicsAllocation();
    }

    void TearDown() override {
        pDevice->getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
        cleanupHeaps();

        alignedFree(sshBuffer);
        alignedFree(iohBuffer);
        alignedFree(dshBuffer);
        alignedFree(cmdBuffer);
        ClHardwareParse::tearDown();
        ClDeviceFixture::tearDown();
    }

    template <typename GfxFamily, typename CommandStreamReceiverType>
    CompletionStamp flushTaskMethod(CommandStreamReceiverType &commandStreamReceiver, LinearStream &commandStream, size_t commandStreamStart,
                                    const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                                    TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) {

        return commandStreamReceiver.flushTask(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }

    template <typename CommandStreamReceiverType>
    CompletionStamp flushTask(CommandStreamReceiverType &commandStreamReceiver,
                              bool block = false,
                              size_t startOffset = 0,
                              bool requiresCoherency = false,
                              bool lowPriority = false) {

        flushTaskFlags.blocking = block;
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
    SubmissionStatus flushSmallTask(CommandStreamReceiverType &commandStreamReceiver,
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
        if (debugManager.flags.ForceSLML3Config.get()) {
            slmUsed = true;
        }

        uint32_t l3Config = PreambleHelper<GfxFamily>::getL3Config(*defaultHwInfo, slmUsed);

        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<GfxFamily>();
        commandStreamReceiver.isPreambleSent = true;
        commandStreamReceiver.isEnginePrologueSent = true;
        commandStreamReceiver.isStateSipSent = true;
        commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
        commandStreamReceiver.setMediaVFEStateDirty(false);
        commandStreamReceiver.stateComputeModeDirty = false;
        auto gmmHelper = pDevice->getGmmHelper();
        auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
        auto mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, isL1CacheEnabled);

        commandStreamReceiver.latestSentStatelessMocsConfig = mocsIndex;
        commandStreamReceiver.lastSentL3Config = l3Config;
        configureCSRHeapStatesToNonDirty<GfxFamily>();
        commandStreamReceiver.taskLevel = taskLevel;

        commandStreamReceiver.lastMediaSamplerConfig = 0;
        commandStreamReceiver.streamProperties.pipelineSelect.setPropertiesAll(true, false, false);
        commandStreamReceiver.streamProperties.stateComputeMode.setPropertiesAll(0, GrfConfig::defaultGrfNumber,
                                                                                 gfxCoreHelper.getDefaultThreadArbitrationPolicy(), pDevice->getPreemptionMode());
        commandStreamReceiver.streamProperties.frontEndState.setPropertiesAll(false, false, false);
    }

    template <typename GfxFamily>
    UltCommandStreamReceiver<GfxFamily> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<GfxFamily> &>(pDevice->getGpgpuCommandStreamReceiver());
    }

    DispatchFlags flushTaskFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    TaskCountType taskLevel = 42;
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

    const size_t sizeStream = 512;
    const size_t alignmentStream = 0x1000;
    bool heaplessStateEnabled = false;
};

template <typename CsrType>
struct UltCommandStreamReceiverTestWithCsr
    : public UltCommandStreamReceiverTest {
  public:
    void SetUp() override {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType>();

        UltCommandStreamReceiverTest::SetUp();
    }

    void TearDown() override {
        UltCommandStreamReceiverTest::TearDown();
    }
};

template <template <typename> class CsrType>
struct UltCommandStreamReceiverTestWithCsrT
    : public UltCommandStreamReceiverTest {
  public:
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType<FamilyType>>();
        beforeDefStateFlag = debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.get();
        debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.set(1);

        UltCommandStreamReceiverTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        UltCommandStreamReceiverTest::TearDown();
        debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.set(beforeDefStateFlag);
    }

  private:
    int32_t beforeDefStateFlag;
};
} // namespace NEO
