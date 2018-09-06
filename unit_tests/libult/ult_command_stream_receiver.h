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

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/mocks/mock_experimental_command_buffer.h"
#include <map>

namespace OCLRT {

class GmmPageTableMngr;

template <typename GfxFamily>
class UltCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily> {
    using BaseClass = CommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::dshState;
    using BaseClass::hwInfo;
    using BaseClass::indirectHeap;
    using BaseClass::iohState;
    using BaseClass::programPreamble;
    using BaseClass::sshState;
    using BaseClass::CommandStreamReceiver::commandStream;
    using BaseClass::CommandStreamReceiver::disableL3Cache;
    using BaseClass::CommandStreamReceiver::dispatchMode;
    using BaseClass::CommandStreamReceiver::executionEnvironment;
    using BaseClass::CommandStreamReceiver::experimentalCmdBuffer;
    using BaseClass::CommandStreamReceiver::flushStamp;
    using BaseClass::CommandStreamReceiver::isPreambleSent;
    using BaseClass::CommandStreamReceiver::lastMediaSamplerConfig;
    using BaseClass::CommandStreamReceiver::lastPreemptionMode;
    using BaseClass::CommandStreamReceiver::lastSentCoherencyRequest;
    using BaseClass::CommandStreamReceiver::lastSentL3Config;
    using BaseClass::CommandStreamReceiver::lastSentThreadArbitrationPolicy;
    using BaseClass::CommandStreamReceiver::lastVmeSubslicesConfig;
    using BaseClass::CommandStreamReceiver::latestFlushedTaskCount;
    using BaseClass::CommandStreamReceiver::latestSentStatelessMocsConfig;
    using BaseClass::CommandStreamReceiver::mediaVfeStateDirty;
    using BaseClass::CommandStreamReceiver::requiredThreadArbitrationPolicy;
    using BaseClass::CommandStreamReceiver::submissionAggregator;
    using BaseClass::CommandStreamReceiver::taskCount;
    using BaseClass::CommandStreamReceiver::taskLevel;
    using BaseClass::CommandStreamReceiver::timestampPacketWriteEnabled;

    UltCommandStreamReceiver(const UltCommandStreamReceiver &) = delete;
    UltCommandStreamReceiver &operator=(const UltCommandStreamReceiver &) = delete;

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
        return new UltCommandStreamReceiver<GfxFamily>(hwInfoIn, executionEnvironment);
    }

    UltCommandStreamReceiver(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment) : BaseClass(hwInfoIn, executionEnvironment) {
        this->storeMakeResidentAllocations = false;
        if (hwInfoIn.capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
            tempPreemptionLocation = new GraphicsAllocation(nullptr, 0);
            this->preemptionCsrAllocation = tempPreemptionLocation;
        }
    }

    virtual MemoryManager *createMemoryManager(bool enable64kbPages) override {
        memoryManager = new OsAgnosticMemoryManager(enable64kbPages);
        return memoryManager;
    }

    virtual GmmPageTableMngr *createPageTableManager() override {
        createPageTableManagerCalled = true;
        return nullptr;
    }

    void overrideCsrSizeReqFlags(CsrSizeRequestFlags &flags) { this->csrSizeRequestFlags = flags; }
    bool isPreambleProgrammed() const { return this->isPreambleSent; }
    bool isGSBAFor32BitProgrammed() const { return this->GSBAFor32BitProgrammed; }
    bool isMediaVfeStateDirty() const { return this->mediaVfeStateDirty; }
    bool isLastVmeSubslicesConfig() const { return this->lastVmeSubslicesConfig; }
    uint32_t getLastSentL3Config() const { return this->lastSentL3Config; }
    int8_t getLastSentCoherencyRequest() const { return this->lastSentCoherencyRequest; }
    int8_t getLastMediaSamplerConfig() const { return this->lastMediaSamplerConfig; }
    PreemptionMode getLastPreemptionMode() const { return this->lastPreemptionMode; }
    uint32_t getLatestSentStatelessMocsConfig() const { return this->latestSentStatelessMocsConfig; }

    virtual ~UltCommandStreamReceiver() override;
    GraphicsAllocation *getTagAllocation() { return tagAllocation; }
    GraphicsAllocation *getPreemptionCsrAllocation() {
        return this->preemptionCsrAllocation;
    }
    using SamplerCacheFlushState = CommandStreamReceiver::SamplerCacheFlushState;
    SamplerCacheFlushState peekSamplerCacheFlushRequired() const { return this->samplerCacheFlushRequired; }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        if (storeMakeResidentAllocations) {
            std::map<GraphicsAllocation *, uint32_t>::iterator it = makeResidentAllocations.find(&gfxAllocation);
            if (it == makeResidentAllocations.end()) {
                std::pair<std::map<GraphicsAllocation *, uint32_t>::iterator, bool> result;
                result = makeResidentAllocations.insert(std::pair<GraphicsAllocation *, uint32_t>(&gfxAllocation, 1));
                DEBUG_BREAK_IF(!result.second);
            } else {
                makeResidentAllocations[&gfxAllocation]++;
            }
        }
        BaseClass::makeResident(gfxAllocation);
    }

    bool isMadeResident(GraphicsAllocation *graphicsAllocation) {
        return makeResidentAllocations.find(graphicsAllocation) != makeResidentAllocations.end();
    }

    std::map<GraphicsAllocation *, uint32_t> makeResidentAllocations;
    bool storeMakeResidentAllocations;

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override {
        CommandStreamReceiverHw<GfxFamily>::activateAubSubCapture(dispatchInfo);
        activateAubSubCaptureCalled = true;
    }
    void flushBatchedSubmissions() override {
        CommandStreamReceiverHw<GfxFamily>::flushBatchedSubmissions();
        flushBatchedSubmissionsCalled = true;
    }
    void initProgrammingFlags() override {
        CommandStreamReceiverHw<GfxFamily>::initProgrammingFlags();
        initProgrammingFlagsCalled = true;
    }

    bool createPageTableManagerCalled = false;
    bool activateAubSubCaptureCalled = false;
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;

  protected:
    using BaseClass::CommandStreamReceiver::memoryManager;
    using BaseClass::CommandStreamReceiver::tagAddress;
    using BaseClass::CommandStreamReceiver::tagAllocation;
    using BaseClass::CommandStreamReceiver::waitForTaskCountAndCleanAllocationList;

    GraphicsAllocation *tempPreemptionLocation = nullptr;
};

template <typename GfxFamily>
UltCommandStreamReceiver<GfxFamily>::~UltCommandStreamReceiver() {
    if (tempPreemptionLocation) {
        this->setPreemptionCsrAllocation(nullptr);
        delete tempPreemptionLocation;
    }
}

} // namespace OCLRT
