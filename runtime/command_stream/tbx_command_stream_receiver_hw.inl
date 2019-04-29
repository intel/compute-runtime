/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/memory_manager/physical_address_allocator.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"

#include "hw_cmds.h"

#include <cstring>

namespace NEO {

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment)
    : BaseClass(executionEnvironment) {

    physicalAddressAllocator.reset(this->createPhysicalAddressAllocator(&this->peekHwInfo()));
    executionEnvironment.initAubCenter(this->localMemoryEnabled, "", this->getType());
    auto aubCenter = executionEnvironment.aubCenter.get();
    UNRECOVERABLE_IF(nullptr == aubCenter);

    aubManager = aubCenter->getAubManager();

    ppgtt = std::make_unique<std::conditional<is64bit, PML4, PDPE>::type>(physicalAddressAllocator.get());
    ggtt = std::make_unique<PDPE>(physicalAddressAllocator.get());

    auto debugDeviceId = DebugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? this->peekHwInfo().capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
    this->stream = &tbxStream;
}

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::~TbxCommandStreamReceiverHw() {
    if (streamInitialized) {
        tbxStream.close();
    }

    this->freeEngineInfo(gttRemap);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::initializeEngine() {
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

    // Global HW Status Page
    {
        const size_t sizeHWSP = 0x1000;
        const size_t alignHWSP = 0x1000;
        engineInfo.pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);
        engineInfo.ggttHWSP = gttRemap.map(engineInfo.pGlobalHWStatusPage, sizeHWSP);
        auto physHWSP = ggtt->map(engineInfo.ggttHWSP, sizeHWSP, this->getGTTBits(), this->getMemoryBankForGtt());

        // Write our GHWSP
        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physHWSP), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttHWSP, sizeHWSP, physHWSP, data);
        tbxStream.writeMMIO(AubMemDump::computeRegisterOffset(csTraits.mmioBase, 0x2080), engineInfo.ggttHWSP);
    }

    // Allocate the LRCA
    const size_t sizeLRCA = csTraits.sizeLRCA;
    const size_t alignLRCA = csTraits.alignLRCA;
    auto pLRCABase = alignedMalloc(sizeLRCA, alignLRCA);
    engineInfo.pLRCA = pLRCABase;

    // Initialize the LRCA to a known state
    csTraits.initialize(pLRCABase);

    // Reserve the RCS ring buffer
    engineInfo.sizeRingBuffer = 0x4 * 0x1000;
    {
        const size_t alignRCS = 0x1000;
        engineInfo.pRingBuffer = alignedMalloc(engineInfo.sizeRingBuffer, alignRCS);
        engineInfo.ggttRingBuffer = gttRemap.map(engineInfo.pRingBuffer, engineInfo.sizeRingBuffer);
        auto physRCS = ggtt->map(engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, this->getGTTBits(), this->getMemoryBankForGtt());

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physRCS), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, physRCS, data);
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
        engineInfo.ggttLRCA = gttRemap.map(engineInfo.pLRCA, sizeLRCA);
        auto lrcAddressPhys = ggtt->map(engineInfo.ggttLRCA, sizeLRCA, this->getGTTBits(), this->getMemoryBankForGtt());

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(lrcAddressPhys), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttLRCA, sizeLRCA, lrcAddressPhys, data);
        AUB::addMemoryWrite(
            tbxStream,
            lrcAddressPhys,
            pLRCABase,
            sizeLRCA,
            this->getAddressSpace(csTraits.aubHintLRCA),
            csTraits.aubHintLRCA);
    }

    DEBUG_BREAK_IF(!engineInfo.pLRCA);
}

template <typename GfxFamily>
CommandStreamReceiver *TbxCommandStreamReceiverHw<GfxFamily>::create(const std::string &baseName, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    TbxCommandStreamReceiverHw<GfxFamily> *csr;
    if (withAubDump) {
        auto hwInfo = executionEnvironment.getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo->pPlatform->eRenderCoreFamily);
        auto localMemoryEnabled = hwHelper.getEnableLocalMemory(*hwInfo);
        auto fullName = AUBCommandStreamReceiver::createFullFilePath(*hwInfo, baseName);
        executionEnvironment.initAubCenter(localMemoryEnabled, fullName, CommandStreamReceiverType::CSR_TBX_WITH_AUB);

        csr = new CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<GfxFamily>>(baseName, executionEnvironment);
        if (csr->aubManager) {
            if (!csr->aubManager->isOpen()) {
                csr->aubManager->open(fullName);
                UNRECOVERABLE_IF(!csr->aubManager->isOpen());
            }
        }
    } else {
        csr = new TbxCommandStreamReceiverHw<GfxFamily>(executionEnvironment);
    }

    if (!csr->aubManager) {
        // Open our stream
        csr->stream->open(nullptr);

        // Add the file header.
        bool streamInitialized = csr->stream->init(AubMemDump::SteppingValues::A, csr->aubDeviceId);
        csr->streamInitialized = streamInitialized;
    }
    return csr;
}

template <typename GfxFamily>
FlushStamp TbxCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    initializeEngine();

    // Write our batch buffer
    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto batchBufferGpuAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    DEBUG_BREAK_IF(currentOffset < batchBuffer.startOffset);
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;

    if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
        CommandStreamReceiver::makeResident(*batchBuffer.commandBufferAllocation);
    } else {
        allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.commandBufferAllocation->updateResidencyTaskCount(this->taskCount, this->osContext->getContextId());
    }

    // Write allocations for residency
    processResidency(allocationsForResidency);

    submitBatchBuffer(batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer, this->getMemoryBank(batchBuffer.commandBufferAllocation), this->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));
    return 0;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) {
    if (hardwareContextController) {
        if (batchBufferSize) {
            hardwareContextController->submit(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, MemoryConstants::pageSize64k);
        }
        return;
    }

    auto csTraits = this->getCsTraits(osContext->getEngineType());

    {
        auto physBatchBuffer = ppgtt->map(static_cast<uintptr_t>(batchBufferGpuAddress), batchBufferSize, entryBits, memoryBank);

        AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);
        AUB::reserveAddressPPGTT(tbxStream, static_cast<uintptr_t>(batchBufferGpuAddress), batchBufferSize, physBatchBuffer,
                                 entryBits, aubHelperHw);

        AUB::addMemoryWrite(
            tbxStream,
            physBatchBuffer,
            batchBuffer,
            batchBufferSize,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary),
            AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary);
    }

    // Add a batch buffer start to the RCS
    auto previousTail = engineInfo.tailRingBuffer;
    {
        typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
        typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
        typedef typename GfxFamily::MI_NOOP MI_NOOP;

        auto pTail = ptrOffset(engineInfo.pRingBuffer, engineInfo.tailRingBuffer);
        auto ggttTail = ptrOffset(engineInfo.ggttRingBuffer, engineInfo.tailRingBuffer);

        auto sizeNeeded =
            sizeof(MI_BATCH_BUFFER_START) +
            sizeof(MI_NOOP) +
            sizeof(MI_LOAD_REGISTER_IMM);
        if (engineInfo.tailRingBuffer + sizeNeeded >= engineInfo.sizeRingBuffer) {
            // Pad the remaining ring with NOOPs
            auto sizeToWrap = engineInfo.sizeRingBuffer - engineInfo.tailRingBuffer;
            memset(pTail, 0, sizeToWrap);
            // write remaining ring
            auto physDumpStart = ggtt->map(ggttTail, sizeToWrap, this->getGTTBits(), this->getMemoryBankForGtt());
            AUB::addMemoryWrite(
                tbxStream,
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
        bbs.setBatchBufferStartAddressGraphicsaddress472(AUB::ptrToPPGTT(batchBuffer));
        bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        *(MI_BATCH_BUFFER_START *)pTail = bbs;
        pTail = ((MI_BATCH_BUFFER_START *)pTail) + 1;

        // Add a NOOP as our tail needs to be aligned to a QWORD
        *(MI_NOOP *)pTail = GfxFamily::cmdInitNoop;
        pTail = ((MI_NOOP *)pTail) + 1;

        // Compute our new ring tail.
        engineInfo.tailRingBuffer = (uint32_t)ptrDiff(pTail, engineInfo.pRingBuffer);

        // Only dump the new commands
        auto ggttDumpStart = ptrOffset(engineInfo.ggttRingBuffer, previousTail);
        auto dumpStart = ptrOffset(engineInfo.pRingBuffer, previousTail);
        auto dumpLength = engineInfo.tailRingBuffer - previousTail;

        // write RCS
        auto physDumpStart = ggtt->map(ggttDumpStart, dumpLength, this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physDumpStart,
            dumpStart,
            dumpLength,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
            AubMemDump::DataTypeHintValues::TraceCommandBuffer);

        // update the RCS mmio tail in the LRCA
        auto physLRCA = ggtt->map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRingBuffer), this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physLRCA + 0x101c,
            &engineInfo.tailRingBuffer,
            sizeof(engineInfo.tailRingBuffer),
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

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
void TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion() {
    if (hardwareContextController) {
        hardwareContextController->pollForCompletion();
        return;
    }

    typedef typename AubMemDump::CmdServicesMemTraceRegisterPoll CmdServicesMemTraceRegisterPoll;

    auto mmioBase = this->getCsTraits(osContext->getEngineType()).mmioBase;
    bool pollNotEqual = getpollNotEqualValueForPollForCompletion();
    uint32_t mask = getMaskAndValueForPollForCompletion();
    uint32_t value = mask;
    tbxStream.registerPoll(
        AubMemDump::computeRegisterOffset(mmioBase, 0x2234), //EXECLIST_STATUS
        mask,
        value,
        pollNotEqual,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) {

    AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        AUB::reserveAddressGGTTAndWriteMmeory(tbxStream, static_cast<uintptr_t>(gpuAddress), cpuAddress, physAddress, size, offset, entryBits,
                                              aubHelperHw);
    };

    ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, entryBits, walker, memoryBank);
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation) {
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

    if (AubHelper::isOneTimeAubWritableAllocationType(gfxAllocation.getAllocationType())) {
        gfxAllocation.setAubWritable(false);
    }

    return true;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) {
    this->flushBatchedSubmissions();

    while (*this->getTagAddress() < this->latestFlushedTaskCount) {
        downloadAllocation(*this->getTagAllocation());
    }

    for (GraphicsAllocation *graphicsAllocation : this->allocationsForDownload) {
        downloadAllocation(*graphicsAllocation);
    }
    this->allocationsForDownload.clear();

    BaseClass::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, forcePowerSavingMode);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::processEviction() {
    this->allocationsForDownload.insert(this->getEvictionAllocations().begin(), this->getEvictionAllocations().end());
    BaseClass::processEviction();
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency) {
    for (auto &gfxAllocation : allocationsForResidency) {
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!((gfxAllocation->getUnderlyingBufferSize() == 0) || !gfxAllocation->isAubWritable()));
        }
        gfxAllocation->updateResidencyTaskCount(this->taskCount + 1, this->osContext->getContextId());
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::downloadAllocation(GraphicsAllocation &gfxAllocation) {
    if (hardwareContextController) {
        hardwareContextController->readMemory(gfxAllocation.getGpuAddress(), gfxAllocation.getUnderlyingBuffer(), gfxAllocation.getUnderlyingBufferSize(),
                                              this->getMemoryBank(&gfxAllocation), MemoryConstants::pageSize64k);
        return;
    }

    auto cpuAddress = gfxAllocation.getUnderlyingBuffer();
    auto gpuAddress = gfxAllocation.getGpuAddress();
    auto length = gfxAllocation.getUnderlyingBufferSize();

    if (length) {
        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
            DEBUG_BREAK_IF(offset > length);
            tbxStream.readMemory(physAddress, ptrOffset(cpuAddress, offset), size);
        };
        ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), length, 0, 0, walker, this->getMemoryBank(&gfxAllocation));
    }
}

template <typename GfxFamily>
uint32_t TbxCommandStreamReceiverHw<GfxFamily>::getMaskAndValueForPollForCompletion() const {
    return 0x100;
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::getpollNotEqualValueForPollForCompletion() const {
    return false;
}
} // namespace NEO
