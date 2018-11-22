/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/mocks/mock_experimental_command_buffer.h"
#include <map>
#include <memory>

namespace OCLRT {

class GmmPageTableMngr;

template <typename GfxFamily>
class UltCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily>, public NonCopyableOrMovableClass {
    using BaseClass = CommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::deviceIndex;
    using BaseClass::dshState;
    using BaseClass::getScratchPatchAddress;
    using BaseClass::hwInfo;
    using BaseClass::indirectHeap;
    using BaseClass::iohState;
    using BaseClass::programPreamble;
    using BaseClass::programStateSip;
    using BaseClass::sshState;
    using BaseClass::CommandStreamReceiver::cleanupResources;
    using BaseClass::CommandStreamReceiver::commandStream;
    using BaseClass::CommandStreamReceiver::disableL3Cache;
    using BaseClass::CommandStreamReceiver::dispatchMode;
    using BaseClass::CommandStreamReceiver::executionEnvironment;
    using BaseClass::CommandStreamReceiver::experimentalCmdBuffer;
    using BaseClass::CommandStreamReceiver::flushStamp;
    using BaseClass::CommandStreamReceiver::GSBAFor32BitProgrammed;
    using BaseClass::CommandStreamReceiver::isPreambleSent;
    using BaseClass::CommandStreamReceiver::isStateSipSent;
    using BaseClass::CommandStreamReceiver::lastMediaSamplerConfig;
    using BaseClass::CommandStreamReceiver::lastPreemptionMode;
    using BaseClass::CommandStreamReceiver::lastSentCoherencyRequest;
    using BaseClass::CommandStreamReceiver::lastSentL3Config;
    using BaseClass::CommandStreamReceiver::lastSentThreadArbitrationPolicy;
    using BaseClass::CommandStreamReceiver::lastVmeSubslicesConfig;
    using BaseClass::CommandStreamReceiver::latestFlushedTaskCount;
    using BaseClass::CommandStreamReceiver::latestSentStatelessMocsConfig;
    using BaseClass::CommandStreamReceiver::mediaVfeStateDirty;
    using BaseClass::CommandStreamReceiver::requiredScratchSize;
    using BaseClass::CommandStreamReceiver::requiredThreadArbitrationPolicy;
    using BaseClass::CommandStreamReceiver::samplerCacheFlushRequired;
    using BaseClass::CommandStreamReceiver::scratchSpaceController;
    using BaseClass::CommandStreamReceiver::stallingPipeControlOnNextFlushRequired;
    using BaseClass::CommandStreamReceiver::submissionAggregator;
    using BaseClass::CommandStreamReceiver::taskCount;
    using BaseClass::CommandStreamReceiver::taskLevel;
    using BaseClass::CommandStreamReceiver::timestampPacketAllocator;
    using BaseClass::CommandStreamReceiver::timestampPacketWriteEnabled;
    using BaseClass::CommandStreamReceiver::waitForTaskCountAndCleanAllocationList;

    virtual ~UltCommandStreamReceiver() override {
        if (tempPreemptionLocation) {
            this->setPreemptionCsrAllocation(nullptr);
        }
    }

    UltCommandStreamReceiver(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment) : BaseClass(hwInfoIn, executionEnvironment) {
        if (hwInfoIn.capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
            tempPreemptionLocation = std::make_unique<GraphicsAllocation>(nullptr, 0, 0, 0, 1u, false);
            this->preemptionCsrAllocation = tempPreemptionLocation.get();
        }
    }

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
        return new UltCommandStreamReceiver<GfxFamily>(hwInfoIn, executionEnvironment);
    }

    virtual MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) override {
        return new OsAgnosticMemoryManager(enable64kbPages, enableLocalMemory, executionEnvironment);
    }

    virtual GmmPageTableMngr *createPageTableManager() override {
        createPageTableManagerCalled = true;
        return nullptr;
    }

    size_t getPreferredTagPoolSize() const override {
        return BaseClass::getPreferredTagPoolSize() + 1;
    }

    void overrideCsrSizeReqFlags(CsrSizeRequestFlags &flags) { this->csrSizeRequestFlags = flags; }
    GraphicsAllocation *getPreemptionCsrAllocation() const { return this->preemptionCsrAllocation; }

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

    bool isMadeResident(GraphicsAllocation *graphicsAllocation) const {
        return makeResidentAllocations.find(graphicsAllocation) != makeResidentAllocations.end();
    }

    std::map<GraphicsAllocation *, uint32_t> makeResidentAllocations;
    bool storeMakeResidentAllocations = false;

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
    std::unique_ptr<GraphicsAllocation> tempPreemptionLocation;
};

} // namespace OCLRT
