/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include <cstring>

namespace OCLRT {

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment)
    : BaseClass(hwInfoIn, executionEnvironment),
      stream(std::make_unique<AUBCommandStreamReceiver::AubFileStream>()),
      subCaptureManager(std::make_unique<AubSubCaptureManager>(fileName)),
      standalone(standalone),
      ppgtt(std::make_unique<TypeSelector<PML4, PDPE, sizeof(void *) == 8>::type>()) {
    this->dispatchMode = DispatchMode::BatchedDispatch;
    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }
    if (DebugManager.flags.AUBDumpSubCaptureMode.get()) {
        this->subCaptureManager->subCaptureMode = static_cast<AubSubCaptureManager::SubCaptureMode>(DebugManager.flags.AUBDumpSubCaptureMode.get());
        this->subCaptureManager->subCaptureFilter.dumpKernelStartIdx = static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelStartIdx.get());
        this->subCaptureManager->subCaptureFilter.dumpKernelEndIdx = static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelEndIdx.get());
        if (DebugManager.flags.AUBDumpFilterKernelName.get() != "unk") {
            this->subCaptureManager->subCaptureFilter.dumpKernelName = DebugManager.flags.AUBDumpFilterKernelName.get();
        }
    }
    auto debugDeviceId = DebugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? hwInfoIn.capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
}

template <typename GfxFamily>
AUBCommandStreamReceiverHw<GfxFamily>::~AUBCommandStreamReceiverHw() {
    AUBCommandStreamReceiverHw<GfxFamily>::closeFile();
    freeEngineInfoTable();
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &AUBCommandStreamReceiverHw<GfxFamily>::getCsTraits(EngineType engineType) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineType];
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<GfxFamily>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initEngineMMIO(EngineType engineType) {
    auto mmioList = AUBFamilyMapper<GfxFamily>::perEngineMMIO[engineType];

    DEBUG_BREAK_IF(!mmioList);
    for (auto &mmioPair : *mmioList) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::initFile(const std::string &fileName) {
    stream.reset(new AUBCommandStreamReceiver::AubFileStream());

    // Open our file
    stream->open(fileName.c_str());

    if (!stream->isOpen()) {
        // This DEBUG_BREAK_IF most probably means you are not executing aub tests with correct current directory (containing aub_out folder)
        // try adding <familycodename>_aub
        DEBUG_BREAK_IF(true);
    }
    // Add the file header
    stream->init(AubMemDump::SteppingValues::A, aubDeviceId);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::closeFile() {
    stream->close();
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::isFileOpen() {
    return stream->isOpen();
}

template <typename GfxFamily>
const std::string &AUBCommandStreamReceiverHw<GfxFamily>::getFileName() {
    return stream->getFileName();
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
            stream->addComment(str.str().c_str());
        }

        AubGTTData data = {0};
        getGTTData(reinterpret_cast<void *>(physHWSP), data);
        AUB::reserveAddressGGTT(*stream, engineInfo.ggttHWSP, sizeHWSP, physHWSP, data);
        stream->writeMMIO(mmioBase + 0x2080, engineInfo.ggttHWSP);
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
            stream->addComment(str.str().c_str());
        }

        AubGTTData data = {0};
        getGTTData(reinterpret_cast<void *>(physRingBuffer), data);
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
        engineInfo.ggttLRCA = gttRemap.map(engineInfo.pLRCA, sizeLRCA);
        auto lrcAddressPhys = ggtt.map(engineInfo.ggttLRCA, sizeLRCA);

        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttLRCA;
            stream->addComment(str.str().c_str());
        }

        AubGTTData data = {0};
        getGTTData(reinterpret_cast<void *>(lrcAddressPhys), data);
        AUB::reserveAddressGGTT(*stream, engineInfo.ggttLRCA, sizeLRCA, lrcAddressPhys, data);
        AUB::addMemoryWrite(
            *stream,
            lrcAddressPhys,
            pLRCABase,
            sizeLRCA,
            getAddressSpace(csTraits.aubHintLRCA),
            csTraits.aubHintLRCA);
    }

    // Create a context to facilitate AUB dumping of memory using PPGTT
    addContextToken();
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::freeEngineInfoTable() {
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
CommandStreamReceiver *AUBCommandStreamReceiverHw<GfxFamily>::create(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment) {
    auto csr = new AUBCommandStreamReceiverHw<GfxFamily>(hwInfoIn, fileName, standalone, executionEnvironment);

    if (!csr->subCaptureManager->isSubCaptureMode()) {
        csr->initFile(fileName);
    }

    return csr;
}

template <typename GfxFamily>
FlushStamp AUBCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer,
                                                        EngineType engineType, ResidencyContainer *allocationsForResidency, OsContext &osContext) {
    if (subCaptureManager->isSubCaptureMode()) {
        if (!subCaptureManager->isSubCaptureEnabled()) {
            if (this->standalone) {
                *this->tagAddress = this->peekLatestSentTaskCount();
            }
            return 0;
        }
    }

    uint32_t mmioBase = getCsTraits(engineType).mmioBase;
    auto &engineInfo = engineInfoTable[engineType];

    if (!engineInfo.pLRCA) {
        initializeEngine(engineType);
        DEBUG_BREAK_IF(!engineInfo.pLRCA);
    }

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
        }
    }

    {
        {
            std::ostringstream str;
            str << "ppgtt: " << std::hex << std::showbase << pBatchBuffer;
            stream->addComment(str.str().c_str());
        }

        auto physBatchBuffer = ppgtt->map(static_cast<uintptr_t>(batchBufferGpuAddress), sizeBatchBuffer);
        AUB::reserveAddressPPGTT(*stream, static_cast<uintptr_t>(batchBufferGpuAddress), sizeBatchBuffer, physBatchBuffer,
                                 getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));

        AUB::addMemoryWrite(
            *stream,
            physBatchBuffer,
            pBatchBuffer,
            sizeBatchBuffer,
            getAddressSpace(AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary),
            AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary);
    }

    if (this->standalone) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            if (!DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                CommandStreamReceiver::makeResident(*batchBuffer.commandBufferAllocation);
            }
        } else {
            allocationsForResidency->push_back(batchBuffer.commandBufferAllocation);
            batchBuffer.commandBufferAllocation->residencyTaskCount = this->taskCount;
        }
        processResidency(allocationsForResidency, osContext);
    }
    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        addGUCStartMessage(static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(pBatchBuffer)), engineType);
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
            auto physDumpStart = ggtt.map(ggttTail, sizeToWrap);
            AUB::addMemoryWrite(
                *stream,
                physDumpStart,
                pTail,
                sizeToWrap,
                getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
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
        bbs.setBatchBufferStartAddressGraphicsaddress472(static_cast<uint64_t>(batchBufferGpuAddress));
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
            stream->addComment(str.str().c_str());
        }

        auto physDumpStart = ggtt.map(ggttDumpStart, dumpLength);
        AUB::addMemoryWrite(
            *stream,
            physDumpStart,
            dumpStart,
            dumpLength,
            getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
            AubMemDump::DataTypeHintValues::TraceCommandBuffer);

        // update the ring mmio tail in the LRCA
        {
            std::ostringstream str;
            str << "ggtt: " << std::hex << std::showbase << engineInfo.ggttLRCA + 0x101c;
            stream->addComment(str.str().c_str());
        }

        auto physLRCA = ggtt.map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRingBuffer));
        AUB::addMemoryWrite(
            *stream,
            physLRCA + 0x101c,
            &engineInfo.tailRingBuffer,
            sizeof(engineInfo.tailRingBuffer),
            getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

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
        *this->tagAddress = this->peekLatestSentTaskCount();
    }

    if (subCaptureManager->isSubCaptureMode()) {
        subCaptureManager->disableSubCapture();
    }

    stream->flush();
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
                                                                ppgtt->map(static_cast<uintptr_t>(patchInfoData.sourceAllocation), 1)));
        }

        if (patchInfoData.targetAllocation) {
            allocationsMap.insert(std::pair<uint64_t, uintptr_t>(patchInfoData.targetAllocation,
                                                                 ppgtt->map(static_cast<uintptr_t>(patchInfoData.targetAllocation), 1)));
        }
    }
    bool result = stream->addComment(str.str().c_str());
    this->flatBatchBufferHelper->getPatchInfoCollection().clear();
    if (!result) {
        return false;
    }

    std::ostringstream allocationStr;
    allocationStr << "AllocationsList" << std::endl;
    for (auto &element : allocationsMap) {
        allocationStr << std::hex << element.first << ";" << element.second << std::endl;
    }
    result = stream->addComment(allocationStr.str().c_str());
    if (!result) {
        return false;
    }
    return true;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::submitLRCA(EngineType engineType, const typename AUBCommandStreamReceiverHw<GfxFamily>::MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(engineType).mmioBase;
    stream->writeMMIO(mmioBase + 0x2230, 0);
    stream->writeMMIO(mmioBase + 0x2230, 0);
    stream->writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[1]);
    stream->writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[0]);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion(EngineType engineType) {
    typedef typename AubMemDump::CmdServicesMemTraceRegisterPoll CmdServicesMemTraceRegisterPoll;

    auto mmioBase = getCsTraits(engineType).mmioBase;
    bool pollNotEqual = false;
    this->stream->registerPoll(
        mmioBase + 0x2234, //EXECLIST_STATUS
        0x100,
        0x100,
        pollNotEqual,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
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
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation) {
    auto cpuAddress = gfxAllocation.getUnderlyingBuffer();
    auto gpuAddress = GmmHelper::decanonize(gfxAllocation.getGpuAddress());
    auto size = gfxAllocation.getUnderlyingBufferSize();
    if (gfxAllocation.gmm && gfxAllocation.gmm->isRenderCompressed) {
        size = gfxAllocation.gmm->gmmResourceInfo->getSizeAllocation();
    }

    if ((size == 0) || !gfxAllocation.isAubWritable())
        return false;

    {
        std::ostringstream str;
        str << "ppgtt: " << std::hex << std::showbase << gpuAddress;
        stream->addComment(str.str().c_str());
    }

    if (cpuAddress == nullptr) {
        DEBUG_BREAK_IF(gfxAllocation.isLocked());
        cpuAddress = this->getMemoryManager()->lockResource(&gfxAllocation);
        gfxAllocation.setLocked(true);
    }

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
        AUB::reserveAddressGGTTAndWriteMmeory(*stream, static_cast<uintptr_t>(gpuAddress), cpuAddress, physAddress, size, offset, getPPGTTAdditionalBits(&gfxAllocation));
    };
    ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, walker);

    if (gfxAllocation.isLocked()) {
        this->getMemoryManager()->unlockResource(&gfxAllocation);
        gfxAllocation.setLocked(false);
    }

    if (AubHelper::isOneTimeAubWritableAllocationType(gfxAllocation.getAllocationType())) {
        gfxAllocation.setAubWritable(false);
    }
    return true;
}

template <typename GfxFamily>
bool AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(AllocationView &allocationView) {
    GraphicsAllocation gfxAllocation(reinterpret_cast<void *>(allocationView.first), allocationView.second);
    return writeMemory(gfxAllocation);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer *allocationsForResidency, OsContext &osContext) {
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

    DEBUG_BREAK_IF(allocationsForResidency == nullptr);
    for (auto &gfxAllocation : *allocationsForResidency) {
        if (dumpAubNonWritable) {
            gfxAllocation->setAubWritable(true);
        }
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!((gfxAllocation->getUnderlyingBufferSize() == 0) || !gfxAllocation->isAubWritable()));
        }
        gfxAllocation->residencyTaskCount = this->taskCount + 1;
    }

    dumpAubNonWritable = false;
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.residencyTaskCount != ObjectNotResident) {
        this->getMemoryManager()->pushAllocationForEviction(&gfxAllocation);
        gfxAllocation.residencyTaskCount = ObjectNotResident;
    }
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) {
    bool active = subCaptureManager->activateSubCapture(dispatchInfo);
    if (active) {
        std::string subCaptureFile = subCaptureManager->getSubCaptureFileName(dispatchInfo);
        if (isFileOpen()) {
            if (subCaptureFile != getFileName()) {
                closeFile();
                freeEngineInfoTable();
            }
        }
        if (!isFileOpen()) {
            initFile(subCaptureFile);
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
void AUBCommandStreamReceiverHw<GfxFamily>::addContextToken() {
    // Some simulator versions don't support adding the context token.
    // This hook allows specialization for those that do.
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::addGUCStartMessage(uint64_t batchBufferAddress, EngineType engineType) {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto bufferSize = sizeof(uint32_t) + sizeof(MI_BATCH_BUFFER_START);

    std::unique_ptr<void, std::function<void(void *)>> buffer(this->getMemoryManager()->alignedMallocWrapper(bufferSize, MemoryConstants::pageSize), [&](void *ptr) { this->getMemoryManager()->alignedFreeWrapper(ptr); });
    LinearStream linearStream(buffer.get(), bufferSize);

    uint32_t *header = static_cast<uint32_t *>(linearStream.getSpace(sizeof(uint32_t)));
    *header = getGUCWorkQueueItemHeader(engineType);

    MI_BATCH_BUFFER_START *miBatchBufferStart = linearStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    DEBUG_BREAK_IF(bufferSize != linearStream.getUsed());
    miBatchBufferStart->init();
    miBatchBufferStart->setBatchBufferStartAddressGraphicsaddress472(AUB::ptrToPPGTT(buffer.get()));
    miBatchBufferStart->setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);

    auto physBufferAddres = ppgtt->map(reinterpret_cast<uintptr_t>(buffer.get()), bufferSize);
    AUB::reserveAddressPPGTT(*stream, reinterpret_cast<uintptr_t>(buffer.get()), bufferSize, physBufferAddres, getPPGTTAdditionalBits(linearStream.getGraphicsAllocation()));

    AUB::addMemoryWrite(
        *stream,
        physBufferAddres,
        buffer.get(),
        bufferSize,
        getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

    PatchInfoData patchInfoData(batchBufferAddress, 0u, PatchInfoAllocationType::Default, reinterpret_cast<uintptr_t>(buffer.get()), sizeof(uint32_t) + sizeof(MI_BATCH_BUFFER_START) - sizeof(uint64_t), PatchInfoAllocationType::GUCStartMessage);
    this->flatBatchBufferHelper->setPatchInfoData(patchInfoData);
}

template <typename GfxFamily>
uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getGUCWorkQueueItemHeader(EngineType engineType) {
    uint32_t GUCWorkQueueItemHeader = 0x00030001;
    return GUCWorkQueueItemHeader;
}

template <typename GfxFamily>
uint64_t AUBCommandStreamReceiverHw<GfxFamily>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::userSupervisorBit);
}

template <typename GfxFamily>
void AUBCommandStreamReceiverHw<GfxFamily>::getGTTData(void *memory, AubGTTData &data) {
    data.present = true;
    data.localMemory = false;
}

template <typename GfxFamily>
int AUBCommandStreamReceiverHw<GfxFamily>::getAddressSpace(int hint) {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

} // namespace OCLRT
