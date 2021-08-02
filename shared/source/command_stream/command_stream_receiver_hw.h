/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/dirty_state_helpers.h"
#include "shared/source/helpers/hw_info.h"

#include "hw_cmds.h"

namespace NEO {
template <typename GfxFamily>
class DeviceCommandStreamReceiver;
struct PipeControlArgs;

template <typename GfxFamily>
class CommandStreamReceiverHw : public CommandStreamReceiver {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;

  public:
    static CommandStreamReceiver *create(ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield) {
        return new CommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment,
                            uint32_t rootDeviceIndex,
                            const DeviceBitfield deviceBitfield);
    ~CommandStreamReceiverHw() override;

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override;

    void forcePipeControl(NEO::LinearStream &commandStreamCSR);

    bool flushBatchedSubmissions() override;
    void programHardwareContext(LinearStream &cmdStream) override;
    size_t getCmdsSizeForHardwareContext() const override;

    static void addBatchBufferEnd(LinearStream &commandStream, void **patchLocation);
    void programEndingCmd(LinearStream &commandStream, Device &device, void **patchLocation, bool directSubmissionEnabled);
    void addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary);
    static void alignToCacheLine(LinearStream &commandStream);

    size_t getRequiredStateBaseAddressSize(const Device &device) const;
    size_t getRequiredCmdStreamSize(const DispatchFlags &dispatchFlags, Device &device);
    size_t getRequiredCmdStreamSizeAligned(const DispatchFlags &dispatchFlags, Device &device);
    size_t getRequiredCmdSizeForPreamble(Device &device) const;
    size_t getCmdSizeForPreemption(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForEpilogue(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForEpilogueCommands(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForL3Config() const;
    size_t getCmdSizeForPipelineSelect() const;
    size_t getCmdSizeForComputeMode();
    size_t getCmdSizeForMediaSampler(bool mediaSamplerRequired) const;
    size_t getCmdSizeForEngineMode(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo);

    bool isComputeModeNeeded() const;
    bool isAdditionalPipeControlNeeded() const;
    bool isPipelineSelectAlreadyProgrammed() const;
    void programComputeMode(LinearStream &csr, DispatchFlags &dispatchFlags, const HardwareInfo &hwInfo);

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) override;
    const HardwareInfo &peekHwInfo() const;

    void collectStateBaseAddresPatchInfo(
        uint64_t commandBufferAddress,
        uint64_t commandOffset,
        const LinearStream &dsh,
        const LinearStream &ioh,
        const LinearStream &ssh,
        uint64_t generalStateBase);

    void resetKmdNotifyHelper(KmdNotifyHelper *newHelper);

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_HW;
    }

    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override;

    void flushTagUpdate() override;
    void flushNonKernelTask(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args, bool isWaitOnEvent, bool isStartOfDispatch, bool isEndOfDispatch) override;
    void flushMiFlushDW();
    void flushMiFlushDW(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData);
    void flushPipeControl();
    void flushPipeControl(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args);
    void flushSemaphoreWait(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args, bool isStartOfDispatch, bool isEndOfDispatch);
    void flushSmallTask(LinearStream &commandStreamTask,
                        size_t commandStreamStartTask);
    void flushHandler(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency);

    bool isUpdateTagFromWaitEnabled();
    void updateTagFromWait() override;

    bool isMultiOsContextCapable() const override;

    MemoryCompressionState getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const override;

    bool isDirectSubmissionEnabled() const override {
        return directSubmission.get() != nullptr;
    }

    bool isBlitterDirectSubmissionEnabled() const override {
        return blitterDirectSubmission.get() != nullptr;
    }

    virtual bool isKmdWaitModeActive() { return true; }

    bool initDirectSubmission(Device &device, OsContext &osContext) override;
    GraphicsAllocation *getClearColorAllocation() override;

    TagAllocatorBase *getTimestampPacketAllocator() override;

    void postInitFlagsSetup() override;

  protected:
    void programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPreamble(LinearStream &csr, Device &device, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPipelineSelect(LinearStream &csr, PipelineSelectArgs &pipelineSelectArgs);
    void programAdditionalPipelineSelect(LinearStream &csr, PipelineSelectArgs &pipelineSelectArgs, bool is3DPipeline);
    void programAdditionalStateBaseAddress(LinearStream &csr, typename GfxFamily::STATE_BASE_ADDRESS &cmd, Device &device);
    void programEpilogue(LinearStream &csr, Device &device, void **batchBufferEndLocation, DispatchFlags &dispatchFlags);
    void programEpliogueCommands(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programMediaSampler(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programPerDssBackedBuffer(LinearStream &scr, Device &device, DispatchFlags &dispatchFlags);
    void programStateSip(LinearStream &cmdStream, Device &device);
    void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t maxFrontEndThreads);
    void programStallingPipeControlForBarrier(LinearStream &cmdStream, DispatchFlags &dispatchFlags);
    void programEngineModeCommands(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programEngineModeEpliogue(LinearStream &csr, const DispatchFlags &dispatchFlags);

    void programEnginePrologue(LinearStream &csr);
    size_t getCmdSizeForPrologue() const;

    void addClearSLMWorkAround(typename GfxFamily::PIPE_CONTROL *pCmd);
    void addPipeControlCmd(LinearStream &commandStream, PipeControlArgs &args);
    void addPipeControlBeforeStateBaseAddress(LinearStream &commandStream);
    size_t getSshHeapSize();
    bool are4GbHeapsAvailable() const;

    uint64_t getScratchPatchAddress();
    void createScratchSpaceController();

    static void emitNoop(LinearStream &commandStream, size_t bytesToUpdate);

    bool detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const;
    bool checkPlatformSupportsNewResourceImplicitFlush() const;
    bool checkPlatformSupportsGpuIdleImplicitFlush() const;

    HeapDirtyState dshState;
    HeapDirtyState iohState;
    HeapDirtyState sshState;

    CsrSizeRequestFlags csrSizeRequestFlags = {};

    bool wasSubmittedToSingleSubdevice = false;

    std::unique_ptr<DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>> directSubmission;
    std::unique_ptr<DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>> blitterDirectSubmission;

    size_t cmdStreamStart = 0;
};

} // namespace NEO
