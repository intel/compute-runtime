/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump_tests.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "unit_tests/fixtures/device_fixture.h"

using OCLRT::AUBCommandStreamReceiver;
using OCLRT::AUBCommandStreamReceiverHw;
using OCLRT::AUBFamilyMapper;
using OCLRT::DeviceFixture;
using OCLRT::EngineType;
using OCLRT::folderAUB;

std::string getAubFileName(const OCLRT::Device *pDevice, const std::string baseName) {
    const auto pGtSystemInfo = pDevice->getHardwareInfo().pSysInfo;
    std::stringstream strfilename;
    strfilename << pDevice->getProductAbbrev() << "_" << pGtSystemInfo->SliceCount << "x" << pGtSystemInfo->SubSliceCount << "x" << pGtSystemInfo->MaxEuPerSubSlice << "_" << baseName;

    return strfilename.str();
}

TEST(PageTableTraits, when48BitTraitsAreUsedThenPageTableAddressesAreCorrect) {
    EXPECT_EQ(BIT(34), AubMemDump::PageTableTraits<48>::ptBaseAddress);
    EXPECT_EQ(BIT(33), AubMemDump::PageTableTraits<48>::pdBaseAddress);
    EXPECT_EQ(BIT(32), AubMemDump::PageTableTraits<48>::pdpBaseAddress);
    EXPECT_EQ(BIT(31), AubMemDump::PageTableTraits<48>::pml4BaseAddress);
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
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    auto gAddress = static_cast<uintptr_t>(-1) - 4096;
    auto pAddress = static_cast<uint64_t>(gAddress) & 0xFFFFFFFF;

    OCLRT::AubHelperHw<FamilyType> aubHelperHw(pDevice->getHardwareCapabilities().localMemorySupported);
    AUB::reserveAddressPPGTT(aubFile, gAddress, 4096, pAddress, 7, aubHelperHw);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, writeVerifyOneBytePPGTT) {
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

    OCLRT::AubHelperHw<FamilyType> aubHelperHw(false);
    AUB::reserveAddressPPGTT(aubFile, gAddress, sizeof(byte), physAddress, 7, aubHelperHw);
    AUB::addMemoryWrite(aubFile, physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal);

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
    aubFile.expectMemory(physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal);

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

    OCLRT::AubHelperHw<FamilyType> aubHelperHw(false);
    AUB::reserveAddressPPGTT(aubFile, gAddress, sizeof(bytes), physAddress, 7, aubHelperHw);
    AUB::addMemoryWrite(aubFile, physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal);

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
    aubFile.expectMemory(physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, simpleRCS) {
    setupAUB<FamilyType>(pDevice, EngineType::ENGINE_RCS);
}

HWTEST_F(AubMemDumpTests, simpleBCS) {
    setupAUB<FamilyType>(pDevice, EngineType::ENGINE_BCS);
}

HWTEST_F(AubMemDumpTests, simpleVCS) {
    setupAUB<FamilyType>(pDevice, EngineType::ENGINE_VCS);
}

HWTEST_F(AubMemDumpTests, simpleVECS) {
    setupAUB<FamilyType>(pDevice, EngineType::ENGINE_VECS);
}
