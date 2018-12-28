/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/aub/aub_center.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/physical_address_allocator.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"
#include <cstring>

namespace OCLRT {

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw(const HardwareInfo &hwInfoIn,
                                                                  ExecutionEnvironment &executionEnvironment)
    : BaseClass(hwInfoIn, executionEnvironment) {

    physicalAddressAllocator.reset(this->createPhysicalAddressAllocator(&hwInfoIn));
    executionEnvironment.initAubCenter(&this->peekHwInfo(), this->localMemoryEnabled, "");
    auto aubCenter = executionEnvironment.aubCenter.get();
    UNRECOVERABLE_IF(nullptr == aubCenter);

    aubManager = aubCenter->getAubManager();
    if (aubManager) {
        hardwareContext = std::unique_ptr<HardwareContext>(aubManager->createHardwareContext(0, hwInfoIn.capabilityTable.defaultEngineType));
    }

    ppgtt = std::make_unique<std::conditional<is64bit, PML4, PDPE>::type>(physicalAddressAllocator.get());
    ggtt = std::make_unique<PDPE>(physicalAddressAllocator.get());

    for (auto &engineInfo : engineInfoTable) {
        engineInfo.pLRCA = nullptr;
        engineInfo.ggttLRCA = 0u;
        engineInfo.pGlobalHWStatusPage = nullptr;
        engineInfo.ggttHWSP = 0u;
        engineInfo.pRCS = nullptr;
        engineInfo.ggttRCS = 0u;
        engineInfo.sizeRCS = 0;
        engineInfo.tailRCS = 0;
    }
    auto debugDeviceId = DebugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? hwInfoIn.capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
    this->stream = &tbxStream;
}

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::~TbxCommandStreamReceiverHw() {
    if (streamInitialized) {
        tbxStream.close();
    }

    for (auto &engineInfo : engineInfoTable) {
        alignedFree(engineInfo.pLRCA);
        gttRemap.unmap(engineInfo.pLRCA);
        engineInfo.pLRCA = nullptr;

        alignedFree(engineInfo.pGlobalHWStatusPage);
        gttRemap.unmap(engineInfo.pGlobalHWStatusPage);
        engineInfo.pGlobalHWStatusPage = nullptr;

        alignedFree(engineInfo.pRCS);
        gttRemap.unmap(engineInfo.pRCS);
        engineInfo.pRCS = nullptr;
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::initializeEngine(size_t engineIndex) {
    if (hardwareContext) {
        hardwareContext->initialize();
        return;
    }

    auto engineInstance = allEngineInstances[engineIndex];
    auto csTraits = this->getCsTraits(engineInstance);
    auto &engineInfo = engineInfoTable[engineIndex];

    if (engineInfo.pLRCA) {
        return;
    }

    this->initGlobalMMIO();
    this->initEngineMMIO(engineInstance);
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
    engineInfo.sizeRCS = 0x4 * 0x1000;
    {
        const size_t alignRCS = 0x1000;
        engineInfo.pRCS = alignedMalloc(engineInfo.sizeRCS, alignRCS);
        engineInfo.ggttRCS = gttRemap.map(engineInfo.pRCS, engineInfo.sizeRCS);
        auto physRCS = ggtt->map(engineInfo.ggttRCS, engineInfo.sizeRCS, this->getGTTBits(), this->getMemoryBankForGtt());

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physRCS), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttRCS, engineInfo.sizeRCS, physRCS, data);
    }

    // Initialize the ring MMIO registers
    {
        uint32_t ringHead = 0x000;
        uint32_t ringTail = 0x000;
        auto ringBase = engineInfo.ggttRCS;
        auto ringCtrl = (uint32_t)((engineInfo.sizeRCS - 0x1000) | 1);
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
CommandStreamReceiver *TbxCommandStreamReceiverHw<GfxFamily>::create(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    TbxCommandStreamReceiverHw<GfxFamily> *csr;
    if (withAubDump) {
        csr = new CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<GfxFamily>>(hwInfoIn, executionEnvironment);
    } else {
        csr = new TbxCommandStreamReceiverHw<GfxFamily>(hwInfoIn, executionEnvironment);
    }

    if (csr->hardwareContext == nullptr) {
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
    auto engineIndex = this->getEngineIndex(osContext->getEngineType());

    initializeEngine(engineIndex);

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

    submitBatchBuffer(engineIndex, batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer, this->getMemoryBank(batchBuffer.commandBufferAllocation), this->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));

    pollForCompletion(osContext->getEngineType());
    return 0;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::submitBatchBuffer(size_t engineIndex, uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) {
    if (hardwareContext) {
        if (batchBufferSize) {
            hardwareContext->submit(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank);
        }
        return;
    }

    auto engineInstance = allEngineInstances[engineIndex];
    auto csTraits = this->getCsTraits(engineInstance);
    auto &engineInfo = engineInfoTable[engineIndex];

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
    auto previousTail = engineInfo.tailRCS;
    {
        typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
        typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
        typedef typename GfxFamily::MI_NOOP MI_NOOP;

        auto pTail = ptrOffset(engineInfo.pRCS, engineInfo.tailRCS);
        auto ggttTail = ptrOffset(engineInfo.ggttRCS, engineInfo.tailRCS);

        auto sizeNeeded =
            sizeof(MI_BATCH_BUFFER_START) +
            sizeof(MI_NOOP) +
            sizeof(MI_LOAD_REGISTER_IMM);
        if (engineInfo.tailRCS + sizeNeeded >= engineInfo.sizeRCS) {
            // Pad the remaining ring with NOOPs
            auto sizeToWrap = engineInfo.sizeRCS - engineInfo.tailRCS;
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
            engineInfo.tailRCS = 0;
            pTail = engineInfo.pRCS;
        } else if (engineInfo.tailRCS == 0) {
            // Add a LRI if this is our first submission
            auto lri = MI_LOAD_REGISTER_IMM::sInit();
            lri.setRegisterOffset(AubMemDump::computeRegisterOffset(csTraits.mmioBase, 0x2244));
            lri.setDataDword(0x00010000);
            *(MI_LOAD_REGISTER_IMM *)pTail = lri;
            pTail = ((MI_LOAD_REGISTER_IMM *)pTail) + 1;
        }

        // Add our BBS
        auto bbs = MI_BATCH_BUFFER_START::sInit();
        bbs.setBatchBufferStartAddressGraphicsaddress472(AUB::ptrToPPGTT(batchBuffer));
        bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        *(MI_BATCH_BUFFER_START *)pTail = bbs;
        pTail = ((MI_BATCH_BUFFER_START *)pTail) + 1;

        // Add a NOOP as our tail needs to be aligned to a QWORD
        *(MI_NOOP *)pTail = MI_NOOP::sInit();
        pTail = ((MI_NOOP *)pTail) + 1;

        // Compute our new ring tail.
        engineInfo.tailRCS = (uint32_t)ptrDiff(pTail, engineInfo.pRCS);

        // Only dump the new commands
        auto ggttDumpStart = ptrOffset(engineInfo.ggttRCS, previousTail);
        auto dumpStart = ptrOffset(engineInfo.pRCS, previousTail);
        auto dumpLength = engineInfo.tailRCS - previousTail;

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
        auto physLRCA = ggtt->map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRCS), this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physLRCA + 0x101c,
            &engineInfo.tailRCS,
            sizeof(engineInfo.tailRCS),
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

        DEBUG_BREAK_IF(engineInfo.tailRCS >= engineInfo.sizeRCS);
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

        this->submitLRCA(engineInstance, contextDescriptor);
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion(EngineInstanceT engineInstance) {
    if (hardwareContext) {
        hardwareContext->pollForCompletion();
        return;
    }

    typedef typename AubMemDump::CmdServicesMemTraceRegisterPoll CmdServicesMemTraceRegisterPoll;

    auto mmioBase = this->getCsTraits(engineInstance).mmioBase;
    bool pollNotEqual = false;
    tbxStream.registerPoll(
        AubMemDump::computeRegisterOffset(mmioBase, 0x2234), //EXECLIST_STATUS
        0x100,
        0x100,
        pollNotEqual,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits, DevicesBitfield devicesBitfield) {
    if (hardwareContext) {
        int hint = AubMemDump::DataTypeHintValues::TraceNotype;
        hardwareContext->writeMemory(gpuAddress, cpuAddress, size, memoryBank, hint);
        return;
    }

    AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        AUB::reserveAddressGGTTAndWriteMmeory(tbxStream, static_cast<uintptr_t>(gpuAddress), cpuAddress, physAddress, size, offset, entryBits,
                                              aubHelperHw);
    };

    ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, entryBits, walker, memoryBank);
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation) {
    auto cpuAddress = gfxAllocation.getUnderlyingBuffer();
    auto gpuAddress = gfxAllocation.getGpuAddress();
    auto size = gfxAllocation.getUnderlyingBufferSize();

    if (size == 0)
        return false;

    writeMemory(gpuAddress, cpuAddress, size, this->getMemoryBank(&gfxAllocation), this->getPPGTTAdditionalBits(&gfxAllocation), gfxAllocation.devicesBitfield);

    return true;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency) {
    for (auto &gfxAllocation : allocationsForResidency) {
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!(gfxAllocation->getUnderlyingBufferSize() == 0));
        }
        gfxAllocation->updateResidencyTaskCount(this->taskCount + 1, this->osContext->getContextId());
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::makeCoherent(GraphicsAllocation &gfxAllocation) {
    if (hardwareContext) {
        hardwareContext->readMemory(gfxAllocation.getGpuAddress(), gfxAllocation.getUnderlyingBuffer(), gfxAllocation.getUnderlyingBufferSize());
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
void TbxCommandStreamReceiverHw<GfxFamily>::waitBeforeMakingNonResidentWhenRequired() {
    auto allocation = this->getTagAllocation();
    UNRECOVERABLE_IF(allocation == nullptr);

    while (*this->getTagAddress() < this->latestFlushedTaskCount) {
        this->makeCoherent(*allocation);
    }
}
} // namespace OCLRT
