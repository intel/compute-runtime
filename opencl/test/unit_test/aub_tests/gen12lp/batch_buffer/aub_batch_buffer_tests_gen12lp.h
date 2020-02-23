/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/aub/aub_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"

template <typename FamilyType>
void setupAUBWithBatchBuffer(const NEO::Device *pDevice, aub_stream::EngineType engineType, uint64_t gpuBatchBufferAddr) {
    typedef typename NEO::AUBFamilyMapper<FamilyType>::AUB AUB;
    const auto &csTraits = NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType>::getCsTraits(engineType);
    auto mmioBase = csTraits.mmioBase;
    uint64_t physAddress = 0x10000;

    NEO::AUBCommandStreamReceiver::AubFileStream aubFile;
    std::string filePath(NEO::folderAUB);
    filePath.append(Os::fileSeparator);
    std::string baseName("simple");
    baseName.append(csTraits.name);
    baseName.append("WithBatchBuffer");
    baseName.append(".aub");
    filePath.append(getAubFileName(pDevice, baseName));

    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    aubFile.init(AubMemDump::SteppingValues::A, AUB::Traits::device);

    aubFile.writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x229c), 0xffff8280);

    // enable CCS
    if (engineType == aub_stream::ENGINE_CCS) {
        aubFile.writeMMIO(0x0000ce90, 0x00010001);
        aubFile.writeMMIO(0x00014800, 0x00010001);
    }

    const size_t sizeHWSP = 0x1000;
    const size_t alignHWSP = 0x1000;
    auto pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);

    uint32_t ggttGlobalHardwareStatusPage = (uint32_t)((uintptr_t)pGlobalHWStatusPage);
    AubGTTData data = {true, false};
    AUB::reserveAddressGGTT(aubFile, ggttGlobalHardwareStatusPage, sizeHWSP, physAddress, data);
    physAddress += sizeHWSP;

    aubFile.writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2080), ggttGlobalHardwareStatusPage);

    using MI_NOOP = typename FamilyType::MI_NOOP;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    // create a user mode batch buffer
    auto physBatchBuffer = physAddress;
    const auto sizeBatchBuffer = 0x1000;
    auto gpuBatchBuffer = static_cast<uintptr_t>(gpuBatchBufferAddr);
    physAddress += sizeBatchBuffer;

    NEO::AubHelperHw<FamilyType> aubHelperHw(false);
    AUB::reserveAddressPPGTT(aubFile, gpuBatchBuffer, sizeBatchBuffer, physBatchBuffer, 3, aubHelperHw);
    uint8_t batchBuffer[sizeBatchBuffer];

    auto noop = FamilyType::cmdInitNoop;
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
        *(MI_BATCH_BUFFER_END *)pBatchBuffer = FamilyType::cmdInitBatchBufferEnd;
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
    auto bbs = FamilyType::cmdInitBatchBufferStart;
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

    aubFile.writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2510), contextDescriptor.ulData[0]);
    aubFile.writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2514), contextDescriptor.ulData[1]);

    // Load our new exec list
    aubFile.writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2550), 1);

    // Poll until HW complete
    using AubMemDump::CmdServicesMemTraceRegisterPoll;
    aubFile.registerPoll(
        AubMemDump::computeRegisterOffset(mmioBase, 0x2234), //EXECLIST_STATUS
        0x00008000,
        0x00008000,
        false,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);

    alignedFree(pRing);
    alignedFree(pLRCABase);
    alignedFree(pGlobalHWStatusPage);

    aubFile.fileHandle.close();
}
