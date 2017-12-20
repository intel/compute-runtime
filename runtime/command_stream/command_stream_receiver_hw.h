/*
 * Copyright (c) 2017, Intel Corporation
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

    CommandStreamReceiverHw(const HardwareInfo &hwInfoIn) : hwInfo(hwInfoIn) {}

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer *allocationsForResidency) override {
        return flushStamp->peekStamp();
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const LinearStream &dsh, const LinearStream &ih,
                              const LinearStream &ioh, const LinearStream &ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags) override;

    void flushBatchedSubmissions() override;

    void addPipeControl(LinearStream &commandStream, bool dcFlush) override;
    int getRequiredPipeControlSize();

    static void addBatchBufferEnd(LinearStream &commandStream, void **patchLocation);
    static void addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress);
    static void alignToCacheLine(LinearStream &commandStream);

    size_t getRequiredCsrSize();
    size_t getCmdSizeForCoherency();
    void programCoherency(LinearStream &csr, DispatchFlags &dispatchFlags);

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait) override;

  protected:
    void programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    void programMediaSampler(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programPreamble(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config);
    virtual void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags);
    virtual void initPageTableManagerRegisters(LinearStream &csr){};

    void addPipeControlWA(LinearStream &commandStream, bool flushDC);
    void addDcFlushToPipeControl(typename GfxFamily::PIPE_CONTROL *pCmd, bool flushDC);
    PIPE_CONTROL *addPipeControlCmd(LinearStream &commandStream);

    uint64_t getScratchPatchAddress();

    static void emitNoop(LinearStream &commandStream, size_t bytesToUpdate);

    HeapDirtyState dshState;
    HeapDirtyState ihState;
    HeapDirtyState iohState;
    HeapDirtyState sshState;

    const HardwareInfo &hwInfo;
    CsrSizeRequestFlags csrSizeRequestFlags = {};
};

template <typename GfxFamily>
size_t getSizeRequiredPreambleCS(const HardwareInfo &hwInfo);
} // namespace OCLRT
