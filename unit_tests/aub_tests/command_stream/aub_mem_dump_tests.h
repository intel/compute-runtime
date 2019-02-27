/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/device/device.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "test.h"

#include "aub_mapper.h"

namespace Os {
extern const char *fileSeparator;
}

extern std::string getAubFileName(const OCLRT::Device *pDevice, const std::string baseName);

template <typename FamilyType>
void setupAUB(const OCLRT::Device *pDevice, OCLRT::EngineType engineType) {
    typedef typename OCLRT::AUBFamilyMapper<FamilyType>::AUB AUB;
    const auto &csTraits = OCLRT::CommandStreamReceiverSimulatedCommonHw<FamilyType>::getCsTraits(engineType);
    auto mmioBase = csTraits.mmioBase;
    uint64_t physAddress = 0x10000;

    OCLRT::AUBCommandStreamReceiver::AubFileStream aubFile;
    std::string filePath(OCLRT::folderAUB);
    filePath.append(Os::fileSeparator);
    std::string baseName("simple");
    baseName.append(csTraits.name);
    baseName.append(".aub");
    filePath.append(getAubFileName(pDevice, baseName));

    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    aubFile.writeMMIO(mmioBase + 0x229c, 0xffff8280);

    const size_t sizeHWSP = 0x1000;
    const size_t sizeRing = 0x4 * 0x1000;

    const size_t sizeTotal = alignUp((sizeHWSP + sizeRing + csTraits.sizeLRCA), 0x1000);
    const size_t alignTotal = sizeTotal;

    auto totalBuffer = alignedMalloc(sizeTotal, alignTotal);
    size_t totalBufferOffset = 0;

    auto pGlobalHWStatusPage = totalBuffer;
    totalBufferOffset += sizeHWSP;
    uint32_t ggttGlobalHardwareStatusPage = (uint32_t)((uintptr_t)pGlobalHWStatusPage);
    AubGTTData data = {true, false};
    AUB::reserveAddressGGTT(aubFile, ggttGlobalHardwareStatusPage, sizeHWSP, physAddress, data);
    physAddress += sizeHWSP;

    aubFile.writeMMIO(mmioBase + 0x2080, ggttGlobalHardwareStatusPage);

    size_t sizeCommands = 0;
    auto pRing = ptrOffset<void *>(totalBuffer, totalBufferOffset);
    totalBufferOffset += sizeRing;

    auto ggttRing = (uint32_t)(uintptr_t)pRing;
    auto physRing = physAddress;
    physAddress += sizeRing;
    auto rRing = AUB::reserveAddressGGTT(aubFile, ggttRing, sizeRing, physRing, data);
    ASSERT_NE(static_cast<uint64_t>(-1), rRing);
    EXPECT_EQ(rRing, physRing);

    uint32_t noopId = 0xbaadd;
    auto cur = (uint32_t *)pRing;

    using MI_NOOP = typename FamilyType::MI_NOOP;
    auto noop = FamilyType::cmdInitNoop;
    *cur++ = noop.TheStructure.RawData[0];
    *cur++ = noop.TheStructure.RawData[0];
    *cur++ = noop.TheStructure.RawData[0];
    noop.TheStructure.Common.IdentificationNumberRegisterWriteEnable = true;
    noop.TheStructure.Common.IdentificationNumber = noopId;
    *cur++ = noop.TheStructure.RawData[0];

    sizeCommands = ptrDiff(cur, pRing);

    AUB::addMemoryWrite(aubFile, physRing, pRing, sizeCommands, AubMemDump::AddressSpaceValues::TraceNonlocal, csTraits.aubHintCommandBuffer);

    auto sizeLRCA = csTraits.sizeLRCA;
    auto pLRCABase = ptrOffset<void *>(totalBuffer, totalBufferOffset);
    totalBufferOffset += csTraits.sizeLRCA;

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

    aubFile.writeMMIO(mmioBase + 0x2230, 0);
    aubFile.writeMMIO(mmioBase + 0x2230, 0);
    aubFile.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[1]);
    aubFile.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[0]);

    alignedFree(totalBuffer);

    aubFile.fileHandle.close();
}
