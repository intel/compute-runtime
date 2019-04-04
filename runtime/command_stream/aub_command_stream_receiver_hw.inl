/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/aub_alloc_dump.h"
#include "runtime/aub_mem_dump/aub_alloc_dump.inl"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/aub_stream_provider.h"
#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "runtime/helpers/hash.h"
#include "runtime/helpers/neo_driver_version.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"

#include "aub_command_stream_receiver_hw.h"
#include "driver_version.h"
#include "hw_cmds.h"
#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/aubstream.h"

#include <algorithm>
#include <cstring>

namespace NEO {

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw(const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment)
    : BaseClass(executionEnvironment),
      standalone(standalone) {

    executionEnvironment.initAubCenter(this->localMemoryEnabled, fileName, this->getType());
    auto aubCenter = executionEnvironment.aubCenter.get();
    UNRECOVERABLE_IF(nullptr == aubCenter);

    subCaptureManager = aubCenter->getSubCaptureManager();
    aubManager = aubCenter->getAubManager();

    if (!aubCenter->getPhysicalAddressAllocator()) {
        aubCenter->initPhysicalAddressAllocator(this->createPhysicalAddressAllocator(&this->peekHwInfo()));
    }
    auto physicalAddressAllocator = aubCenter->getPhysicalAddressAllocator();
    UNRECOVERABLE_IF(nullptr == physicalAddressAllocator);

    ppgtt = std::make_unique<std::conditional<is64bit, PML4, PDPE>::type>(physicalAddressAllocator);
    ggtt = std::make_unique<PDPE>(physicalAddressAllocator);

    gttRemap = aubCenter->getAddressMapper();
    UNRECOVERABLE_IF(nullptr == gttRemap);

    auto streamProvider = aubCenter->getStreamProvider();
    UNRECOVERABLE_IF(nullptr == streamProvider);

    stream = streamProvider->getStream();
    UNRECOVERABLE_IF(nullptr == stream);

    this->dispatchMode = DispatchMode::BatchedDispatch;
    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }

    auto debugDeviceId = DebugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? this->peekHwInfo().capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
    this->defaultSshSize = 64 * KB;
}

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::~AUBCommandStreamReceiverHw() {
    if (osContext) {
        pollForCompletion();
    }
    this->freeEngineInfo(*gttRemap);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::openFile(const std::string &fileName) {
    auto streamLocked = getAubStream()->lockStream();
    initFile(fileName);
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::reopenFile(const std::string &fileName) {
    auto streamLocked = getAubStream()->lockStream();

    if (isFileOpen()) {
        if (fileName != getFileName()) {
            closeFile();
            this->freeEngineInfo(*gttRemap);
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
            UNRECOVERABLE_IF(!aubManager->isOpen());

            std::ostringstream str;
            str << "driver version: " << driverVersion;
            aubManager->addComment(str.str().c_str());
        }
        return;
    }

    if (!getAubStream()->isOpen()) {
        // Open our file
        stream->open(fileName.c_str());

        if (!getAubStream()->isOpen()) {
            // This UNRECOVERABLE_IF most probably means you are not executing aub tests with correct current directory (containing aub_out folder)
            // try adding <familycodename>_aub
            UNRECOVERABLE_IF(true);
        }
        // Add the file header
        stream->init(AubMemDump::SteppingValues::A, aubDeviceId);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::closeFile() {
    aubManager ? aubManager->close() : stream->close();
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::isFileOpen() const {
    return aubManager ? aubManager->isOpen() : getAubStream()->isOpen();
}

template <typename GfxFamily>
const std::string AUBCommandStreamReceiverHw<GfxFamily>::getFileName() {
    return aubManager ? aubManager->getFileName() : getAubStream()->getFileName();
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initializeEngine() {
    if (hardwareContextController) {
        hardwareContextController->initialize();
        return;
    }

    auto csTraits = this->getCsTraits(osContext->getEngineType());

    if (engineInfo.pLRCA) {
        return;
    }

    this->initGlobalMMIO();
    this->initEngineMMIO();
    this->initAdditionalMMIO();

    // Write driver version
    {
        std::ostringstream str;
        str << "driver version: " << driverVersion;
        getAubStream()->addComment(str.str().c_str());
    }

    // Global HW Status Page
    {
        const size_t sizeHWSP = 0x1000;
        const size_t alignHWSP = 0x1000;
        engineInfo.pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);
        engineInfo.ggttHWSP = gttRemap->map(engineInfo.pGlobalHWStatusPage, sizeHWSP);

        auto physHWSP = ggtt->map(engineInfo.ggttHWSP, sizeHWSP, this->getGTTBits(), this->getMemoryBankForGtt());

        // Write our GHWSP
        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttHWSP;
            getAubStream()->addComment(str.str().c_str());
        }

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physHWSP), data);
        AUB::reserveAddressGGTT(*stream, engineInfo.ggttHWSP, sizeHWSP, physHWSP, data);
        stream->writeMMIO(AubMemDump::computeRegisterOffset(csTraits.mmioBase, 0x2080), engineInfo.ggttHWSP);
    }

    // Allocate the LRCA
    const size_t sizeLRCA = csTraits.sizeLRCA;
    const size_t alignLRCA = csTraits.alignLRCA;
    auto pLRCABase = alignedMalloc(sizeLRCA, alignLRCA);
    engineInfo.pLRCA = pLRCABase;

    // Initialize the LRCA to a known state
    csTraits.initialize(pLRCABase);

    // Reserve the ring buffer
    engineInfo.sizeRingBuffer = 0x4 * 0x1000;
    {
        const size_t alignRingBuffer = 0x1000;
        engineInfo.pRingBuffer = alignedMalloc(engineInfo.sizeRingBuffer, alignRingBuffer);
        engineInfo.ggttRingBuffer = gttRemap->map(engineInfo.pRingBuffer, engineInfo.sizeRingBuffer);
        auto physRingBuffer = ggtt->map(engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, this->getGTTBits(), this->getMemoryBankForGtt());

        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttRingBuffer;
            getAubStream()->addComment(str.str().c_str());
        }

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physRingBuffer), data);
        AUB::reserveAddressGGTT(*stream, engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, physRingBuffer, data);
    }

    // Initialize the ring MMIO registers
    {
        uint32_t ringHead = 0x000;
        uint32_t ringTail = 0x000;
        auto ringBase = engineInfo.ggttRingBuffer;
        auto ringCtrl = (uint32_t)((engineInfo.sizeRingBuffer - 0x1000) | 1);
        csTraits.setRingHead(pLRCABase, ringHead);
        csTraits.setRingTail(pLRCABase, ringTail);
        csTraits.setRingBase(pLRCABase, ringBase);
        csTraits.setRingCtrl(pLRCABase, ringCtrl);
    }

    // Write our LRCA
    {
        engineInfo.ggttLRCA = gttRemap->map(engineInfo.pLRCA, sizeLRCA);
        auto lrcAddressPhys = ggtt->map(engineInfo.ggttLRCA, sizeLRCA, this->getGTTBits(), this->getMemoryBankForGtt());

        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttLRCA;
            getAubStream()->addComment(str.str().c_str());
        }

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(lrcAddressPhys), data);
        AUB::reserveAddressGGTT(*stream, engineInfo.ggttLRCA, sizeLRCA, lrcAddressPhys, data);
        AUB::addMemoryWrite(
            *stream,
            lrcAddressPhys,
            pLRCABase,
            sizeLRCA,
            this->getAddressSpace(csTraits.aubHintLRCA),
            csTraits.aubHintLRCA);
    }

    // Create a context to facilitate AUB dumping of memory using PPGTT
    addContextToken(getDumpHandle());

    DEBUG_BREAK_IF(!engineInfo.pLRCA);
}

template <typename GfxFamily>
CommandStreamReceiver *AUBCommandStreamReceiverHw<GfxFamily>::create(const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment) {
    auto csr = new AUBCommandStreamReceiverHw<GfxFamily>(fileName, standalone, executionEnvironment);

    if (!csr->subCaptureManager->isSubCaptureMode()) {
        csr->openFile(fileName);
    }

    return csr;
}

template <typename GfxFamily>
FlushStamp AUBCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (subCaptureManager->isSubCaptureMode()) {
        if (!subCaptureManager->isSubCaptureEnabled()) {
            if (this->standalone) {
                *this->tagAddress = this->peekLatestSentTaskCount();
            }
            return 0;
        }
    }

    auto streamLocked = getAubStream()->lockStream();

    initializeEngine();

    // Write our batch buffer
    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto batchBufferGpuAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    DEBUG_BREAK_IF(currentOffset < batchBuffer.startOffset);
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        nullptr, [&](GraphicsAllocation *ptr) { this->getMemoryManager()->freeGraphicsMemory(ptr); });
    if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        flatBatchBuffer.reset(this->flatBatchBufferHelper->flattenBatchBuffer(batchBuffer, sizeBatchBuffer, this->dispatchMode));
        if (flatBatchBuffer.get() != nullptr) {
            pBatchBuffer = flatBatchBuffer->getUnderlyingBuffer();
            batchBufferGpuAddress = flatBatchBuffer->getGpuAddress();
            batchBuffer.commandBufferAllocation = flatBatchBuffer.get();
        }
    }

    allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);

    processResidency(allocationsForResidency);

    if (!this->standalone || DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        allocationsForResidency.pop_back();
    }

    submitBatchBuffer(batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer, this->getMemoryBank(batchBuffer.commandBufferAllocation), this->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));

    if (this->standalone) {
        *this->tagAddress = this->peekLatestSentTaskCount();
    }

    if (subCaptureManager->isSubCaptureMode()) {
        pollForCompletion();
        subCaptureManager->disableSubCapture();
    }

    getAubStream()->flush();
    return 0;
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::addPatchInfoComments() {
    std::map<uint64_t, uint64_t> allocationsMap;

    std::ostringstream str;
    str << "PatchInfoData" << std::endl;
    for (auto &patchInfoData : this->flatBatchBufferHelper->getPatchInfoCollection()) {
        str << std::hex << patchInfoData.sourceAllocation << ";";
        str << std::hex << patchInfoData.sourceAllocationOffset << ";";
        str << std::hex << patchInfoData.sourceType << ";";
        str << std::hex << patchInfoData.targetAllocation << ";";
        str << std::hex << patchInfoData.targetAllocationOffset << ";";
        str << std::hex << patchInfoData.targetType << ";";
        str << std::endl;

        if (patchInfoData.sourceAllocation) {
            allocationsMap.insert(std::pair<uint64_t, uint64_t>(patchInfoData.sourceAllocation,
                                                                ppgtt->map(static_cast<uintptr_t>(patchInfoData.sourceAllocation), 1, 0, MemoryBanks::MainBank)));
        }

        if (patchInfoData.targetAllocation) {
            allocationsMap.insert(std::pair<uint64_t, uintptr_t>(patchInfoData.targetAllocation,
                                                                 ppgtt->map(static_cast<uintptr_t>(patchInfoData.targetAllocation), 1, 0, MemoryBanks::MainBank)));
        }
    }
    bool result = getAubStream()->addComment(str.str().c_str());
    this->flatBatchBufferHelper->getPatchInfoCollection().clear();
    if (!result) {
        return false;
    }

    std::ostringstream allocationStr;
    allocationStr << "AllocationsList" << std::endl;
    for (auto &element : allocationsMap) {
        allocationStr << std::hex << element.first << ";" << element.second << std::endl;
    }
    result = getAubStream()->addComment(allocationStr.str().c_str());
    if (!result) {
        return false;
    }
    return true;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) {
    if (hardwareContextController) {
        if (batchBufferSize) {
            hardwareContextController->submit(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, MemoryConstants::pageSize64k);
        }
        return;
    }

    auto csTraits = this->getCsTraits(osContext->getEngineType());

    {
        {
            std::ostringstream str;
            str << "ppgtt: " << std::hex << std::showbase << batchBuffer;
            getAubStream()->addComment(str.str().c_str());
        }

        auto physBatchBuffer = ppgtt->map(static_cast<uintptr_t>(batchBufferGpuAddress), batchBufferSize, entryBits, memoryBank);
        AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);
        AUB::reserveAddressPPGTT(*stream, static_cast<uintptr_t>(batchBufferGpuAddress), batchBufferSize, physBatchBuffer,
                                 entryBits, aubHelperHw);

        AUB::addMemoryWrite(
            *stream,
            physBatchBuffer,
            batchBuffer,
            batchBufferSize,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary),
            AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary);
    }

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        addGUCStartMessage(static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(batchBuffer)));
        addPatchInfoComments();
    }

    // Add a batch buffer start to the ring buffer
    auto previousTail = engineInfo.tailRingBuffer;
    {
        typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
        typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
        typedef typename GfxFamily::MI_NOOP MI_NOOP;

        auto pTail = ptrOffset(engineInfo.pRingBuffer, engineInfo.tailRingBuffer);
        auto ggttTail = ptrOffset(engineInfo.ggttRingBuffer, engineInfo.tailRingBuffer);

        auto sizeNeeded =
            sizeof(MI_BATCH_BUFFER_START) +
            sizeof(MI_LOAD_REGISTER_IMM);

        auto tailAlignment = sizeof(uint64_t);
        sizeNeeded = alignUp(sizeNeeded, tailAlignment);

        if (engineInfo.tailRingBuffer + sizeNeeded >= engineInfo.sizeRingBuffer) {
            // Pad the remaining ring with NOOPs
            auto sizeToWrap = engineInfo.sizeRingBuffer - engineInfo.tailRingBuffer;
            memset(pTail, 0, sizeToWrap);
            // write remaining ring

            auto physDumpStart = ggtt->map(ggttTail, sizeToWrap, this->getGTTBits(), this->getMemoryBankForGtt());
            AUB::addMemoryWrite(
                *stream,
                physDumpStart,
                pTail,
                sizeToWrap,
                this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
                AubMemDump::DataTypeHintValues::TraceCommandBuffer);
            previousTail = 0;
            engineInfo.tailRingBuffer = 0;
            pTail = engineInfo.pRingBuffer;
        } else if (engineInfo.tailRingBuffer == 0) {
            // Add a LRI if this is our first submission
            auto lri = GfxFamily::cmdInitLoadRegisterImm;
            lri.setRegisterOffset(AubMemDump::computeRegisterOffset(csTraits.mmioBase, 0x2244));
            lri.setDataDword(0x00010000);
            *(MI_LOAD_REGISTER_IMM *)pTail = lri;
            pTail = ((MI_LOAD_REGISTER_IMM *)pTail) + 1;
        }

        // Add our BBS
        auto bbs = GfxFamily::cmdInitBatchBufferStart;
        bbs.setBatchBufferStartAddressGraphicsaddress472(static_cast<uint64_t>(batchBufferGpuAddress));
        bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        *(MI_BATCH_BUFFER_START *)pTail = bbs;
        pTail = ((MI_BATCH_BUFFER_START *)pTail) + 1;

        // Compute our new ring tail.
        engineInfo.tailRingBuffer = (uint32_t)ptrDiff(pTail, engineInfo.pRingBuffer);

        // Add NOOPs as needed as our tail needs to be aligned
        while (engineInfo.tailRingBuffer % tailAlignment) {
            *(MI_NOOP *)pTail = GfxFamily::cmdInitNoop;
            pTail = ((MI_NOOP *)pTail) + 1;
            engineInfo.tailRingBuffer = (uint32_t)ptrDiff(pTail, engineInfo.pRingBuffer);
        }
        UNRECOVERABLE_IF((engineInfo.tailRingBuffer % tailAlignment) != 0);

        // Only dump the new commands
        auto ggttDumpStart = ptrOffset(engineInfo.ggttRingBuffer, previousTail);
        auto dumpStart = ptrOffset(engineInfo.pRingBuffer, previousTail);
        auto dumpLength = engineInfo.tailRingBuffer - previousTail;

        // write ring
        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << ggttDumpStart;
            getAubStream()->addComment(str.str().c_str());
        }

        auto physDumpStart = ggtt->map(ggttDumpStart, dumpLength, this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            *stream,
            physDumpStart,
            dumpStart,
            dumpLength,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
            AubMemDump::DataTypeHintValues::TraceCommandBuffer);

        // update the ring mmio tail in the LRCA
        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttLRCA + 0x101c;
            getAubStream()->addComment(str.str().c_str());
        }

        auto physLRCA = ggtt->map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRingBuffer), this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            *stream,
            physLRCA + 0x101c,
            &engineInfo.tailRingBuffer,
            sizeof(engineInfo.tailRingBuffer),
            this->getAddressSpace(csTraits.aubHintLRCA));

        DEBUG_BREAK_IF(engineInfo.tailRingBuffer >= engineInfo.sizeRingBuffer);
    }

    // Submit our execlist by submitting to the execlist submit ports
    {
        typename AUB::MiContextDescriptorReg contextDescriptor = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

        contextDescriptor.sData.Valid = true;
        contextDescriptor.sData.ForcePageDirRestore = false;
        contextDescriptor.sData.ForceRestore = false;
        contextDescriptor.sData.Legacy = true;
        contextDescriptor.sData.FaultSupport = 0;
        contextDescriptor.sData.PrivilegeAccessOrPPGTT = true;
        contextDescriptor.sData.ADor64bitSupport = AUB::Traits::addressingBits > 32;

        auto ggttLRCA = engineInfo.ggttLRCA;
        contextDescriptor.sData.LogicalRingCtxAddress = ggttLRCA / 4096;
        contextDescriptor.sData.ContextID = 0;

        this->submitLRCA(contextDescriptor);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion() {
    const auto lock = std::unique_lock<decltype(pollForCompletionLock)>{pollForCompletionLock};
    if (this->pollForCompletionTaskCount == this->latestSentTaskCount) {
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

    if (hardwareContextController) {
        hardwareContextController->pollForCompletion();
        return;
    }

    const auto mmioBase = this->getCsTraits(osContext->getEngineType()).mmioBase;
    const bool pollNotEqual = false;
    const uint32_t mask = getMaskAndValueForPollForCompletion();
    const uint32_t value = mask;
    stream->registerPoll(
        AubMemDump::computeRegisterOffset(mmioBase, 0x2234), //EXECLIST_STATUS
        mask,
        value,
        pollNotEqual,
        AubMemDump::CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
}

template <typename GfxFamily>
inline void AUBCommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) {
    CommandStreamReceiverSimulatedHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, forcePowerSavingMode);
    pollForCompletion();
}

template <typename GfxFamily>
constexpr uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getMaskAndValueForPollForCompletion() {
    return 0x100;
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
    {
        std::ostringstream str;
        str << "ppgtt: " << std::hex << std::showbase << gpuAddress << " end address: " << gpuAddress + size << " cpu address: " << cpuAddress << " size: " << std::dec << size;
        getAubStream()->addComment(str.str().c_str());
    }

    AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        AUB::reserveAddressGGTTAndWriteMmeory(*stream, static_cast<uintptr_t>(gpuAddress), cpuAddress, physAddress, size, offset, entryBits,
                                              aubHelperHw);
    };

    ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, entryBits, walker, memoryBank);
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation) {
    bool ownsLock = !gfxAllocation.isLocked();

    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;
    if (!this->getParametersForWriteMemory(gfxAllocation, gpuAddress, cpuAddress, size)) {
        return false;
    }

    if (aubManager) {
        this->writeMemoryWithAubManager(gfxAllocation);
    } else {
        writeMemory(gpuAddress, cpuAddress, size, this->getMemoryBank(&gfxAllocation), this->getPPGTTAdditionalBits(&gfxAllocation));
    }

    if (gfxAllocation.isLocked() && ownsLock) {
        this->getMemoryManager()->unlockResource(&gfxAllocation);
    }

    if (AubHelper::isOneTimeAubWritableAllocationType(gfxAllocation.getAllocationType())) {
        gfxAllocation.setAubWritable(false);
    }

    return true;
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(AllocationView &allocationView) {
    GraphicsAllocation gfxAllocation(GraphicsAllocation::AllocationType::UNKNOWN, reinterpret_cast<void *>(allocationView.first), allocationView.first, 0llu, allocationView.second, MemoryPool::MemoryNull, false);
    return writeMemory(gfxAllocation);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
    if (hardwareContextController) {
        //Add support for expectMMIO to AubStream
        return;
    }
    this->getAubStream()->expectMMIO(mmioRegister, expectedValue);
}

template <typename GfxFamily>
cl_int AUBCommandStreamReceiverHw<GfxFamily>::expectMemory(const void *gfxAddress, const void *srcAddress,
                                                           size_t length, uint32_t compareOperation) {
    pollForCompletion();

    if (hardwareContextController) {
        hardwareContextController->expectMemory(reinterpret_cast<uint64_t>(gfxAddress), srcAddress, length, compareOperation);
    }

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        UNRECOVERABLE_IF(offset > length);

        this->getAubStream()->expectMemory(physAddress,
                                           ptrOffset(srcAddress, offset),
                                           size,
                                           this->getAddressSpaceFromPTEBits(entryBits),
                                           compareOperation);
    };

    this->ppgtt->pageWalk(reinterpret_cast<uintptr_t>(gfxAddress), length, 0, PageTableEntry::nonValidBits, walker, MemoryBanks::BankNotSpecified);

    return CL_SUCCESS;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency) {
    if (subCaptureManager->isSubCaptureMode()) {
        if (!subCaptureManager->isSubCaptureEnabled()) {
            return;
        }
    }

    for (auto &externalAllocation : externalAllocations) {
        if (!writeMemory(externalAllocation)) {
            DEBUG_BREAK_IF(externalAllocation.second != 0);
        }
    }

    for (auto &gfxAllocation : allocationsForResidency) {
        if (dumpAubNonWritable) {
            gfxAllocation->setAubWritable(true);
        }
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!((gfxAllocation->getUnderlyingBufferSize() == 0) || !gfxAllocation->isAubWritable()));
        }
        gfxAllocation->updateResidencyTaskCount(this->taskCount + 1, this->osContext->getContextId());
    }

    dumpAubNonWritable = false;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::dumpAllocation(GraphicsAllocation &gfxAllocation) {
    if (DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.get()) {
        if (!gfxAllocation.isAllocDumpable()) {
            return;
        }
        gfxAllocation.setAllocDumpable(false);
    }

    auto dumpFormat = AubAllocDump::getDumpFormat(gfxAllocation);
    if (dumpFormat > AubAllocDump::DumpFormat::NONE) {
        pollForCompletion();
    }

    if (hardwareContextController) {
        auto surfaceInfo = std::unique_ptr<aub_stream::SurfaceInfo>(AubAllocDump::getDumpSurfaceInfo<GfxFamily>(gfxAllocation, dumpFormat));
        if (nullptr != surfaceInfo) {
            hardwareContextController->dumpSurface(*surfaceInfo.get());
        }
        return;
    }

    AubAllocDump::dumpAllocation<GfxFamily>(dumpFormat, gfxAllocation, getAubStream(), getDumpHandle());
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.isResident(this->osContext->getContextId())) {
        dumpAllocation(gfxAllocation);
        this->getEvictionAllocations().push_back(&gfxAllocation);
        gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) {
    bool active = subCaptureManager->activateSubCapture(dispatchInfo);
    if (active) {
        std::string subCaptureFile = subCaptureManager->getSubCaptureFileName(dispatchInfo);
        auto isReopened = reopenFile(subCaptureFile);
        if (isReopened) {
            dumpAubNonWritable = true;
        }
    }
    if (this->standalone) {
        if (DebugManager.flags.ForceCsrFlushing.get()) {
            this->flushBatchedSubmissions();
        }
        if (DebugManager.flags.ForceCsrReprogramming.get()) {
            this->initProgrammingFlags();
        }
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::addAubComment(const char *message) {
    if (aubManager) {
        aubManager->addComment(message);
        return;
    }
    getAubStream()->addComment(message);
}

template <typename GfxFamily>
uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getDumpHandle() {
    return hashPtrToU32(this);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::addContextToken(uint32_t dumpHandle) {
    // Some simulator versions don't support adding the context token.
    // This hook allows specialization for those that do.
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::addGUCStartMessage(uint64_t batchBufferAddress) {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto bufferSize = sizeof(uint32_t) + sizeof(MI_BATCH_BUFFER_START);
    AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);

    std::unique_ptr<void, std::function<void(void *)>> buffer(this->getMemoryManager()->alignedMallocWrapper(bufferSize, MemoryConstants::pageSize), [&](void *ptr) { this->getMemoryManager()->alignedFreeWrapper(ptr); });
    LinearStream linearStream(buffer.get(), bufferSize);

    uint32_t *header = static_cast<uint32_t *>(linearStream.getSpace(sizeof(uint32_t)));
    *header = getGUCWorkQueueItemHeader();

    MI_BATCH_BUFFER_START *miBatchBufferStart = linearStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    DEBUG_BREAK_IF(bufferSize != linearStream.getUsed());
    *miBatchBufferStart = GfxFamily::cmdInitBatchBufferStart;
    miBatchBufferStart->setBatchBufferStartAddressGraphicsaddress472(AUB::ptrToPPGTT(buffer.get()));
    miBatchBufferStart->setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);

    auto physBufferAddres = ppgtt->map(reinterpret_cast<uintptr_t>(buffer.get()), bufferSize,
                                       this->getPPGTTAdditionalBits(linearStream.getGraphicsAllocation()),
                                       MemoryBanks::MainBank);

    AUB::reserveAddressPPGTT(*stream, reinterpret_cast<uintptr_t>(buffer.get()), bufferSize, physBufferAddres,
                             this->getPPGTTAdditionalBits(linearStream.getGraphicsAllocation()),
                             aubHelperHw);

    AUB::addMemoryWrite(
        *stream,
        physBufferAddres,
        buffer.get(),
        bufferSize,
        this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

    PatchInfoData patchInfoData(batchBufferAddress, 0u, PatchInfoAllocationType::Default, reinterpret_cast<uintptr_t>(buffer.get()), sizeof(uint32_t) + sizeof(MI_BATCH_BUFFER_START) - sizeof(uint64_t), PatchInfoAllocationType::GUCStartMessage);
    this->flatBatchBufferHelper->setPatchInfoData(patchInfoData);
}

template <typename GfxFamily>
uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getGUCWorkQueueItemHeader() {
    uint32_t GUCWorkQueueItemHeader = 0x00030001;
    return GUCWorkQueueItemHeader;
}

template <typename GfxFamily>
int AUBCommandStreamReceiverHw<GfxFamily>::getAddressSpaceFromPTEBits(uint64_t entryBits) const {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

} // namespace NEO
