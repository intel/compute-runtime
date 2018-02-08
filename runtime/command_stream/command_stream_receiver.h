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
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/thread_arbitration_policy.h"
#include "runtime/command_stream/submissions_aggregator.h"
#include "runtime/helpers/completion_stamp.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/command_stream/csr_definitions.h"
#include <cstddef>
#include <cstdint>

namespace OCLRT {
class Device;
class EventBuilder;
class LinearStream;
class MemoryManager;
class OSInterface;
class GraphicsAllocation;

class CommandStreamReceiver {
  public:
    enum DispatchMode {
        DeviceDefault = 0,          //default for given device
        ImmediateDispatch,          //everything is submitted to the HW immediately
        AdaptiveDispatch,           //dispatching is handled to async thread, which combines batch buffers basing on load (not implemented)
        BatchedDispatchWithCounter, //dispatching is batched, after n commands there is implicit flush (not implemented)
        BatchedDispatch             // dispatching is batched, explicit clFlush is required
    };

    enum class SamplerCacheFlushState {
        samplerCacheFlushNotRequired,
        samplerCacheFlushBefore, //add sampler cache flush before Walker with redescribed image
        samplerCacheFlushAfter   //add sampler cache flush after Walker with redescribed image
    };
    CommandStreamReceiver();
    virtual ~CommandStreamReceiver();

    virtual FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                                      const LinearStream &dsh, const LinearStream &ih,
                                      const LinearStream &ioh, const LinearStream &ssh,
                                      uint32_t taskLevel, DispatchFlags &dispatchFlags) = 0;

    virtual void flushBatchedSubmissions() = 0;

    virtual void makeCoherent(void *address, size_t length){};
    virtual void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    void makeSurfacePackNonResident(ResidencyContainer *allocationsForResidency);
    virtual void processResidency(ResidencyContainer *allocationsForResidency) {}
    virtual void processEviction();

    virtual void addPipeControl(LinearStream &commandStream, bool dcFlush) = 0;

    MemoryManager *getMemoryManager();
    virtual MemoryManager *createMemoryManager(bool enable64kbPages) { return nullptr; }
    void setMemoryManager(MemoryManager *mm);

    GraphicsAllocation *createAllocationAndHandleResidency(const void *address, size_t size, bool addToDefferFreeList = true);
    void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationType);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() { return osInterface.get(); };

    MOCKABLE_VIRTUAL void setTagAllocation(GraphicsAllocation *allocation);
    volatile uint32_t *getTagAddress() const { return tagAddress; }

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait) { return true; };

    uint32_t peekTaskCount() const { return taskCount; }

    uint32_t peekTaskLevel() const { return taskLevel; }

    uint32_t peekLatestSentTaskCount() const { return latestSentTaskCount; }

    uint32_t peekLatestFlushedTaskCount() const { return latestFlushedTaskCount; }

    void overrideDispatchPolicy(CommandStreamReceiver::DispatchMode overrideValue) { this->dispatchMode = overrideValue; }

    virtual void overrideMediaVFEStateDirty(bool dirty) { mediaVfeStateDirty = dirty; }

    void setRequiredScratchSize(uint32_t newRequiredScratchSize);
    GraphicsAllocation *getScratchAllocation() { return scratchAllocation; }

    void setPreemptionCsrAllocation(GraphicsAllocation *allocation) { preemptionCsrAllocation = allocation; }

    void cleanupResources();

    void requestThreadArbitrationPolicy(uint32_t requiredPolicy) { this->requiredThreadArbitrationPolicy = requiredPolicy; }

    virtual void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait) = 0;
    MOCKABLE_VIRTUAL bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait);

    // returns size of block that needs to be reserved at the beginning of each instruction heap for CommandStreamReceiver
    MOCKABLE_VIRTUAL size_t getInstructionHeapCmdStreamReceiverReservedSize() const;

    // allows CommandStreamReceiver to prepopulate reserved block in instruction heap
    MOCKABLE_VIRTUAL void initializeInstructionHeapCmdStreamReceiverReservedBlock(LinearStream &ih) const;

    void setSamplerCacheFlushRequired(SamplerCacheFlushState value) { this->samplerCacheFlushRequired = value; }

  protected:
    // taskCount - # of tasks submitted
    uint32_t taskCount = 0;
    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<uint32_t> taskLevel{0};

    std::atomic<uint32_t> latestSentTaskCount{0};
    std::atomic<uint32_t> latestFlushedTaskCount{0};

    std::unique_ptr<FlushStampTracker> flushStamp;

    volatile uint32_t *tagAddress = nullptr;
    GraphicsAllocation *tagAllocation = nullptr;

    bool isPreambleSent = false;
    bool GSBAFor32BitProgrammed = false;
    bool mediaVfeStateDirty = true;
    bool lastVmeSubslicesConfig = false;

    uint32_t lastSentL3Config = 0;
    int8_t lastSentCoherencyRequest = -1;
    int8_t lastMediaSamplerConfig = -1;
    PreemptionMode lastPreemptionMode = PreemptionMode::Initial;
    uint32_t latestSentStatelessMocsConfig;

    LinearStream commandStream;

    uint32_t requiredThreadArbitrationPolicy = ThreadArbitrationPolicy::threadArbirtrationPolicyRoundRobin;
    uint32_t lastSentThreadAribtrationPolicy = ThreadArbitrationPolicy::threadArbitrationPolicyNotPresent;

    GraphicsAllocation *scratchAllocation = nullptr;
    GraphicsAllocation *preemptionCsrAllocation = nullptr;

    MemoryManager *memoryManager = nullptr;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<SubmissionAggregator> submissionAggregator;

    DispatchMode dispatchMode = ImmediateDispatch;
    bool disableL3Cache = 0;
    uint32_t requiredScratchSize = 0;
    uint64_t totalMemoryUsed = 0u;
    SamplerCacheFlushState samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
};

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(const HardwareInfo &hwInfoIn, bool withAubDump);
} // namespace OCLRT
