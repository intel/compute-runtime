/*
* Copyright (c) 2018, Intel Corporation
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

#pragma once
#include "unit_tests/aub_tests/command_stream/aub_mem_dump_tests.h"

template <typename FamilyType>
void setupAUBWithBatchBuffer(const OCLRT::Device *pDevice, OCLRT::EngineType engineType, uint64_t gpuBatchBufferAddr) {
    typedef typename OCLRT::AUBFamilyMapper<FamilyType>::AUB AUB;
    const auto &csTraits = OCLRT::AUBCommandStreamReceiverHw<FamilyType>::getCsTraits(engineType);
    auto mmioBase = csTraits.mmioBase;
    uint64_t physAddress = 0x10000;

    OCLRT::AUBCommandStreamReceiver::AubFileStream aubFile;
    std::string filePath(OCLRT::folderAUB);
    filePath.append(Os::fileSeparator);
    std::string baseName("simple");
    baseName.append(csTraits.name);
    baseName.append("WithBatchBuffer");
    baseName.append(".aub");
    filePath.append(getAubFileName(pDevice, baseName));

    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    aubFile.init(AubMemDump::SteppingValues::A, AUB::Traits::device);

    aubFile.writeMMIO(mmioBase + 0x229c, 0xffff8280);

    const size_t sizeHWSP = 0x1000;
    const size_t alignHWSP = 0x1000;
    auto pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);

    uint32_t ggttGlobalHardwareStatusPage = (uint32_t)((uintptr_t)pGlobalHWStatusPage);
    AubGTTData data = {true, true, true};
    AUB::reserveAddressGGTT(aubFile, ggttGlobalHardwareStatusPage, sizeHWSP, physAddress, data);
    physAddress += sizeHWSP;

    aubFile.writeMMIO(mmioBase + 0x2080, ggttGlobalHardwareStatusPage);

    using MI_NOOP = typename FamilyType::MI_NOOP;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    // create a user mode batch buffer
    auto physBatchBuffer = physAddress;
    const auto sizeBatchBuffer = 0x1000;
    auto gpuBatchBuffer = static_cast<uintptr_t>(gpuBatchBufferAddr);
    physAddress += sizeBatchBuffer;
    AUB::reserveAddressPPGTT(aubFile, gpuBatchBuffer, sizeBatchBuffer, physBatchBuffer, 7);
    uint8_t batchBuffer[sizeBatchBuffer];

    auto noop = MI_NOOP::sInit();
    uint32_t noopId = 0xbaadd;

    {
        auto pBatchBuffer = (void *)batchBuffer;
        *(MI_NOOP *)pBatchBuffer = noop;
        pBatchBuffer = ptrOffset(pBatchBuffer, sizeof(MI_NOOP));
        *(MI_NOOP *)pBatchBuffer = noop;
        pBatchBuffer = ptrOffset(pBatchBuffer, sizeof(MI_NOOP));
        *(MI_NOOP *)pBatchBuffer = noop;
        pBatchBuffer = ptrOffset(pBatchBuffer, sizeof(MI_NOOP));
        noop.TheStructure.Common.IdentificationNumberRegisterWriteEnable = true;
        noop.TheStructure.Common.IdentificationNumber = noopId++;
        *(MI_NOOP *)pBatchBuffer = noop;
        pBatchBuffer = ptrOffset(pBatchBuffer, sizeof(MI_NOOP));
        *(MI_BATCH_BUFFER_END *)pBatchBuffer = MI_BATCH_BUFFER_END::sInit();
        pBatchBuffer = ptrOffset(pBatchBuffer, sizeof(MI_BATCH_BUFFER_END));
        auto sizeBufferUsed = ptrDiff(pBatchBuffer, batchBuffer);

        AUB::addMemoryWrite(aubFile, physBatchBuffer, batchBuffer, sizeBufferUsed, AubMemDump::AddressSpaceValues::TraceNonlocal, AubMemDump::DataTypeHintValues::TraceBatchBuffer);
    }

    const size_t sizeRing = 0x4 * 0x1000;
    const size_t alignRing = 0x1000;
    size_t sizeCommands = 0;
    auto pRing = alignedMalloc(sizeRing, alignRing);

    auto ggttRing = (uint32_t)(uintptr_t)pRing;
    auto physRing = physAddress;
    physAddress += sizeRing;
    auto rRing = AUB::reserveAddressGGTT(aubFile, ggttRing, sizeRing, physRing, data);
    ASSERT_NE(static_cast<uint64_t>(-1), rRing);
    EXPECT_EQ(rRing, physRing);

    auto cur = (uint32_t *)pRing;
    auto bbs = MI_BATCH_BUFFER_START::sInit();
    bbs.setBatchBufferStartAddressGraphicsaddress472(gpuBatchBuffer);
    bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
    *(MI_BATCH_BUFFER_START *)cur = bbs;
    cur = ptrOffset(cur, sizeof(MI_BATCH_BUFFER_START));
    noop.TheStructure.Common.IdentificationNumberRegisterWriteEnable = true;
    noop.TheStructure.Common.IdentificationNumber = noopId;
    *cur++ = noop.TheStructure.RawData[0];

    sizeCommands = ptrDiff(cur, pRing);

    AUB::addMemoryWrite(aubFile, physRing, pRing, sizeCommands, AubMemDump::AddressSpaceValues::TraceNonlocal, csTraits.aubHintCommandBuffer);

    auto sizeLRCA = csTraits.sizeLRCA;
    auto pLRCABase = alignedMalloc(csTraits.sizeLRCA, csTraits.alignLRCA);

    csTraits.initialize(pLRCABase);
    csTraits.setRingHead(pLRCABase, 0x0000);
    csTraits.setRingTail(pLRCABase, static_cast<uint32_t>(sizeCommands));
    csTraits.setRingBase(pLRCABase, ggttRing);
    auto ringCtrl = static_cast<uint32_t>((sizeRing - 0x1000) | 1);
    csTraits.setRingCtrl(pLRCABase, ringCtrl);

    auto ggttLRCA = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pLRCABase));
    auto physLRCA = physAddress;
    physAddress += sizeLRCA;
    AUB::reserveAddressGGTT(aubFile, ggttLRCA, sizeLRCA, physLRCA, data);
    AUB::addMemoryWrite(aubFile, physLRCA, pLRCABase, sizeLRCA, AubMemDump::AddressSpaceValues::TraceNonlocal, csTraits.aubHintLRCA);

    typename AUB::MiContextDescriptorReg contextDescriptor = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

    contextDescriptor.sData.Valid = true;
    contextDescriptor.sData.ForcePageDirRestore = false;
    contextDescriptor.sData.ForceRestore = false;
    contextDescriptor.sData.Legacy = true;
    contextDescriptor.sData.FaultSupport = 0;
    contextDescriptor.sData.PrivilegeAccessOrPPGTT = true;
    contextDescriptor.sData.ADor64bitSupport = AUB::Traits::addressingBits > 32;

    contextDescriptor.sData.LogicalRingCtxAddress = (uintptr_t)pLRCABase / 4096;
    contextDescriptor.sData.ContextID = 0;

    // Submit our exec-list
    aubFile.writeMMIO(mmioBase + 0x2230, 0);
    aubFile.writeMMIO(mmioBase + 0x2230, 0);
    aubFile.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[1]);
    aubFile.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[0]);

    // Poll until HW complete
    using AubMemDump::CmdServicesMemTraceRegisterPoll;
    aubFile.registerPoll(
        mmioBase + 0x2234, //EXECLIST_STATUS
        0x100,
        0x100,
        false,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);

    alignedFree(pRing);
    alignedFree(pLRCABase);
    alignedFree(pGlobalHWStatusPage);

    aubFile.fileHandle.close();
}
