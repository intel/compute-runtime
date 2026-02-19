/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <igfxfmid.h>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * NEO::maxCoreEnumValue];

template <typename BaseCSR>
CommandStreamReceiverWithAUBDump<BaseCSR>::CommandStreamReceiverWithAUBDump(const std::string &baseName,
                                                                            ExecutionEnvironment &executionEnvironment,
                                                                            uint32_t rootDeviceIndex,
                                                                            const DeviceBitfield deviceBitfield)
    : BaseCSR(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    bool isAubManager = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter && executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter->getAubManager();
    bool isTbxMode = CommandStreamReceiverType::tbx == BaseCSR::getType();
    bool createAubCsr = (isAubManager && isTbxMode) ? false : true;
    if (createAubCsr) {
        aubCSR.reset(AUBCommandStreamReceiver::create(baseName, false, executionEnvironment, rootDeviceIndex, deviceBitfield));
        UNRECOVERABLE_IF(!aubCSR);
        UNRECOVERABLE_IF(!aubCSR->initializeTagAllocation());

        uint32_t subDevices = static_cast<uint32_t>(this->deviceBitfield.count());
        auto tagAddressToInitialize = aubCSR->getTagAddress();

        for (uint32_t i = 0; i < subDevices; i++) {
            *tagAddressToInitialize = std::numeric_limits<uint32_t>::max();
            tagAddressToInitialize = ptrOffset(tagAddressToInitialize, this->immWritePostSyncWriteOffset);
        }
    }
}

template <typename BaseCSR>
SubmissionStatus CommandStreamReceiverWithAUBDump<BaseCSR>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (aubCSR) {
        aubCSR->flush(batchBuffer, allocationsForResidency);
        aubCSR->setLatestSentTaskCount(BaseCSR::peekLatestSentTaskCount());
        aubCSR->setLatestFlushedTaskCount(BaseCSR::peekLatestSentTaskCount());
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
AubSubCaptureStatus CommandStreamReceiverWithAUBDump<BaseCSR>::checkAndActivateAubSubCapture(const std::string &kernelName) {
    auto status = BaseCSR::checkAndActivateAubSubCapture(kernelName);
    if (aubCSR) {
        status = aubCSR->checkAndActivateAubSubCapture(kernelName);
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
WaitStatus CommandStreamReceiverWithAUBDump<BaseCSR>::waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait,
                                                                                            bool useQuickKmdSleep, QueueThrottle throttle) {
    if (aubCSR) {
        aubCSR->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle);
    }

    return BaseCSR::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle);
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::addAubComment(const char *comment) {
    if (aubCSR) {
        aubCSR->addAubComment(comment);
    }
    BaseCSR::addAubComment(comment);
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::pollForCompletion(bool skipTaskCountCheck) {
    if (aubCSR) {
        aubCSR->pollForCompletion(skipTaskCountCheck);
    }
    BaseCSR::pollForCompletion(skipTaskCountCheck);
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::pollForAubCompletion() {
    if (aubCSR) {
        aubCSR->pollForCompletion(true);
    }
    BaseCSR::pollForAubCompletion();
}

template <typename BaseCSR>
bool CommandStreamReceiverWithAUBDump<BaseCSR>::expectMemory(const void *gfxAddress, const void *srcAddress,
                                                             size_t length, uint32_t compareOperation) {
    if (aubCSR) {
        [[maybe_unused]] auto result = aubCSR->expectMemory(gfxAddress, srcAddress, length, compareOperation);
        DEBUG_BREAK_IF(!result);
    }
    return BaseCSR::expectMemory(gfxAddress, srcAddress, length, compareOperation);
}

template <typename BaseCSR>
bool CommandStreamReceiverWithAUBDump<BaseCSR>::writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) {
    const bool chunkWriteForAub = isChunkCopy && ((aubCSR == nullptr) || aubCSR->isChunkCopySupportedForSimulation());
    const uint64_t aubChunkOffset = chunkWriteForAub ? gpuVaChunkOffset : 0;
    const size_t aubChunkSize = chunkWriteForAub ? chunkSize : 0;
    bool aubWriteStatus = true;
    if (aubCSR) {
        aubWriteStatus = aubCSR->writeMemory(gfxAllocation,
                                             chunkWriteForAub,
                                             aubChunkOffset,
                                             aubChunkSize);
        DEBUG_BREAK_IF(!aubWriteStatus);
    }

    const bool chunkWriteForBase = isChunkCopy && BaseCSR::isChunkCopySupportedForSimulation();
    const uint64_t baseChunkOffset = chunkWriteForBase ? gpuVaChunkOffset : 0;
    const size_t baseChunkSize = chunkWriteForBase ? chunkSize : 0;
    const bool baseWriteStatus = BaseCSR::writeMemory(gfxAllocation,
                                                      chunkWriteForBase,
                                                      baseChunkOffset,
                                                      baseChunkSize);

    if (BaseCSR::getType() == CommandStreamReceiverType::hardware) {
        return aubCSR ? aubWriteStatus : baseWriteStatus;
    }

    if (aubCSR) {
        return aubWriteStatus && baseWriteStatus;
    }

    return baseWriteStatus;
}

template <typename BaseCSR>
bool CommandStreamReceiverWithAUBDump<BaseCSR>::isChunkCopySupportedForSimulation() const {
    const bool aubChunkCopySupported = (aubCSR == nullptr) || aubCSR->isChunkCopySupportedForSimulation();
    if (BaseCSR::getType() == CommandStreamReceiverType::hardware) {
        return aubChunkCopySupported;
    }

    return BaseCSR::isChunkCopySupportedForSimulation() || aubChunkCopySupported;
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::setWritableForSimulation(bool writable, GraphicsAllocation &gfxAllocation) {
    BaseCSR::setWritableForSimulation(writable, gfxAllocation);
    if (aubCSR) {
        aubCSR->setWritableForSimulation(writable, gfxAllocation);
    }
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::writePooledMemory(SharedPoolAllocation &sharedPoolAllocation, bool initFullPageTables) {
    if (aubCSR) {
        aubCSR->writePooledMemory(sharedPoolAllocation, initFullPageTables);
    }
    BaseCSR::writePooledMemory(sharedPoolAllocation, initFullPageTables);
}

} // namespace NEO
