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

#include "aub_mem_dump_tests.h"
#include "unit_tests/fixtures/device_fixture.h"

using OCLRT::AUBCommandStreamReceiver;
using OCLRT::AUBCommandStreamReceiverHw;
using OCLRT::AUBFamilyMapper;
using OCLRT::EngineType;
using OCLRT::folderAUB;

std::string getAubFileName(const OCLRT::Device *pDevice, const std::string baseName) {
    const auto pGtSystemInfo = pDevice->getHardwareInfo().pSysInfo;
    std::stringstream strfilename;
    strfilename << pDevice->getProductAbbrev() << "_" << pGtSystemInfo->SliceCount << "x" << pGtSystemInfo->SubSliceCount << "x" << pGtSystemInfo->MaxEuPerSubSlice << "_" << baseName;

    return strfilename.str();
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
    AUB::reserveAddressPPGTT(aubFile, gAddress, 4096, pAddress, 7);

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
    AUB::reserveAddressPPGTT(aubFile, gAddress, sizeof(byte), physAddress, 7);
    AUB::addMemoryWrite(aubFile, physAddress, &byte, sizeof(byte), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, &byte, sizeof(byte));

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
    aubFile.expectMemory(physAddress, &byte, sizeof(byte));

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
    AUB::reserveAddressPPGTT(aubFile, gAddress, sizeof(bytes), physAddress, 7);
    AUB::addMemoryWrite(aubFile, physAddress, bytes, sizeof(bytes), AubMemDump::AddressSpaceValues::TraceNonlocal);
    aubFile.expectMemory(physAddress, bytes, sizeof(bytes));

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
    aubFile.expectMemory(physAddress, bytes, sizeof(bytes));

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
