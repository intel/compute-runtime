/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_cmds.h"
#include "core/helpers/hw_info.h"
#include "core/helpers/options.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/dirty_state_helpers.h"

namespace NEO {
template <typename GfxFamily>
class DeviceCommandStreamReceiver;

template <typename GfxFamily>
class CommandStreamReceiverHw : public CommandStreamReceiver {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;

  public:
    static CommandStreamReceiver *create(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
        return new CommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex);
    }

    CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override;

    bool flushBatchedSubmissions() override;

    static void addBatchBufferEnd(LinearStream &commandStream, void **patchLocation);
    void addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary);
    static void alignToCacheLine(LinearStream &commandStream);

    size_t getRequiredStateBaseAddressSize() const;
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

    void programComputeMode(LinearStream &csr, DispatchFlags &dispatchFlags);
    void adjustComputeMode(LinearStream &csr, DispatchFlags &dispatchFlags, void *const stateComputeMode);

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) override;
    const HardwareInfo &peekHwInfo() const { return *executionEnvironment.getHardwareInfo(); }

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

    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking) override;

    bool isMultiOsContextCapable() const override;

  protected:
    void programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPreamble(LinearStream &csr, Device &device, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPipelineSelect(LinearStream &csr, PipelineSelectArgs &pipelineSelectArgs);
    void programEpilogue(LinearStream &csr, void **batchBufferEndLocation, DispatchFlags &dispatchFlags);
    void programEpliogueCommands(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programMediaSampler(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programStateSip(LinearStream &cmdStream, Device &device);
    void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t maxFrontEndThreads);
    void programStallingPipeControlForBarrier(LinearStream &cmdStream, DispatchFlags &dispatchFlags);
    void programEngineModeCommands(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programEngineModeEpliogue(LinearStream &csr, const DispatchFlags &dispatchFlags);

    void addClearSLMWorkAround(typename GfxFamily::PIPE_CONTROL *pCmd);
    PIPE_CONTROL *addPipeControlCmd(LinearStream &commandStream);
    PIPE_CONTROL *addPipeControlBeforeStateBaseAddress(LinearStream &commandStream);
    size_t getSshHeapSize();

    uint64_t getScratchPatchAddress();
    void createScratchSpaceController();

    static void emitNoop(LinearStream &commandStream, size_t bytesToUpdate);

    bool detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const;

    HeapDirtyState dshState;
    HeapDirtyState iohState;
    HeapDirtyState sshState;

    CsrSizeRequestFlags csrSizeRequestFlags = {};
};

} // namespace NEO
