/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/dirty_state_helpers.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"

namespace NEO {
template <typename GfxFamily>
class DeviceCommandStreamReceiver;

template <typename GfxFamily>
class CommandStreamReceiverHw : public CommandStreamReceiver {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;

  public:
    static CommandStreamReceiver *create(ExecutionEnvironment &executionEnvironment) {
        return new CommandStreamReceiverHw<GfxFamily>(executionEnvironment);
    }

    CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment);

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override;

    void flushBatchedSubmissions() override;

    static void addBatchBufferEnd(LinearStream &commandStream, void **patchLocation);
    void addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary);
    static void alignToCacheLine(LinearStream &commandStream);

    size_t getRequiredStateBaseAddressSize() const;
    size_t getRequiredCmdStreamSize(const DispatchFlags &dispatchFlags, Device &device);
    size_t getRequiredCmdStreamSizeAligned(const DispatchFlags &dispatchFlags, Device &device);
    size_t getRequiredCmdSizeForPreamble(Device &device) const;
    size_t getCmdSizeForPreemption(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForL3Config() const;
    size_t getCmdSizeForPipelineSelect() const;
    size_t getCmdSizeForComputeMode();
    size_t getCmdSizeForMediaSampler(bool mediaSamplerRequired) const;
    void programComputeMode(LinearStream &csr, DispatchFlags &dispatchFlags);

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

    void blitBuffer(Buffer &dstBuffer, Buffer &srcBuffer, uint64_t sourceSize, CsrDependencies &csrDependencies) override;

  protected:
    using CommandStreamReceiver::osContext;

    void programPreemption(LinearStream &csr, Device &device, DispatchFlags &dispatchFlags);
    void programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPreamble(LinearStream &csr, Device &device, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPipelineSelect(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programMediaSampler(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programStateSip(LinearStream &cmdStream, Device &device);
    void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags);
    virtual void initPageTableManagerRegisters(LinearStream &csr){};

    void addClearSLMWorkAround(typename GfxFamily::PIPE_CONTROL *pCmd);
    PIPE_CONTROL *addPipeControlCmd(LinearStream &commandStream);
    size_t getSshHeapSize();

    uint64_t getScratchPatchAddress();
    void createScratchSpaceController();

    static void emitNoop(LinearStream &commandStream, size_t bytesToUpdate);

    bool detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const;

    HeapDirtyState dshState;
    HeapDirtyState iohState;
    HeapDirtyState sshState;

    CsrSizeRequestFlags csrSizeRequestFlags = {};
    bool localMemoryEnabled;
};

} // namespace NEO
