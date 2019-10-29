/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/execution_environment/execution_environment.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];

template <typename BaseCSR>
CommandStreamReceiverWithAUBDump<BaseCSR>::CommandStreamReceiverWithAUBDump(const std::string &baseName, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex)
    : BaseCSR(executionEnvironment, rootDeviceIndex) {
    bool isAubManager = executionEnvironment.rootDeviceEnvironments[0].aubCenter && executionEnvironment.rootDeviceEnvironments[0].aubCenter->getAubManager();
    bool isTbxMode = CommandStreamReceiverType::CSR_TBX == BaseCSR::getType();
    bool createAubCsr = (isAubManager && isTbxMode) ? false : true;
    if (createAubCsr) {
        aubCSR.reset(AUBCommandStreamReceiver::create(baseName, false, executionEnvironment, rootDeviceIndex));
    }
}

template <typename BaseCSR>
FlushStamp CommandStreamReceiverWithAUBDump<BaseCSR>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (aubCSR) {
        aubCSR->flush(batchBuffer, allocationsForResidency);
        aubCSR->setLatestSentTaskCount(BaseCSR::peekLatestSentTaskCount());
    }
    FlushStamp flushStamp = BaseCSR::flush(batchBuffer, allocationsForResidency);
    return flushStamp;
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
} // namespace NEO
