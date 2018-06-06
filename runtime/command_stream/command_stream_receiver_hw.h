/*
 * Copyright (c) 2018, Intel Corporation
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

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/dirty_state_helpers.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/options.h"

namespace OCLRT {
template <typename GfxFamily>
class DeviceCommandStreamReceiver;

template <typename GfxFamily>
class CommandStreamReceiverHw : public CommandStreamReceiver {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;

  public:
    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn) {
        return new CommandStreamReceiverHw<GfxFamily>(hwInfoIn);
    }

    CommandStreamReceiverHw(const HardwareInfo &hwInfoIn);

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency) override;

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags) override;

    void flushBatchedSubmissions() override;

    void addPipeControl(LinearStream &commandStream, bool dcFlush) override;
    int getRequiredPipeControlSize() const;

    static void addBatchBufferEnd(LinearStream &commandStream, void **patchLocation);
    void addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress);
    static void alignToCacheLine(LinearStream &commandStream);

    size_t getRequiredCmdStreamSize(const DispatchFlags &dispatchFlags);
    size_t getRequiredCmdStreamSizeAligned(const DispatchFlags &dispatchFlags);
    size_t getRequiredCmdSizeForPreamble() const;
    size_t getCmdSizeForPreemption(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForL3Config() const;
    size_t getCmdSizeForPipelineSelect() const;
    size_t getCmdSizeForCoherency();
    size_t getCmdSizeForMediaSampler(bool mediaSamplerRequired) const;
    void programCoherency(LinearStream &csr, DispatchFlags &dispatchFlags);

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override;
    const HardwareInfo &peekHwInfo() const { return hwInfo; }

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

  protected:
    void programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPreamble(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programPipelineSelect(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programMediaSampler(LinearStream &csr, DispatchFlags &dispatchFlags);
    virtual void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags);
    virtual void initPageTableManagerRegisters(LinearStream &csr){};

    void addPipeControlWA(LinearStream &commandStream, bool flushDC);
    void addDcFlushToPipeControl(typename GfxFamily::PIPE_CONTROL *pCmd, bool flushDC);
    void addClearSLMWorkAround(typename GfxFamily::PIPE_CONTROL *pCmd);
    PIPE_CONTROL *addPipeControlCmd(LinearStream &commandStream);

    uint64_t getScratchPatchAddress();

    static void emitNoop(LinearStream &commandStream, size_t bytesToUpdate);

    HeapDirtyState dshState;
    HeapDirtyState iohState;
    HeapDirtyState sshState;

    const HardwareInfo &hwInfo;
    CsrSizeRequestFlags csrSizeRequestFlags = {};

    std::unique_ptr<KmdNotifyHelper> kmdNotifyHelper;
};

} // namespace OCLRT
