/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub/aub_subcapture.h"
#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/aub_mem_dump/aub_alloc_dump.inl"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/neo_driver_version.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/page_table.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/source/os_interface/product_helper.h"

#include "aubstream/aubstream.h"

#include <cstring>

namespace NEO {

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw(const std::string &fileName,
                                                                  bool standalone,
                                                                  ExecutionEnvironment &executionEnvironment,
                                                                  uint32_t rootDeviceIndex,
                                                                  const DeviceBitfield deviceBitfield)
    : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield),
      standalone(standalone) {

    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initAubCenter(this->isLocalMemoryEnabled(), fileName, this->getType());
    auto aubCenter = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.get();
    UNRECOVERABLE_IF(nullptr == aubCenter);

    auto subCaptureCommon = aubCenter->getSubCaptureCommon();
    UNRECOVERABLE_IF(nullptr == subCaptureCommon);
    subCaptureManager = std::make_unique<AubSubCaptureManager>(fileName, *subCaptureCommon, ApiSpecificConfig::getRegistryPath());

    aubManager = aubCenter->getAubManager();

    auto releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    if (!aubCenter->getPhysicalAddressAllocator()) {
        aubCenter->initPhysicalAddressAllocator(this->createPhysicalAddressAllocator(&this->peekHwInfo(), releaseHelper));
    }
    auto physicalAddressAllocator = aubCenter->getPhysicalAddressAllocator();
    UNRECOVERABLE_IF(nullptr == physicalAddressAllocator);
    ppgtt = std::make_unique<std::conditional<is64bit, PML4, PDPE>::type>(physicalAddressAllocator);
    ggtt = std::make_unique<PDPE>(physicalAddressAllocator);

    if (debugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)debugManager.flags.CsrDispatchMode.get();
    }

    auto debugDeviceId = debugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? this->peekHwInfo().capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
    this->defaultSshSize = 64 * MemoryConstants::kiloByte;
}

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::~AUBCommandStreamReceiverHw() {
    if (osContext) {
        pollForCompletion();
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::openFile(const std::string &fileName) {
    auto streamLocked = lockStream();
    initFile(fileName);
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::reopenFile(const std::string &fileName) {
    auto streamLocked = lockStream();

    if (isFileOpen()) {
        if (fileName != getFileName()) {
            closeFile();
        }
    }
    if (!isFileOpen()) {
        initFile(fileName);
        return true;
    }
    return false;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initFile(const std::string &fileName) {
    if (aubManager) {
        if (!aubManager->isOpen()) {
            aubManager->open(fileName);
            // This UNRECOVERABLE_IF most probably means you are not executing aub tests with correct current directory (containing aub_out folder)
            UNRECOVERABLE_IF(!aubManager->isOpen());

            std::ostringstream str;
            str << "driver version: " << driverVersion;
            aubManager->addComment(str.str().c_str());

            std::string strWithNonDefaultFlags;
            std::string strWithAllFlags;

            debugManager.getStringWithFlags(strWithAllFlags, strWithNonDefaultFlags);
            auto vectorWithNonDefaultFlags = StringHelpers::split(strWithNonDefaultFlags, "\n");
            for (auto &comment : vectorWithNonDefaultFlags) {
                aubManager->addComment((comment).c_str());
            }
        }
        return;
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::closeFile() {
    if (aubManager && aubManager->isOpen()) {
        aubManager->close();
    }
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::isFileOpen() const {
    return aubManager ? aubManager->isOpen() : false;
}

template <typename GfxFamily>
const std::string AUBCommandStreamReceiverHw<GfxFamily>::getFileName() {
    return aubManager ? aubManager->getFileName() : std::string{};
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initializeEngine() {
    auto streamLocked = lockStream();
    isEngineInitialized = true;

    if (hardwareContextController) {
        hardwareContextController->initialize();
    }
}

template <typename GfxFamily>
CommandStreamReceiver *AUBCommandStreamReceiverHw<GfxFamily>::create(const std::string &fileName,
                                                                     bool standalone,
                                                                     ExecutionEnvironment &executionEnvironment,
                                                                     uint32_t rootDeviceIndex,
                                                                     const DeviceBitfield deviceBitfield) {
    auto csr = std::make_unique<AUBCommandStreamReceiverHw<GfxFamily>>(fileName, standalone, executionEnvironment, rootDeviceIndex, deviceBitfield);

    if (!csr->subCaptureManager->isSubCaptureMode()) {
        csr->openFile(fileName);
    }

    return csr.release();
}

template <typename GfxFamily>
SubmissionStatus AUBCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (subCaptureManager->isSubCaptureMode()) {
        if (!subCaptureManager->isSubCaptureEnabled()) {
            if (this->standalone) {
                volatile TagAddressType *pollAddress = this->tagAddress;
                for (uint32_t i = 0; i < this->activePartitions; i++) {
                    *pollAddress = this->peekLatestSentTaskCount();
                    pollAddress = ptrOffset(pollAddress, this->immWritePostSyncWriteOffset);
                }
            }
            return SubmissionStatus::success;
        }
    }

    initializeEngine();

    // Write our batch buffer
    auto commandBufferAllocationBackup = batchBuffer.commandBufferAllocation;
    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto batchBufferGpuAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    DEBUG_BREAK_IF(currentOffset < batchBuffer.startOffset);
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        nullptr, [&](GraphicsAllocation *ptr) { this->getMemoryManager()->freeGraphicsMemory(ptr); });
    if (debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        flatBatchBuffer.reset(this->flatBatchBufferHelper->flattenBatchBuffer(this->rootDeviceIndex, batchBuffer, sizeBatchBuffer, this->dispatchMode, this->getOsContext().getDeviceBitfield()));
        if (flatBatchBuffer.get() != nullptr) {
            pBatchBuffer = flatBatchBuffer->getUnderlyingBuffer();
            batchBufferGpuAddress = flatBatchBuffer->getGpuAddress();
            batchBuffer.commandBufferAllocation = flatBatchBuffer.get();
        }
    }

    allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);

    processResidency(allocationsForResidency, 0u);

    if (!this->standalone || debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        allocationsForResidency.pop_back();
    }

    submitBatchBufferAub(batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer, this->getMemoryBank(batchBuffer.commandBufferAllocation), this->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));

    if (this->standalone) {
        volatile TagAddressType *pollAddress = this->tagAddress;
        for (uint32_t i = 0; i < this->activePartitions; i++) {
            *pollAddress = this->peekLatestSentTaskCount();
            pollAddress = ptrOffset(pollAddress, this->immWritePostSyncWriteOffset);
        }
    }

    if (subCaptureManager->isSubCaptureMode()) {
        pollForCompletion();
        subCaptureManager->disableSubCapture();
    }

    if (debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        pollForCompletion();
        batchBuffer.commandBufferAllocation = commandBufferAllocationBackup;
    }

    return SubmissionStatus::success;
}

template <typename GfxFamily>

void AUBCommandStreamReceiverHw<GfxFamily>::submitBatchBufferAub(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) {
    auto streamLocked = lockStream();

    if (hardwareContextController) {
        if (batchBufferSize) {
            hardwareContextController->submit(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, MemoryConstants::pageSize64k, false);
        }
        return;
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion(bool skipTaskCountCheck) {
    const auto lock = std::unique_lock<decltype(pollForCompletionLock)>{pollForCompletionLock};
    if (!skipTaskCountCheck && (this->pollForCompletionTaskCount == this->latestSentTaskCount)) {
        return;
    }
    pollForCompletionImpl();
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletionImpl() {
    this->pollForCompletionTaskCount = this->latestSentTaskCount;

    if (subCaptureManager->isSubCaptureMode()) {
        if (!subCaptureManager->isSubCaptureEnabled()) {
            return;
        }
    }

    auto streamLocked = lockStream();

    if (hardwareContextController) {
        hardwareContextController->pollForCompletion();
        return;
    }
}

template <typename GfxFamily>
inline WaitStatus AUBCommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) {
    const auto result = CommandStreamReceiverSimulatedHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle);
    pollForCompletion();

    return result;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::makeResidentExternal(AllocationView &allocationView) {
    externalAllocations.push_back(allocationView);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::makeNonResidentExternal(uint64_t gpuAddress) {
    for (auto it = externalAllocations.begin(); it != externalAllocations.end(); it++) {
        if (it->first == gpuAddress) {
            externalAllocations.erase(it);
            break;
        }
    }
}
template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) {
    UNRECOVERABLE_IF(!isEngineInitialized);
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) {
    if (!this->isAubWritable(gfxAllocation)) {
        return false;
    }

    if (!isEngineInitialized) {
        initializeEngine();
    }

    bool ownsLock = !gfxAllocation.isLocked();
    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;
    if (!this->getParametersForMemory(gfxAllocation, gpuAddress, cpuAddress, size)) {
        return false;
    }

    auto streamLocked = lockStream();

    if (aubManager) {
        this->writeMemoryWithAubManager(gfxAllocation, isChunkCopy, gpuVaChunkOffset, chunkSize);
    } else {
        UNRECOVERABLE_IF(isChunkCopy);
    }

    streamLocked.unlock();

    if (gfxAllocation.isLocked() && ownsLock) {
        this->getMemoryManager()->unlockResource(&gfxAllocation);
    }

    if (AubHelper::isOneTimeAubWritableAllocationType(gfxAllocation.getAllocationType())) {
        this->setAubWritable(false, gfxAllocation);
    }

    return true;
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(AllocationView &allocationView) {
    GraphicsAllocation gfxAllocation(this->rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, reinterpret_cast<void *>(allocationView.first), allocationView.first, 0llu, allocationView.second, MemoryPool::memoryNull, 0u);
    return writeMemory(gfxAllocation);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::writeMMIO(uint32_t offset, uint32_t value) {
    auto streamLocked = lockStream();

    if (hardwareContextController) {
        hardwareContextController->writeMMIO(offset, value);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
    if (hardwareContextController) {
        // Add support for expectMMIO to AubStream
        return;
    }
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::expectMemory(const void *gfxAddress, const void *srcAddress,
                                                         size_t length, uint32_t compareOperation) {
    pollForCompletion();

    auto streamLocked = lockStream();

    if (hardwareContextController) {
        hardwareContextController->expectMemory(reinterpret_cast<uint64_t>(gfxAddress), srcAddress, length, compareOperation);
        return true;
    }
    return false;
}

template <typename GfxFamily>
SubmissionStatus AUBCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) {
    if (subCaptureManager->isSubCaptureMode()) {
        if (!subCaptureManager->isSubCaptureEnabled()) {
            return SubmissionStatus::success;
        }
    }

    for (auto &externalAllocation : externalAllocations) {
        if (!writeMemory(externalAllocation)) {
            DEBUG_BREAK_IF(externalAllocation.second != 0);
        }
    }

    for (auto &gfxAllocation : allocationsForResidency) {
        if (dumpAubNonWritable) {
            this->setAubWritable(true, *gfxAllocation);
        }
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!((gfxAllocation->getUnderlyingBufferSize() == 0) ||
                             !this->isAubWritable(*gfxAllocation)));
        }
        gfxAllocation->updateResidencyTaskCount(this->taskCount + 1, this->osContext->getContextId());
    }

    if (this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface) {
        this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface->processFlushResidency(this);
    }

    dumpAubNonWritable = false;
    return SubmissionStatus::success;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::dumpAllocation(GraphicsAllocation &gfxAllocation) {
    bool isBcsCsr = EngineHelpers::isBcs(this->osContext->getEngineType());

    if (isBcsCsr != gfxAllocation.getAubInfo().bcsDumpOnly) {
        return;
    }

    if (debugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.get() || debugManager.flags.AUBDumpAllocsOnEnqueueSVMMemcpyOnly.get()) {
        if (!gfxAllocation.isAllocDumpable()) {
            return;
        }
        gfxAllocation.setAllocDumpable(false, isBcsCsr);
    }

    auto dumpFormat = AubAllocDump::getDumpFormat(gfxAllocation);
    if (dumpFormat > AubAllocDump::DumpFormat::NONE) {
        pollForCompletion();
    }

    auto streamLocked = lockStream();

    if (hardwareContextController) {
        auto surfaceInfo = std::unique_ptr<aub_stream::SurfaceInfo>(AubAllocDump::getDumpSurfaceInfo<GfxFamily>(gfxAllocation, *this->peekGmmHelper(), dumpFormat));
        if (nullptr != surfaceInfo) {
            hardwareContextController->dumpSurface(*surfaceInfo.get());
        }
        return;
    }
}

template <typename GfxFamily>
AubSubCaptureStatus AUBCommandStreamReceiverHw<GfxFamily>::checkAndActivateAubSubCapture(const std::string &kernelName) {
    auto status = subCaptureManager->checkAndActivateSubCapture(kernelName);
    if (status.isActive) {
        auto &subCaptureFile = subCaptureManager->getSubCaptureFileName(kernelName);
        auto isReopened = reopenFile(subCaptureFile);
        if (isReopened) {
            dumpAubNonWritable = true;
        }
    }
    if (this->standalone) {
        this->programForAubSubCapture(status.wasActiveInPreviousEnqueue, status.isActive);
    }
    return status;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::addAubComment(const char *message) {
    auto streamLocked = lockStream();
    if (aubManager) {
        aubManager->addComment(message);
        return;
    }
}

template <typename GfxFamily>
uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getDumpHandle() {
    return hashPtrToU32(this);
}
} // namespace NEO
