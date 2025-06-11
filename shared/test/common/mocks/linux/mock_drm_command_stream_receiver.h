/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

using namespace NEO;

template <typename GfxFamily>
class TestedDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
  public:
    using BaseClass = DrmCommandStreamReceiver<GfxFamily>;
    using BaseClass::drm;
    using BaseClass::exec;
    using BaseClass::execObjectsStorage;
    using BaseClass::residency;
    using BaseClass::useUserFenceWait;
    using BaseClass::vmBindAvailable;
    using CommandStreamReceiver::activePartitions;
    using CommandStreamReceiver::clearColorAllocation;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::completionFenceValuePointer;
    using CommandStreamReceiver::createPreemptionAllocation;
    using CommandStreamReceiver::flushStamp;
    using CommandStreamReceiver::getTagAddress;
    using CommandStreamReceiver::getTagAllocation;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::heaplessStateInitEnabled;
    using CommandStreamReceiver::heaplessStateInitialized;
    using CommandStreamReceiver::immWritePostSyncWriteOffset;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::makeResident;
    using CommandStreamReceiver::pushAllocationsForMakeResident;
    using CommandStreamReceiver::tagAddress;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::timeStampPostSyncWriteOffset;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;
    using CommandStreamReceiver::useNotifyEnableForPostSync;
    using CommandStreamReceiverHw<GfxFamily>::directSubmission;
    using CommandStreamReceiverHw<GfxFamily>::blitterDirectSubmission;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::lastSentSliceCount;

    TestedDrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                   const DeviceBitfield deviceBitfield)
        : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, 0, deviceBitfield) {
    }
    TestedDrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                   uint32_t rootDeviceIndex,
                                   const DeviceBitfield deviceBitfield)
        : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    void overrideDispatchPolicy(DispatchMode overrideValue) {
        this->dispatchMode = overrideValue;
    }

    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        makeNonResidentResult.called = true;
        makeNonResidentResult.allocation = &gfxAllocation;
        BaseClass::makeNonResident(gfxAllocation);
    }

    struct MakeResidentNonResidentResult {
        bool called = false;
        GraphicsAllocation *allocation = nullptr;
    };

    MakeResidentNonResidentResult makeNonResidentResult;

    SubmissionAggregator *peekSubmissionAggregator() const {
        return this->submissionAggregator.get();
    }

    ADDMETHOD(processResidency, SubmissionStatus, true, SubmissionStatus::success, (ResidencyContainer & allocationsForResidency, uint32_t handleId), (allocationsForResidency, handleId));

    ADDMETHOD(exec, int, true, 0, (const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index), (batchBuffer, vmHandleId, drmContextId, index));

    void overrideSubmissionAggregator(SubmissionAggregator *newSubmissionsAggregator) {
        this->submissionAggregator.reset(newSubmissionsAggregator);
    }

    bool createPreemptionAllocation() override {
        if (ultHwConfig.csrBaseCallCreatePreemption) {
            return CommandStreamReceiver::createPreemptionAllocation();
        } else {
            return ultHwConfig.csrCreatePreemptionReturnValue;
        }
    }

    void fillReusableAllocationsList() override {
        fillReusableAllocationsListCalled++;
    }

    struct WaitUserFenceResult {
        uint32_t called = 0u;
        TaskCountType waitValue = 0u;
        int returnValue = 0;
        bool callParent = true;
    };

    WaitUserFenceResult waitUserFenceResult;

    bool waitUserFence(TaskCountType waitValue, uint64_t hostAddress, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
        waitUserFenceResult.called++;
        waitUserFenceResult.waitValue = waitValue;

        if (waitUserFenceResult.callParent) {
            return BaseClass::waitUserFence(waitValue, hostAddress, timeout, userInterrupt, externalInterruptId, allocForInterruptWait);
        } else {
            return (waitUserFenceResult.returnValue == 0);
        }
    }

    ADDMETHOD(flushInternal, SubmissionStatus, true, SubmissionStatus::success, (const BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency), (batchBuffer, allocationsForResidency));

    void readBackAllocation(void *source) override {
        latestReadBackAddress = source;
        DrmCommandStreamReceiver<GfxFamily>::readBackAllocation(source);
    }

    void plugProxyDrm(DrmMock *proxyDrm) {
        this->drm = proxyDrm;
    }

    void stopDirectSubmission(bool blocking, bool needsLock) override {
        stopDirectSubmissionCalled = true;
    }

    void *latestReadBackAddress = nullptr;
    uint32_t fillReusableAllocationsListCalled = 0;
    bool stopDirectSubmissionCalled = false;
};

template <typename GfxFamily>
class TestedDrmCommandStreamReceiverWithFailingExec : public TestedDrmCommandStreamReceiver<GfxFamily> {
  public:
    TestedDrmCommandStreamReceiverWithFailingExec(ExecutionEnvironment &executionEnvironment,
                                                  const DeviceBitfield deviceBitfield)
        : TestedDrmCommandStreamReceiver<GfxFamily>(
              executionEnvironment,
              deviceBitfield) {
    }
    TestedDrmCommandStreamReceiverWithFailingExec(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield)
        : TestedDrmCommandStreamReceiver<GfxFamily>(executionEnvironment,
                                                    rootDeviceIndex,
                                                    deviceBitfield) {
    }

    int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
        return -1;
    }
};
