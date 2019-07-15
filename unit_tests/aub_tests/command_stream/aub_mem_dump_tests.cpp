/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump_tests.h"

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_aub_csr.h"

using NEO::AUBCommandStreamReceiver;
using NEO::AUBCommandStreamReceiverHw;
using NEO::AUBFamilyMapper;
using NEO::DeviceFixture;
using NEO::folderAUB;

std::string getAubFileName(const NEO::Device *pDevice, const std::string baseName) {
    const auto pGtSystemInfo = &pDevice->getHardwareInfo().gtSystemInfo;
    std::stringstream strfilename;
    uint32_t subSlicesPerSlice = pGtSystemInfo->SubSliceCount / pGtSystemInfo->SliceCount;
    strfilename << pDevice->getProductAbbrev() << "_" << pGtSystemInfo->SliceCount << "x" << subSlicesPerSlice << "x" << pGtSystemInfo->MaxEuPerSubSlice << "_" << baseName;

    return strfilename.str();
}

TEST(PageTableTraits, when48BitTraitsAreUsedThenPageTableAddressesAreCorrect) {
    EXPECT_EQ(BIT(32), AubMemDump::PageTableTraits<48>::ptBaseAddress);
    EXPECT_EQ(BIT(31), AubMemDump::PageTableTraits<48>::pdBaseAddress);
    EXPECT_EQ(BIT(30), AubMemDump::PageTableTraits<48>::pdpBaseAddress);
    EXPECT_EQ(BIT(29), AubMemDump::PageTableTraits<48>::pml4BaseAddress);
}

TEST(PageTableTraits, when32BitTraitsAreUsedThenPageTableAddressesAreCorrect) {
    EXPECT_EQ(BIT(38), AubMemDump::PageTableTraits<32>::ptBaseAddress);
    EXPECT_EQ(BIT(37), AubMemDump::PageTableTraits<32>::pdBaseAddress);
    EXPECT_EQ(BIT(36), AubMemDump::PageTableTraits<32>::pdpBaseAddress);
}

typedef Test<DeviceFixture> AubMemDumpTests;

HWTEST_F(AubMemDumpTests, givenAubFileStreamWhenOpenAndCloseIsCalledThenFileNameIsReportedCorrectly) {
    AUBCommandStreamReceiver::AubFileStream aubFile;
    std::string fileName = "file_name.aub";
    aubFile.open(fileName.c_str());
    EXPECT_STREQ(fileName.c_str(), aubFile.getFileName().c_str());

    aubFile.close();
    EXPECT_STREQ("", aubFile.getFileName().c_str());
}

HWTEST_F(AubMemDumpTests, testHeader) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, "header.aub"));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, reserveMaxAddress) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, "reserveMaxAddress.aub"));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto hwInfo = pDevice->getHardwareInfo();
    auto deviceId = hwInfo.capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    auto gAddress = static_cast<uintptr_t>(-1) - 4096;
    auto pAddress = static_cast<uint64_t>(gAddress) & 0xFFFFFFFF;

    auto enableLocalMemory = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getEnableLocalMemory(hwInfo);
    NEO::AubHelperHw<FamilyType> aubHelperHw(enableLocalMemory);
    AUB::reserveAddressPPGTT(aubFile, gAddress, 4096, pAddress, 7, aubHelperHw);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, DISABLED_writeVerifyOneBytePPGTT) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, "writeVerifyOneBytePPGTT.aub"));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    uint8_t byte = 0xbf;
    auto gAddress = reinterpret_cast<uintptr_t>(&byte);
    uint64_t physAddress = reinterpret_cast<uint64_t>(&byte) & 0xFFFFFFFF;

    NEO::AubHelperHw<FamilyType> aubHelperHw(false);
    AUB::reserveAddressPPGTT(aubFile, gAddress, sizeof(byte), physAddress, 7, aubHelperHw);
    AUB::addMemoryWrite(aubFile, physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal,
                         AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, writeVerifyOneByteGGTT) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, "writeVerifyOneByteGGTT.aub"));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    uint8_t byte = 0xbf;
    uint64_t physAddress = reinterpret_cast<uint64_t>(&byte) & 0xFFFFFFFF;
    AubGTTData data = {true, false};
    AUB::reserveAddressGGTT(aubFile, &byte, sizeof(byte), physAddress, data);
    AUB::addMemoryWrite(aubFile, physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal,
                         AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, writeVerifySevenBytesPPGTT) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, "writeVerifySevenBytesPPGTT.aub"));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    uint8_t bytes[] = {0, 1, 2, 3, 4, 5, 6};
    auto gAddress = reinterpret_cast<uintptr_t>(bytes);
    auto physAddress = reinterpret_cast<uint64_t>(bytes) & 0xFFFFFFFF;

    NEO::AubHelperHw<FamilyType> aubHelperHw(false);
    AUB::reserveAddressPPGTT(aubFile, gAddress, sizeof(bytes), physAddress, 7, aubHelperHw);
    AUB::addMemoryWrite(aubFile, physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal,
                         AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, writeVerifySevenBytesGGTT) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, "writeVerifySevenBytesGGTT.aub"));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    uint8_t bytes[] = {0, 1, 2, 3, 4, 5, 6};
    uint64_t physAddress = reinterpret_cast<uint64_t>(bytes) & 0xFFFFFFFF;
    AubGTTData data = {true, false};
    AUB::reserveAddressGGTT(aubFile, bytes, sizeof(bytes), physAddress, data);
    AUB::addMemoryWrite(aubFile, physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal,
                         AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, simpleRCS) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_RCS);
}

HWTEST_F(AubMemDumpTests, simpleBCS) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_BCS);
}

HWTEST_F(AubMemDumpTests, simpleVCS) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_VCS);
}

HWTEST_F(AubMemDumpTests, simpleVECS) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_VECS);
}

TEST(AubMemDumpBasic, givenDebugOverrideMmioWhenMmioNotMatchThenDoNotAlterValue) {
    DebugManagerStateRestore dbgRestore;

    uint32_t dbgOffset = 0x1000;
    uint32_t dbgValue = 0xDEAD;
    DebugManager.flags.AubDumpOverrideMmioRegister.set(static_cast<int32_t>(dbgOffset));
    DebugManager.flags.AubDumpOverrideMmioRegisterValue.set(static_cast<int32_t>(dbgValue));

    uint32_t offset = 0x2000;
    uint32_t value = 0x3000;
    MMIOPair mmio = std::make_pair(offset, value);

    MockAubFileStreamMockMmioWrite mockAubStream;
    mockAubStream.writeMMIO(offset, value);
    EXPECT_EQ(1u, mockAubStream.mmioList.size());
    EXPECT_TRUE(mockAubStream.isOnMmioList(mmio));
}

TEST(AubMemDumpBasic, givenDebugOverrideMmioWhenMmioMatchThenAlterValue) {
    DebugManagerStateRestore dbgRestore;
    uint32_t dbgOffset = 0x2000;
    uint32_t dbgValue = 0xDEAD;
    MMIOPair dbgMmio = std::make_pair(dbgOffset, dbgValue);

    DebugManager.flags.AubDumpOverrideMmioRegister.set(static_cast<int32_t>(dbgOffset));
    DebugManager.flags.AubDumpOverrideMmioRegisterValue.set(static_cast<int32_t>(dbgValue));

    uint32_t offset = 0x2000;
    uint32_t value = 0x3000;

    MockAubFileStreamMockMmioWrite mockAubStream;
    mockAubStream.writeMMIO(offset, value);
    EXPECT_EQ(1u, mockAubStream.mmioList.size());
    EXPECT_TRUE(mockAubStream.isOnMmioList(dbgMmio));
}
