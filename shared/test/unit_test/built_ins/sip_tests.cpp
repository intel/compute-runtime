/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/test.h"

#include "common/StateSaveAreaHeader.h"

#include <set>

using namespace NEO;
namespace NEO {
extern std::set<std::string> virtualFileList;
}

struct RawBinarySipFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.LoadBinarySipFromFile.set("dummy_file.bin");

        backupSipInitType = std::make_unique<VariableBackup<bool>>(&MockSipData::useMockSip, false);
        backupSipClassType = std::make_unique<VariableBackup<SipClassType>>(&SipKernel::classType);

        backupFopenReturned = std::make_unique<VariableBackup<FILE *>>(&IoFunctions::mockFopenReturned);
        backupFtellReturned = std::make_unique<VariableBackup<long int>>(&IoFunctions::mockFtellReturn, 128);
        backupFreadReturned = std::make_unique<VariableBackup<size_t>>(&IoFunctions::mockFreadReturn, 128u);

        backupFopenCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFopenCalled, 0u);
        backupFseekCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFseekCalled, 0u);
        backupFtellCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFtellCalled, 0u);
        backupRewindCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockRewindCalled, 0u);
        backupFreadCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFreadCalled, 0u);
        backupFcloseCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFcloseCalled, 0u);
        backupFailAfterNFopenCount = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::failAfterNFopenCount, 0u);

        DeviceFixture::setUp();
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    DebugManagerStateRestore dbgRestorer;

    std::unique_ptr<VariableBackup<bool>> backupSipInitType;
    std::unique_ptr<VariableBackup<SipClassType>> backupSipClassType;

    std::unique_ptr<VariableBackup<FILE *>> backupFopenReturned;
    std::unique_ptr<VariableBackup<long int>> backupFtellReturned;
    std::unique_ptr<VariableBackup<size_t>> backupFreadReturned;

    std::unique_ptr<VariableBackup<uint32_t>> backupFopenCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFseekCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFtellCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupRewindCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFreadCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFcloseCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFailAfterNFopenCount;
};

TEST(SipBinaryFromFile, givenFilenameWhenCreatingHeaderFilenameThenSuffixIsAddedBeforeExtension) {
    std::string fileName = "abc.bin";
    auto headerName = MockSipKernel::createHeaderFilename(fileName);
    EXPECT_EQ("abc_header.bin", headerName);
}

TEST(SipBinaryFromFile, givenFilenameWithoutExtnesionWhenCreatingHeaderFilenameThenSuffixIsAdded) {
    std::string fileName = "abc";
    auto headerName = MockSipKernel::createHeaderFilename(fileName);
    EXPECT_EQ("abc_header", headerName);
}

using RawBinarySipTest = Test<RawBinarySipFixture>;

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenInitSipKernelThenSipIsLoadedFromFile) {
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(2u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(2u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(2u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(2u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(2u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto sipAllocation = SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    auto header = SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaHeader();
    EXPECT_NE(0u, header.size());
}

TEST_F(RawBinarySipTest, givenFileHeaderMissingWhenInitSipKernelThenSipIsLoadedFromFileWithoutHeader) {
    IoFunctions::failAfterNFopenCount = 1;
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(2u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto sipAllocation = SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    auto header = SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaHeader();
    EXPECT_EQ(0u, header.size());
}

TEST_F(RawBinarySipTest, givenDebuggerAndRawBinaryFileWhenInitSipKernelThenDbgSipIsLoadedFromFile) {
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    auto currentSipKernelType = SipKernel::getSipKernelType(*pDevice);
    bool ret = SipKernel::initSipKernel(currentSipKernelType, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(2u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(2u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(2u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(2u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(2u, IoFunctions::mockFcloseCalled);

    EXPECT_LE(SipKernelType::dbgCsr, currentSipKernelType);

    uint32_t sipIndex = static_cast<uint32_t>(currentSipKernelType);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto sipAllocation = SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenOpenFileFailsThenSipIsNotLoadedFromFile) {
    IoFunctions::mockFopenReturned = nullptr;
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(0u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(0u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(0u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(0u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(0u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenTellSizeDiffrentThanBytesReadThenSipIsNotCreated) {
    IoFunctions::mockFtellReturn = 28;
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenBytesReadIsZeroThenSipIsNotCreated) {
    IoFunctions::mockFreadReturn = 0u;
    IoFunctions::mockFtellReturn = 0;
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenAllocationCreationFailsThenSipIsNotCreated) {
    pDevice->executionEnvironment->memoryManager.reset(new FailMemoryManager(0, *pDevice->executionEnvironment));
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenInitSipKernelTwiceThenSipIsLoadedFromFileAndCreatedOnce) {
    bool ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(2u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(2u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(2u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(2u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(2u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto &refSipKernel = SipKernel::getSipKernel(*pDevice, nullptr);
    EXPECT_EQ(sipKernel, &refSipKernel);

    auto sipAllocation = refSipKernel.getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    ret = SipKernel::initSipKernel(SipKernelType::csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(2u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(2u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(2u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(2u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(2u, IoFunctions::mockFcloseCalled);

    auto secondSipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, secondSipKernel);
    auto secondStoredAllocation = sipKernel->getSipAllocation();
    EXPECT_NE(nullptr, secondStoredAllocation);
    EXPECT_EQ(sipKernel, secondSipKernel);
    EXPECT_EQ(storedAllocation, secondStoredAllocation);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenGettingBindlessDebugSipThenSipIsLoadedFromFile) {
    auto sipAllocation = SipKernel::getBindlessDebugSipKernel(*pDevice).getSipAllocation();

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::dbgBindless);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    auto header = SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaHeader();
    EXPECT_NE(0u, header.size());
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenGettingBindlessDebugSipWithContextThenSipIsLoadedFromFile) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           pDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto sipAllocation = NEO::SipKernel::getBindlessDebugSipKernel(*pDevice, osContext.get()).getSipAllocation();

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::dbgBindless);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    auto header = SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaHeader();
    EXPECT_NE(0u, header.size());
}

struct HexadecimalHeaderSipKernel : public SipKernel {
    using SipKernel::getSipKernelImpl;
    using SipKernel::initHexadecimalArraySipKernel;
};
using HexadecimalHeaderSipTest = Test<DeviceFixture>;

TEST_F(HexadecimalHeaderSipTest, whenInitHexadecimalArraySipKernelIsCalledThenSipKernelIsCorrect) {
    VariableBackup<SipClassType> backupSipClassType(&SipKernel::classType, SipClassType::hexadecimalHeaderFile);

    EXPECT_TRUE(HexadecimalHeaderSipKernel::initHexadecimalArraySipKernel(SipKernelType::csr, *pDevice));
    EXPECT_EQ(SipKernelType::csr, SipKernel::getSipKernelType(*pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    const auto expectedSipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, expectedSipKernel);

    const auto &sipKernel = HexadecimalHeaderSipKernel::getSipKernelImpl(*pDevice);
    EXPECT_EQ(expectedSipKernel, &sipKernel);

    auto expectedSipAllocation = expectedSipKernel->getSipAllocation();
    auto sipAllocation = sipKernel.getSipAllocation();
    EXPECT_EQ(expectedSipAllocation, sipAllocation);
}

TEST_F(HexadecimalHeaderSipTest, givenFailMemoryManagerWhenInitHexadecimalArraySipKernelIsCalledThenSipKernelIsNullptr) {
    pDevice->executionEnvironment->memoryManager.reset(new FailMemoryManager(0, *pDevice->executionEnvironment));
    EXPECT_FALSE(HexadecimalHeaderSipKernel::initHexadecimalArraySipKernel(SipKernelType::csr, *pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(HexadecimalHeaderSipTest, whenInitHexadecimalArraySipKernelIsCalledTwiceThenSipKernelIsCreatedOnce) {
    VariableBackup<SipClassType> backupSipClassType(&SipKernel::classType, SipClassType::hexadecimalHeaderFile);
    EXPECT_TRUE(HexadecimalHeaderSipKernel::initHexadecimalArraySipKernel(SipKernelType::csr, *pDevice));

    const auto &sipKernel = HexadecimalHeaderSipKernel::getSipKernelImpl(*pDevice);
    EXPECT_TRUE(HexadecimalHeaderSipKernel::initHexadecimalArraySipKernel(SipKernelType::csr, *pDevice));

    const auto &sipKernel2 = HexadecimalHeaderSipKernel::getSipKernelImpl(*pDevice);
    EXPECT_EQ(&sipKernel, &sipKernel2);

    auto sipAllocation = sipKernel.getSipAllocation();
    auto sipAllocation2 = sipKernel2.getSipAllocation();
    EXPECT_EQ(sipAllocation, sipAllocation2);
}

using StateSaveAreaSipTest = Test<RawBinarySipFixture>;

TEST_F(StateSaveAreaSipTest, givenEmptyStateSaveAreaHeaderWhenGetStateSaveAreaSizeCalledThenMaxDbgSurfaceSizeIsReturned) {

    MockSipData::useMockSip = true;
    MockSipData::mockSipKernel->mockStateSaveAreaHeader.clear();
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &gfxCoreHelper = this->pDevice->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.getSipKernelMaxDbgSurfaceSize(hwInfo), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST_F(StateSaveAreaSipTest, givenCorruptedStateSaveAreaHeaderWhenGetStateSaveAreaSizeCalledThenMaxDbgSurfaceSizeIsReturned) {
    MockSipData::useMockSip = true;
    MockSipData::mockSipKernel->mockStateSaveAreaHeader = {'g', 'a', 'r', 'b', 'a', 'g', 'e'};
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &gfxCoreHelper = this->pDevice->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.getSipKernelMaxDbgSurfaceSize(hwInfo), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST_F(StateSaveAreaSipTest, givenCorrectStateSaveAreaHeaderWhenGetStateSaveAreaSizeCalledThenCorrectDbgSurfaceSizeIsReturned) {
    MockSipData::useMockSip = true;
    auto hwInfo = pDevice->getHardwareInfo();
    auto numSlices = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);
    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    EXPECT_EQ(0x1800u * numSlices * 6 * 16 * 7 + alignUp(sizeof(SIP::StateSaveAreaHeader), MemoryConstants::pageSize), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));

    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(0x1800u * numSlices * 8 * 7 + alignUp(sizeof(SIP::StateSaveAreaHeader), MemoryConstants::pageSize), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST(DebugBindlessSip, givenDebuggerAndUseBindlessDebugSipWhenGettingSipTypeThenDebugBindlessTypeIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseBindlessDebugSip.set(1);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(mockDevice.get());

    auto sipType = NEO::SipKernel::getSipKernelType(*mockDevice);

    EXPECT_EQ(SipKernelType::dbgBindless, sipType);
}

TEST(Sip, WhenGettingTypeThenCorrectTypeIsReturned) {
    std::vector<char> ssaHeader;
    SipKernel csr{SipKernelType::csr, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::csr, csr.getType());

    SipKernel dbgCsr{SipKernelType::dbgCsr, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::dbgCsr, dbgCsr.getType());

    SipKernel dbgCsrLocal{SipKernelType::dbgCsrLocal, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::dbgCsrLocal, dbgCsrLocal.getType());

    SipKernel undefined{SipKernelType::count, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::count, undefined.getType());
}

TEST(Sip, givenDebuggingInactiveWhenSipTypeIsQueriedThenCsrSipTypeIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto sipType = SipKernel::getSipKernelType(*mockDevice);
    EXPECT_EQ(SipKernelType::csr, sipType);
}

TEST(DebugSip, givenDebuggingActiveWhenSipTypeIsQueriedThenDbgCsrSipTypeIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice.get());

    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(mockDevice.get());

    auto sipType = SipKernel::getSipKernelType(*mockDevice);
    EXPECT_LE(SipKernelType::dbgCsr, sipType);
}

TEST(DebugSip, givenBuiltInsWhenDbgCsrSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto &builtins = *mockDevice->getBuiltIns();
    auto &sipKernel = builtins.getSipKernel(SipKernelType::dbgCsr, *mockDevice);

    EXPECT_NE(nullptr, &sipKernel);
    EXPECT_EQ(SipKernelType::dbgCsr, sipKernel.getType());
}

TEST(DebugBindlessSip, givenBindlessDebugSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice);

    EXPECT_NE(nullptr, &sipKernel);
    EXPECT_EQ(SipKernelType::dbgBindless, sipKernel.getType());

    EXPECT_FALSE(sipKernel.getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenContextWhenBindlessDebugSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice, &csr->getOsContext());
    EXPECT_NE(nullptr, &sipKernel);

    auto contextSip = builtIns->perContextSipKernels[contextId].first.get();

    EXPECT_NE(nullptr, contextSip);
    EXPECT_EQ(SipKernelType::dbgBindless, contextSip->getType());
    EXPECT_NE(nullptr, contextSip->getSipAllocation());
    EXPECT_FALSE(contextSip->getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenOfflineDebuggingModeWhenGettingSipForContextThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getSipKernel(*mockDevice, &csr->getOsContext());
    EXPECT_NE(nullptr, &sipKernel);

    auto contextSip = builtIns->perContextSipKernels[contextId].first.get();

    EXPECT_NE(nullptr, contextSip);
    EXPECT_EQ(SipKernelType::dbgBindless, contextSip->getType());
    EXPECT_NE(nullptr, contextSip->getSipAllocation());
    EXPECT_FALSE(contextSip->getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenTwoContextsWhenBindlessDebugSipIsRequestedThenEachSipKernelIsAssignedToADifferentContextId) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);

    const uint32_t context0Id = 0u;
    std::unique_ptr<OsContext> osContext0(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                            mockDevice->getRootDeviceIndex(), context0Id,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext0->setDefaultContext(true);

    const uint32_t context1Id = 1u;
    std::unique_ptr<OsContext> osContext1(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                            mockDevice->getRootDeviceIndex(), context1Id,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext1->setDefaultContext(true);

    auto &sipKernel1 = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice, osContext1.get());
    auto &sipKernel0 = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice, osContext0.get());
    EXPECT_NE(sipKernel0.getSipAllocation(), sipKernel1.getSipAllocation());

    auto context0SipKernel = builtIns->perContextSipKernels[context0Id].first.get();
    auto context1SipKernel = builtIns->perContextSipKernels[context1Id].first.get();
    EXPECT_NE(context0SipKernel, context1SipKernel);

    const auto alloc0Id = static_cast<MemoryAllocation *>(context0SipKernel->getSipAllocation())->id;
    const auto alloc1Id = static_cast<MemoryAllocation *>(context1SipKernel->getSipAllocation())->id;
    EXPECT_NE(alloc0Id, alloc1Id);
}

TEST(DebugBindlessSip, givenFailingSipAllocationWhenBindlessDebugSipWithContextIsRequestedThenSipAllocationInSipKernelIsNull) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto executionEnvironment = mockDevice->getExecutionEnvironment();

    auto mockMemoryManager = new MockMemoryManager();
    mockMemoryManager->isMockHostMemoryManager = true;
    mockMemoryManager->forceFailureInPrimaryAllocation = true;
    executionEnvironment->memoryManager.reset(mockMemoryManager);

    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice, &csr->getOsContext());
    EXPECT_NE(nullptr, &sipKernel);

    auto contextSip = builtIns->perContextSipKernels[contextId].first.get();

    EXPECT_NE(nullptr, contextSip);
    EXPECT_EQ(SipKernelType::dbgBindless, contextSip->getType());
    EXPECT_EQ(nullptr, contextSip->getSipAllocation());
    EXPECT_FALSE(contextSip->getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenCorrectSipKernelWhenReleasingAllocationManuallyThenFreeGraphicsMemoryIsSkippedOnDestruction) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    [[maybe_unused]] auto &sipKernel = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice, &csr->getOsContext());

    mockDevice->getMemoryManager()->freeGraphicsMemory(builtIns->perContextSipKernels[contextId].first.get()->getSipAllocation());
    builtIns->perContextSipKernels[contextId].first.reset(nullptr);
}

TEST(DebugBindlessSip, givenOfflineDebuggingModeWhenSipIsInitializedThenBinaryIsParsed) {
    MockCompilerDebugVars igcDebugVars;
    uint32_t binary[20] = {0};
    binary[2] = 0xcafebead;
    binary[6] = 0xcafebead;
    igcDebugVars.binaryToReturnSize = sizeof(binary);
    igcDebugVars.binaryToReturn = binary;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);

    auto osContext = std::make_unique<OsContextMock>(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext->debuggableContext = true;

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    mockDevice->getBuiltIns()->getSipKernel(SipKernelType::dbgBindless, *mockDevice);

    auto sipKernel = builtIns->sipKernels[static_cast<uint32_t>(SipKernelType::dbgBindless)].first.get();

    EXPECT_NE(nullptr, sipKernel);

    EXPECT_EQ(2u, sipKernel->getCtxOffset());
    EXPECT_EQ(6u, sipKernel->getPidOffset());
    EXPECT_EQ(sizeof(binary), sipKernel->getBinary().size());

    gEnvironment->igcPopDebugVars();
}

TEST(DebugBindlessSip, givenOfflineDebuggingModeAndInvalidSipWhenSipIsInitializedThenContextIdOffsetsAreZero) {
    MockCompilerDebugVars igcDebugVars;
    uint32_t binary[20] = {0};
    binary[19] = 0xcafebead;
    igcDebugVars.binaryToReturnSize = sizeof(binary);
    igcDebugVars.binaryToReturn = binary;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);

    auto osContext = std::make_unique<OsContextMock>(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext->debuggableContext = true;

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    mockDevice->getBuiltIns()->getSipKernel(SipKernelType::dbgBindless, *mockDevice);

    auto sipKernel = builtIns->sipKernels[static_cast<uint32_t>(SipKernelType::dbgBindless)].first.get();

    EXPECT_NE(nullptr, sipKernel);

    EXPECT_EQ(0u, sipKernel->getCtxOffset());
    EXPECT_EQ(0u, sipKernel->getPidOffset());
    EXPECT_EQ(sizeof(binary), sipKernel->getBinary().size());

    gEnvironment->igcPopDebugVars();
}

TEST(DebugBindlessSip, givenOfflineDebuggingModeWhenDebugSipForContextIsCreatedThenContextIdIsPatched) {
    MockCompilerDebugVars igcDebugVars;
    uint32_t binary[20] = {0};
    binary[2] = 0xcafebead;
    binary[6] = 0xcafebead;
    igcDebugVars.binaryToReturnSize = sizeof(binary);
    igcDebugVars.binaryToReturn = binary;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto executionEnvironment = mockDevice->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);

    auto osContext = std::make_unique<OsContextMock>(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext->debuggableContext = true;
    osContext->offlineDumpCtxId = uint64_t(0xAA) << 32 | 0xBB;

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice, &csr->getOsContext());

    EXPECT_NE(nullptr, &sipKernel);

    EXPECT_EQ(0u, sipKernel.getCtxOffset());
    EXPECT_EQ(0u, sipKernel.getPidOffset());
    EXPECT_EQ(0u, sipKernel.getBinary().size());

    uint32_t *patchedBinary = reinterpret_cast<uint32_t *>(sipKernel.getSipAllocation()->getUnderlyingBuffer());
    uint32_t low = static_cast<uint32_t>(osContext->getOfflineDumpContextId(0) & 0xFFFFFFFFu);
    uint32_t high = static_cast<uint32_t>((osContext->getOfflineDumpContextId(0) >> 32) & 0xFFFFFFFFu);
    EXPECT_EQ(low, patchedBinary[2]);
    EXPECT_EQ(high, patchedBinary[6]);

    gEnvironment->igcPopDebugVars();
}
class SipKernelMock : public SipKernel {
  public:
    using SipKernel::selectSipClassType;
};

using DebugBuiltinSipTest = Test<DeviceFixture>;

TEST_F(DebugBuiltinSipTest, givenDebuggerWhenInitSipKernelThenDbgSipIsLoadedFromBuiltin) {
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    auto sipKernelType = SipKernel::getSipKernelType(*pDevice);
    EXPECT_TRUE(SipKernel::initSipKernel(sipKernelType, *pDevice));
    EXPECT_LE(SipKernelType::dbgCsr, sipKernelType);

    auto sipAllocation = SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();
    EXPECT_NE(nullptr, sipAllocation);
    EXPECT_EQ(SipKernelMock::classType, SipClassType::builtins);
}
