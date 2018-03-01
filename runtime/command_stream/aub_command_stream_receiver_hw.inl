/*
 * Copyright (c) 2017 -2018, Intel Corporation
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

#include "hw_cmds.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include <cstring>

namespace OCLRT {

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, bool standalone)
    : BaseClass(hwInfoIn),
      standalone(standalone) {
    this->dispatchMode = CommandStreamReceiver::DispatchMode::BatchedDispatch;
    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (CommandStreamReceiver::DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }
    for (auto &engineInfo : engineInfoTable) {
        engineInfo.pLRCA = nullptr;
        engineInfo.ggttLRCA = 0u;
        engineInfo.pGlobalHWStatusPage = nullptr;
        engineInfo.ggttHWSP = 0u;
        engineInfo.pRingBuffer = nullptr;
        engineInfo.ggttRingBuffer = 0u;
        engineInfo.sizeRingBuffer = 0;
        engineInfo.tailRingBuffer = 0;
    }
}

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::~AUBCommandStreamReceiverHw() {
    stream.close();

    for (auto &engineInfo : engineInfoTable) {
        alignedFree(engineInfo.pLRCA);
        gttRemap.unmap(engineInfo.pLRCA);
        engineInfo.pLRCA = nullptr;

        alignedFree(engineInfo.pGlobalHWStatusPage);
        gttRemap.unmap(engineInfo.pGlobalHWStatusPage);
        engineInfo.pGlobalHWStatusPage = nullptr;

        alignedFree(engineInfo.pRingBuffer);
        gttRemap.unmap(engineInfo.pRingBuffer);
        engineInfo.pRingBuffer = nullptr;
    }
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &AUBCommandStreamReceiverHw<GfxFamily>::getCsTraits(EngineType engineType) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineType];
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<GfxFamily>::globalMMIO) {
        stream.writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initEngineMMIO(EngineType engineType) {
    auto mmioList = AUBFamilyMapper<GfxFamily>::perEngineMMIO[engineType];

    DEBUG_BREAK_IF(!mmioList);
    for (auto &mmioPair : *mmioList) {
        stream.writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initializeEngine(EngineType engineType) {
    auto mmioBase = getCsTraits(engineType).mmioBase;
    auto &engineInfo = engineInfoTable[engineType];

    initGlobalMMIO();
    initEngineMMIO(engineType);

    // Global HW Status Page
    {
        const size_t sizeHWSP = 0x1000;
        const size_t alignHWSP = 0x1000;
        engineInfo.pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);
        engineInfo.ggttHWSP = gttRemap.map(engineInfo.pGlobalHWStatusPage, sizeHWSP);
        auto physHWSP = ggtt.map(engineInfo.ggttHWSP, sizeHWSP);

        // Write our GHWSP
        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttHWSP;
            stream.addComment(str.str().c_str());
        }

        AUB::reserveAddressGGTT(stream, engineInfo.ggttHWSP, sizeHWSP, physHWSP);
        stream.writeMMIO(mmioBase + 0x2080, engineInfo.ggttHWSP);
    }

    // Allocate the LRCA
    auto csTraits = getCsTraits(engineType);
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
        engineInfo.ggttRingBuffer = gttRemap.map(engineInfo.pRingBuffer, engineInfo.sizeRingBuffer);
        auto physRingBuffer = ggtt.map(engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer);

        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttRingBuffer;
            stream.addComment(str.str().c_str());
        }

        AUB::reserveAddressGGTT(stream, engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, physRingBuffer);
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
        auto lrcAddressPhys = ggtt.map(engineInfo.ggttLRCA, sizeLRCA);

        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttLRCA;
            stream.addComment(str.str().c_str());
        }

        AUB::reserveAddressGGTT(stream, engineInfo.ggttLRCA, sizeLRCA, lrcAddressPhys);
        AUB::addMemoryWrite(
            stream,
            lrcAddressPhys,
            pLRCABase,
            sizeLRCA,
            AubMemDump::AddressSpaceValues::TraceNonlocal,
            csTraits.aubHintLRCA);
    }

    // Create a context to facilitate AUB dumping of memory using PPGTT
    addContextToken();
}

template <typename GfxFamily>
CommandStreamReceiver *AUBCommandStreamReceiverHw<GfxFamily>::create(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone) {
    auto csr = new AUBCommandStreamReceiverHw<GfxFamily>(hwInfoIn, standalone);

    // Open our file
    csr->stream.open(fileName.c_str());

    if (!csr->stream.fileHandle.is_open()) {
        // This DEBUG_BREAK_IF most probably means you are not executing aub tests with correct current directory (containing aub_out folder)
        // try adding <familycodename>_aub
        DEBUG_BREAK_IF(true);
    }
    // Add the file header.
    csr->stream.init(AubMemDump::SteppingValues::A, AUB::Traits::device);

    return csr;
}

template <typename GfxFamily>
FlushStamp AUBCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer,
                                                        EngineType engineType, ResidencyContainer *allocationsForResidency) {
    uint32_t mmioBase = getCsTraits(engineType).mmioBase;
    auto &engineInfo = engineInfoTable[engineType];

    if (!engineInfo.pLRCA) {
        initializeEngine(engineType);
        DEBUG_BREAK_IF(!engineInfo.pLRCA);
    }

    if (this->standalone) {
        if (this->dispatchMode == CommandStreamReceiver::DispatchMode::ImmediateDispatch) {
            makeResident(*batchBuffer.commandBufferAllocation);
        } else {
            allocationsForResidency->push_back(batchBuffer.commandBufferAllocation);
            batchBuffer.commandBufferAllocation->residencyTaskCount = this->taskCount;
        }
        processResidency(allocationsForResidency);
    }

    // Write our batch buffer
    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    DEBUG_BREAK_IF(currentOffset < batchBuffer.startOffset);
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;
    {
        {
            std::ostringstream str;
            str << "ppgtt: " << std::hex << std::showbase << pBatchBuffer;
            stream.addComment(str.str().c_str());
        }

        auto physBatchBuffer = ppgtt.map(reinterpret_cast<uintptr_t>(pBatchBuffer), sizeBatchBuffer);
        AUB::reserveAddressPPGTT(stream, reinterpret_cast<uintptr_t>(pBatchBuffer), sizeBatchBuffer, physBatchBuffer);

        AUB::addMemoryWrite(
            stream,
            physBatchBuffer,
            pBatchBuffer,
            sizeBatchBuffer,
            AubMemDump::AddressSpaceValues::TraceNonlocal,
            AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary);
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
            auto physDumpStart = ggtt.map(ggttTail, sizeToWrap);
            AUB::addMemoryWrite(
                stream,
                physDumpStart,
                pTail,
                sizeToWrap,
                AubMemDump::AddressSpaceValues::TraceNonlocal,
                AubMemDump::DataTypeHintValues::TraceCommandBuffer);
            previousTail = 0;
            engineInfo.tailRingBuffer = 0;
            pTail = engineInfo.pRingBuffer;
        } else if (engineInfo.tailRingBuffer == 0) {
            // Add a LRI if this is our first submission
            auto lri = MI_LOAD_REGISTER_IMM::sInit();
            lri.setRegisterOffset(mmioBase + 0x2244);
            lri.setDataDword(0x00010000);
            *(MI_LOAD_REGISTER_IMM *)pTail = lri;
            pTail = ((MI_LOAD_REGISTER_IMM *)pTail) + 1;
        }

        // Add our BBS
        auto bbs = MI_BATCH_BUFFER_START::sInit();
        bbs.setBatchBufferStartAddressGraphicsaddress472(AUB::ptrToPPGTT(pBatchBuffer));
        bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        *(MI_BATCH_BUFFER_START *)pTail = bbs;
        pTail = ((MI_BATCH_BUFFER_START *)pTail) + 1;

        // Compute our new ring tail.
        engineInfo.tailRingBuffer = (uint32_t)ptrDiff(pTail, engineInfo.pRingBuffer);

        // Add NOOPs as needed as our tail needs to be aligned
        while (engineInfo.tailRingBuffer % tailAlignment) {
            *(MI_NOOP *)pTail = MI_NOOP::sInit();
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
            stream.addComment(str.str().c_str());
        }

        auto physDumpStart = ggtt.map(ggttDumpStart, dumpLength);
        AUB::addMemoryWrite(
            stream,
            physDumpStart,
            dumpStart,
            dumpLength,
            AubMemDump::AddressSpaceValues::TraceNonlocal,
            AubMemDump::DataTypeHintValues::TraceCommandBuffer);

        // update the ring mmio tail in the LRCA
        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttLRCA + 0x101c;
            stream.addComment(str.str().c_str());
        }

        auto physLRCA = ggtt.map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRingBuffer));
        AUB::addMemoryWrite(
            stream,
            physLRCA + 0x101c,
            &engineInfo.tailRingBuffer,
            sizeof(engineInfo.tailRingBuffer),
            AubMemDump::AddressSpaceValues::TraceNonlocal);

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

        submitLRCA(engineType, contextDescriptor);
    }

    if (this->standalone) {
        pollForCompletion(engineType);
    }
    return 0;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::submitLRCA(EngineType engineType, const typename AUBCommandStreamReceiverHw<GfxFamily>::MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(engineType).mmioBase;
    stream.writeMMIO(mmioBase + 0x2230, 0);
    stream.writeMMIO(mmioBase + 0x2230, 0);
    stream.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[1]);
    stream.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[0]);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion(EngineType engineType) {
    typedef typename AubMemDump::CmdServicesMemTraceRegisterPoll CmdServicesMemTraceRegisterPoll;

    auto mmioBase = getCsTraits(engineType).mmioBase;
    bool pollNotEqual = false;
    this->stream.registerPoll(
        mmioBase + 0x2234, //EXECLIST_STATUS
        0x100,
        0x100,
        pollNotEqual,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::makeResident(GraphicsAllocation &gfxAllocation) {
    auto submissionTaskCount = this->taskCount + 1;
    if (gfxAllocation.residencyTaskCount < (int)submissionTaskCount) {
        this->getMemoryManager()->pushAllocationForResidency(&gfxAllocation);
    }
    gfxAllocation.residencyTaskCount = submissionTaskCount;
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation) {
    auto cpuAddress = gfxAllocation.getUnderlyingBuffer();
    auto gpuAddress = Gmm::decanonize(gfxAllocation.getGpuAddress());
    auto size = gfxAllocation.getUnderlyingBufferSize();
    auto allocType = gfxAllocation.getAllocationType();

    if ((size == 0) || !!(allocType & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE))
        return false;

    {
        std::ostringstream str;
        str << "ppgtt: " << std::hex << std::showbase << gpuAddress;
        stream.addComment(str.str().c_str());
    }

    if (cpuAddress == nullptr) {
        DEBUG_BREAK_IF(gfxAllocation.isLocked());
        cpuAddress = this->getMemoryManager()->lockResource(&gfxAllocation);
        gfxAllocation.setLocked(true);
    }

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
        static const size_t pageSize = 4096;
        auto vmAddr = (static_cast<uintptr_t>(gpuAddress) + offset) & ~(pageSize - 1);
        auto pAddr = physAddress & ~(pageSize - 1);

        AUB::reserveAddressPPGTT(stream, vmAddr, pageSize, pAddr);
        AUB::addMemoryWrite(stream, physAddress,
                            reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(cpuAddress) + offset),
                            size, AubMemDump::AddressSpaceValues::TraceNonlocal);
    };
    ppgtt.pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, walker);

    if (gfxAllocation.isLocked()) {
        this->getMemoryManager()->unlockResource(&gfxAllocation);
        gfxAllocation.setLocked(false);
    }

    if (!!(allocType & GraphicsAllocation::ALLOCATION_TYPE_BUFFER) ||
        !!(allocType & GraphicsAllocation::ALLOCATION_TYPE_IMAGE))
        gfxAllocation.setAllocationType(allocType | GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE);

    return true;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer *allocationsForResidency) {
    auto &residencyAllocations = allocationsForResidency ? *allocationsForResidency : this->getMemoryManager()->getResidencyAllocations();

    for (auto &gfxAllocation : residencyAllocations) {
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!((gfxAllocation->getUnderlyingBufferSize() == 0) ||
                             !!(gfxAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE)));
        }
        gfxAllocation->residencyTaskCount = this->taskCount + 1;
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.residencyTaskCount != ObjectNotResident) {
        this->getMemoryManager()->pushAllocationForEviction(&gfxAllocation);
        gfxAllocation.residencyTaskCount = ObjectNotResident;
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::addContextToken() {
    // Some simulator versions don't support adding the context token.
    // This hook allows specialization for those that do.
}
} // namespace OCLRT
