/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump_tests.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using NEO::ApiSpecificConfig;
using NEO::AUBCommandStreamReceiver;
using NEO::AUBCommandStreamReceiverHw;
using NEO::AUBFamilyMapper;
using NEO::ClDeviceFixture;
using NEO::folderAUB;

std::string getAubFileName(const NEO::Device *pDevice, const std::string baseName) {
    const auto pGtSystemInfo = &pDevice->getHardwareInfo().gtSystemInfo;
    auto releaseHelper = pDevice->getReleaseHelper();
    std::stringstream strfilename;
    uint32_t subSlicesPerSlice = pGtSystemInfo->SubSliceCount / pGtSystemInfo->SliceCount;
    const auto deviceConfig = AubHelper::getDeviceConfigString(releaseHelper, 1, pGtSystemInfo->SliceCount, subSlicesPerSlice, pGtSystemInfo->MaxEuPerSubSlice);
    strfilename << hardwarePrefix[pDevice->getHardwareInfo().platform.eProductFamily] << "_" << deviceConfig << "_" << baseName;

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

typedef Test<ClDeviceFixture> AubMemDumpTests;

HWTEST_F(AubMemDumpTests, givenAubFileStreamWhenOpenAndCloseIsCalledThenFileNameIsReportedCorrectly) {
    AUBCommandStreamReceiver::AubFileStream aubFile;
    std::string fileName = "file_name.aub";
    aubFile.open(fileName.c_str());
    EXPECT_STREQ(fileName.c_str(), aubFile.getFileName().c_str());

    aubFile.close();
    EXPECT_STREQ("", aubFile.getFileName().c_str());
}

HWTEST_F(AubMemDumpTests, GivenHeaderThenExpectationsAreMet) {
    std::string filePath(folderAUB);
    std::string filenameWithPrefix = ApiSpecificConfig::getAubPrefixForSpecificApi();
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, filenameWithPrefix.append("header.aub")));
    AUBCommandStreamReceiver::AubFileStream aubFile;
    aubFile.fileHandle.open(filePath.c_str(), std::ofstream::binary);

    // Header
    auto deviceId = pDevice->getHardwareInfo().capabilityTable.aubDeviceId;
    aubFile.init(AubMemDump::SteppingValues::A, deviceId);

    aubFile.fileHandle.close();
}

HWTEST_F(AubMemDumpTests, GivenWriteVerifyOneBytePpgttThenExpectationsAreMet) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    std::string filenameWithPrefix = ApiSpecificConfig::getAubPrefixForSpecificApi();
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, filenameWithPrefix.append("writeVerifyOneBytePPGTT.aub")));
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

HWTEST_F(AubMemDumpTests, GivenWriteVerifyOneByteGgttThenExpectationsAreMet) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    std::string filenameWithPrefix = ApiSpecificConfig::getAubPrefixForSpecificApi();
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, filenameWithPrefix.append("writeVerifyOneByteGGTT.aub")));
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

HWTEST_F(AubMemDumpTests, GivenWriteVerifySevenBytesPpgttThenExpectationsAreMet) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    std::string filenameWithPrefix = ApiSpecificConfig::getAubPrefixForSpecificApi();
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, filenameWithPrefix.append("writeVerifySevenBytesPPGTT.aub")));
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

HWTEST_F(AubMemDumpTests, GivenWriteVerifySevenBytesGgttThenExpectationsAreMet) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;
    std::string filePath(folderAUB);
    std::string filenameWithPrefix = ApiSpecificConfig::getAubPrefixForSpecificApi();
    filePath.append(Os::fileSeparator);
    filePath.append(getAubFileName(pDevice, filenameWithPrefix.append("writeVerifySevenBytesGGTT.aub")));
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

HWTEST_F(AubMemDumpTests, GivenRcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_RCS);
}

HWTEST_F(AubMemDumpTests, GivenBcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_BCS);
}

HWTEST_F(AubMemDumpTests, GivenVcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_VCS);
}

HWTEST_F(AubMemDumpTests, GivenVecsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_VECS);
}