/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "opencl/source/aub/aub_center.h"
#include "opencl/source/command_stream/aub_command_stream_receiver.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];

template <typename BaseCSR>
CommandStreamReceiverWithAUBDump<BaseCSR>::CommandStreamReceiverWithAUBDump(const std::string &baseName, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex)
    : BaseCSR(executionEnvironment, rootDeviceIndex) {
    bool isAubManager = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter && executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter->getAubManager();
    bool isTbxMode = CommandStreamReceiverType::CSR_TBX == BaseCSR::getType();
    bool createAubCsr = (isAubManager && isTbxMode) ? false : true;
    if (createAubCsr) {
        aubCSR.reset(AUBCommandStreamReceiver::create(baseName, false, executionEnvironment, rootDeviceIndex));
        UNRECOVERABLE_IF(!aubCSR->initializeTagAllocation());
        *aubCSR->getTagAddress() = std::numeric_limits<uint32_t>::max();
    }
}

template <typename BaseCSR>
bool CommandStreamReceiverWithAUBDump<BaseCSR>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (aubCSR) {
        aubCSR->flush(batchBuffer, allocationsForResidency);
        aubCSR->setLatestSentTaskCount(BaseCSR::peekLatestSentTaskCount());
    }
    return BaseCSR::flush(batchBuffer, allocationsForResidency);
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    auto residencyTaskCount = gfxAllocation.getResidencyTaskCount(this->osContext->getContextId());
    BaseCSR::makeNonResident(gfxAllocation);
    if (aubCSR) {
        gfxAllocation.updateResidencyTaskCount(residencyTaskCount, this->osContext->getContextId());
        aubCSR->makeNonResident(gfxAllocation);
    }
}

template <typename BaseCSR>
AubSubCaptureStatus CommandStreamReceiverWithAUBDump<BaseCSR>::checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo) {
    auto status = BaseCSR::checkAndActivateAubSubCapture(dispatchInfo);
    if (aubCSR) {
        status = aubCSR->checkAndActivateAubSubCapture(dispatchInfo);
    }
    BaseCSR::programForAubSubCapture(status.wasActiveInPreviousEnqueue, status.isActive);
    return status;
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::setupContext(OsContext &osContext) {
    BaseCSR::setupContext(osContext);
    if (aubCSR) {
        aubCSR->setupContext(osContext);
    }
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                                                      bool useQuickKmdSleep, bool forcePowerSavingMode) {
    if (aubCSR) {
        aubCSR->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, forcePowerSavingMode);
    }

    BaseCSR::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, forcePowerSavingMode);
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::addAubComment(const char *comment) {
    if (aubCSR) {
        aubCSR->addAubComment(comment);
    }
    BaseCSR::addAubComment(comment);
}
} // namespace NEO
