/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debugger/debugger_l0.h"
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
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "common/StateSaveAreaHeader.h"

#include <map>

using namespace NEO;
namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
}

struct RawBinarySipFixture : public DeviceWithoutSipFixture {
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

        DeviceWithoutSipFixture::setUp();
    }

    void tearDown() {
        DeviceWithoutSipFixture::tearDown();
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
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

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenGettingHeaplessDebugSipThenSipIsLoadedFromFile) {

    auto compilerHelper = std::unique_ptr<MockCompilerProductHelper>(new MockCompilerProductHelper());
    auto releaseHelper = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    compilerHelper->isHeaplessModeEnabledResult = true;
    pDevice->getRootDeviceEnvironmentRef().compilerProductHelper.reset(compilerHelper.release());
    pDevice->getRootDeviceEnvironmentRef().releaseHelper.reset(releaseHelper.release());
    auto sipAllocation = SipKernel::getDebugSipKernel(*pDevice).getSipAllocation();
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::dbgHeapless);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    auto header = SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaHeader();
    EXPECT_NE(0u, header.size());
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenGettingBindlessDebugSipThenSipIsLoadedFromFile) {

    auto compilerHelper = std::unique_ptr<MockCompilerProductHelper>(new MockCompilerProductHelper());
    auto releaseHelper = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    compilerHelper->isHeaplessModeEnabledResult = false;
    pDevice->getRootDeviceEnvironmentRef().compilerProductHelper.reset(compilerHelper.release());
    pDevice->getRootDeviceEnvironmentRef().releaseHelper.reset(releaseHelper.release());
    auto sipAllocation = SipKernel::getDebugSipKernel(*pDevice).getSipAllocation();
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::dbgBindless);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    auto header = SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaHeader();
    EXPECT_NE(0u, header.size());
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenGettingDebugSipWithContextThenSipIsLoadedFromFile) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           pDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto sipAllocation = NEO::SipKernel::getDebugSipKernel(*pDevice, osContext.get()).getSipAllocation();

    uint32_t sipIndex;
    auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        sipIndex = static_cast<uint32_t>(SipKernelType::dbgHeapless);
    } else {
        sipIndex = static_cast<uint32_t>(SipKernelType::dbgBindless);
    }
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

using HexadecimalHeaderSipTest = Test<DeviceWithoutSipFixture>;

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

struct StateSaveAreaSipTest : Test<RawBinarySipFixture> {
    void SetUp() override {
        RawBinarySipFixture::setUp();
        MockSipData::useMockSip = true;
        stateSaveAreaHeaderBackup = MockSipData::mockSipKernel->mockStateSaveAreaHeader;
    }
    void TearDown() override {
        MockSipData::mockSipKernel->mockStateSaveAreaHeader = std::move(stateSaveAreaHeaderBackup);
        RawBinarySipFixture::tearDown();
    }
    VariableBackup<bool> backupSipInitType{&MockSipData::useMockSip};
    std::vector<char> stateSaveAreaHeaderBackup;
};

TEST_F(StateSaveAreaSipTest, givenEmptyStateSaveAreaHeaderWhenGetStateSaveAreaSizeCalledThenZeroSizeIsReturned) {
    MockSipData::mockSipKernel->mockStateSaveAreaHeader.clear();
    EXPECT_EQ(0u, SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST_F(StateSaveAreaSipTest, givenCorruptedStateSaveAreaHeaderWhenGetStateSaveAreaSizeCalledThenZeroSizeIsReturned) {
    MockSipData::mockSipKernel->mockStateSaveAreaHeader = {'g', 'a', 'r', 'b', 'a', 'g', 'e'};
    EXPECT_EQ(0u, SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST_F(StateSaveAreaSipTest, givenCorrectStateSaveAreaHeaderWhenGetStateSaveAreaSizeCalledThenCorrectDbgSurfaceSizeIsReturned) {
    auto hwInfo = pDevice->getHardwareInfo();
    auto numSlices = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);
    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    EXPECT_EQ(0x1800u * numSlices * 2 * 16 * 7 + alignUp(sizeof(SIP::StateSaveAreaHeader), MemoryConstants::pageSize), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));

    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
    EXPECT_EQ(0x1800u * numSlices * 8 * 7 + alignUp(sizeof(SIP::StateSaveAreaHeader), MemoryConstants::pageSize), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));

    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(3);
    auto fifoSize = 100 * sizeof(SIP::fifo_node);
    auto stateSaveSize = 0x1800u * numSlices * 8 * 7 + sizeof(NEO::StateSaveAreaHeader);
    EXPECT_EQ(alignUp(fifoSize + stateSaveSize, MemoryConstants::pageSize), SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST_F(StateSaveAreaSipTest, givenStateSaveAreaHeaderVersion4WhenGetStateSaveAreaSizeCalledThenTotalWmtpDataSizeIsReturned) {
    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(4);
    auto header = reinterpret_cast<SIP::StateSaveAreaHeader *>(MockSipData::mockSipKernel->mockStateSaveAreaHeader.data());
    header->versionHeader.version.major = 4u;
    EXPECT_EQ(MockSipData::totalWmtpDataSize, SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST_F(StateSaveAreaSipTest, givenStateSaveAreaHeaderVersion4WhenGetSipKernelIsCalledForCsrSipThenPreemptionSurfaceSizeIsUpdated) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(4);
    MockCompilerDebugVars debugVars = {};
    debugVars.stateSaveAreaHeaderToReturn = stateSaveAreaHeader.data();
    debugVars.stateSaveAreaHeaderToReturnSize = stateSaveAreaHeader.size();
    gEnvironment->igcPushDebugVars(debugVars);
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) -> void { gEnvironment->igcPopDebugVars(); }};

    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.requiredPreemptionSurfaceSize = static_cast<size_t>(MockSipData::totalWmtpDataSize * 4);
    EXPECT_NE(hwInfo->capabilityTable.requiredPreemptionSurfaceSize, MockSipData::totalWmtpDataSize);

    auto builtins = pDevice->getBuiltIns();
    EXPECT_EQ(MockSipData::totalWmtpDataSize, builtins->getSipKernel(SipKernelType::csr, *pDevice).getStateSaveAreaSize(pDevice));
    EXPECT_EQ(hwInfo->capabilityTable.requiredPreemptionSurfaceSize, MockSipData::totalWmtpDataSize);
}

TEST_F(StateSaveAreaSipTest, givenNotsupportedStateSaveAreaHeaderVersionWhenGetStateSaveAreaSizeCalledThenNoSizeIsReturned) {
    VariableBackup<bool> backupSipInitType(&MockSipData::useMockSip, true);
    MockSipData::mockSipKernel->mockStateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(1);
    auto header = reinterpret_cast<SIP::StateSaveAreaHeader *>(MockSipData::mockSipKernel->mockStateSaveAreaHeader.data());
    header->versionHeader.version.major = 5u;
    EXPECT_EQ(0u, SipKernel::getSipKernel(*pDevice, nullptr).getStateSaveAreaSize(pDevice));
}

TEST(DebugBindlessSip, givenDebuggerAndUseBindlessDebugSipWhenGettingSipTypeThenDebugBindlessTypeIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseBindlessDebugSip.set(1);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(mockDevice.get());

    auto sipType = NEO::SipKernel::getSipKernelType(*mockDevice);

    auto &compilerProductHelper = mockDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (heaplessEnabled) {
        EXPECT_EQ(SipKernelType::dbgHeapless, sipType);

    } else {
        EXPECT_EQ(SipKernelType::dbgBindless, sipType);
    }
}

TEST(DebugHeaplessSip, givenDebuggerAndUseHeaplessModeWhenGettingSipTypeThenDebugHeaplessTypeIsReturned) {

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(mockDevice.get());
    auto compilerHelper = std::unique_ptr<MockCompilerProductHelper>(new MockCompilerProductHelper());
    auto releaseHelper = std::unique_ptr<MockReleaseHelper>(new MockReleaseHelper());
    compilerHelper->isHeaplessModeEnabledResult = true;
    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->compilerProductHelper.reset(compilerHelper.release());
    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->releaseHelper.reset(releaseHelper.release());

    auto sipType = NEO::SipKernel::getSipKernelType(*mockDevice);

    EXPECT_EQ(SipKernelType::dbgHeapless, sipType);
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

TEST(DebugSip, givenDebugSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getDebugSipKernel(*mockDevice);

    EXPECT_NE(nullptr, &sipKernel);
    auto &compilerProductHelper = mockDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        EXPECT_EQ(SipKernelType::dbgHeapless, sipKernel.getType());
    } else {
        EXPECT_EQ(SipKernelType::dbgBindless, sipKernel.getType());
    }

    EXPECT_FALSE(sipKernel.getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenContextWhenBindlessDebugSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getDebugSipKernel(*mockDevice, &csr->getOsContext());
    EXPECT_NE(nullptr, &sipKernel);

    auto contextSip = builtIns->perContextSipKernels[contextId].first.get();

    EXPECT_NE(nullptr, contextSip);
    EXPECT_EQ(SipKernelType::dbgBindless, contextSip->getType());
    EXPECT_NE(nullptr, contextSip->getSipAllocation());
    EXPECT_FALSE(contextSip->getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenOfflineDebuggingModeWhenGettingSipForContextThenCorrectSipKernelIsReturned) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    auto &sipKernel = NEO::SipKernel::getSipKernel(*mockDevice, &csr->getOsContext());
    EXPECT_NE(nullptr, &sipKernel);

    auto contextSip = builtIns->perContextSipKernels[contextId].first.get();

    EXPECT_NE(nullptr, contextSip);
    EXPECT_EQ(SipKernelType::dbgBindless, contextSip->getType());
    EXPECT_NE(nullptr, contextSip->getSipAllocation());
    EXPECT_FALSE(contextSip->getStateSaveAreaHeader().empty());
}

TEST(DebugSip, givenOfflineDebuggingModeWhenGettingSipForContextThenMemoryTransferGoesThroughCpuPointer) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    auto memoryManager = static_cast<MockMemoryManager *>(mockDevice->getMemoryManager());

    const uint32_t contextId = 2u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    std::vector<char> sipBinary;
    std::vector<char> stateSaveAreaHeader;
    mockDevice->getCompilerInterface()->getSipKernelBinary(*mockDevice, SipKernelType::dbgBindless, sipBinary, stateSaveAreaHeader);

    memoryManager->copyMemoryToAllocationBanksCalled = 0;
    auto &sipKernel = NEO::SipKernel::getSipKernel(*mockDevice, &csr->getOsContext());
    ASSERT_NE(nullptr, &sipKernel);
    EXPECT_EQ(1u, memoryManager->copyMemoryToAllocationBanksCalled);

    EXPECT_EQ(0, memcmp(sipBinary.data(), sipKernel.getSipAllocation()->getUnderlyingBuffer(), sipBinary.size()));
}

TEST(DebugBindlessSip, givenTwoContextsWhenBindlessDebugSipIsRequestedThenEachSipKernelIsAssignedToADifferentContextId) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

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

    auto &sipKernel1 = NEO::SipKernel::getDebugSipKernel(*mockDevice, osContext1.get());
    auto &sipKernel0 = NEO::SipKernel::getDebugSipKernel(*mockDevice, osContext0.get());
    EXPECT_NE(sipKernel0.getSipAllocation(), sipKernel1.getSipAllocation());

    auto context0SipKernel = builtIns->perContextSipKernels[context0Id].first.get();
    auto context1SipKernel = builtIns->perContextSipKernels[context1Id].first.get();
    EXPECT_NE(context0SipKernel, context1SipKernel);

    const auto alloc0Id = static_cast<MemoryAllocation *>(context0SipKernel->getSipAllocation())->id;
    const auto alloc1Id = static_cast<MemoryAllocation *>(context1SipKernel->getSipAllocation())->id;
    EXPECT_NE(alloc0Id, alloc1Id);
}

TEST(DebugBindlessSip, givenFailingSipAllocationWhenBindlessDebugSipWithContextIsRequestedThenSipAllocationInSipKernelIsNull) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);

    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    auto mockMemoryManager = new MockMemoryManager();
    mockMemoryManager->isMockHostMemoryManager = true;
    mockMemoryManager->forceFailureInPrimaryAllocation = true;
    executionEnvironment->memoryManager.reset(mockMemoryManager);

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getDebugSipKernel(*mockDevice, &csr->getOsContext());
    EXPECT_NE(nullptr, &sipKernel);

    auto contextSip = builtIns->perContextSipKernels[contextId].first.get();

    EXPECT_NE(nullptr, contextSip);
    EXPECT_EQ(SipKernelType::dbgBindless, contextSip->getType());
    EXPECT_EQ(nullptr, contextSip->getSipAllocation());
    EXPECT_FALSE(contextSip->getStateSaveAreaHeader().empty());
}

TEST(DebugBindlessSip, givenCorrectSipKernelWhenReleasingAllocationManuallyThenFreeGraphicsMemoryIsSkippedOnDestruction) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           mockDevice->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    [[maybe_unused]] auto &sipKernel = NEO::SipKernel::getDebugSipKernel(*mockDevice, &csr->getOsContext());

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

    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

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

    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

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

    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    auto osContext = std::make_unique<OsContextMock>(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    osContext->debuggableContext = true;
    osContext->offlineDumpCtxId = uint64_t(0xAA) << 32 | 0xBB;
    auto csr = mockDevice->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    EXPECT_NE(nullptr, mockDevice);

    // ensure once_flag has not already been satisfied
    auto contextId = osContext->getContextId();
    auto it = builtIns->perContextSipKernels.find(contextId);
    if (it != builtIns->perContextSipKernels.end()) {
        mockDevice->getMemoryManager()->freeGraphicsMemory(it->second.first->getSipAllocation());

        builtIns->perContextSipKernels.erase(contextId);
    }
    builtIns->perContextSipKernels.erase(contextId);

    auto &sipKernel = NEO::SipKernel::getDebugSipKernel(*mockDevice, &csr->getOsContext());

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

using DebugBuiltinSipTest = Test<DeviceFixture>;

TEST_F(DebugBuiltinSipTest, givenDebuggerWhenInitSipKernelThenDbgSipIsLoadedFromBuiltin) {
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    auto sipKernelType = SipKernel::getSipKernelType(*pDevice);
    EXPECT_TRUE(SipKernel::initSipKernel(sipKernelType, *pDevice));
    EXPECT_LE(SipKernelType::dbgCsr, sipKernelType);

    auto sipAllocation = SipKernel::getSipKernel(*pDevice, nullptr).getSipAllocation();
    EXPECT_NE(nullptr, sipAllocation);
    EXPECT_EQ(MockSipKernel::classType, SipClassType::builtins);

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
}

TEST_F(DebugBuiltinSipTest, givenDebugFlagForForceSipClassWhenInitSipKernelThenProperSipClassIsSet) {
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    VariableBackup sipClassBackup(&MockSipKernel::classType);
    DebugManagerStateRestore restorer;

    debugManager.flags.ForceSipClass.set(static_cast<int32_t>(SipClassType::builtins));
    EXPECT_TRUE(SipKernel::initSipKernel(SipKernelType::csr, *pDevice));
    EXPECT_EQ(MockSipKernel::classType, SipClassType::builtins);

    debugManager.flags.ForceSipClass.set(static_cast<int32_t>(SipClassType::hexadecimalHeaderFile));
    EXPECT_TRUE(SipKernel::initSipKernel(SipKernelType::csr, *pDevice));
    EXPECT_EQ(MockSipKernel::classType, SipClassType::hexadecimalHeaderFile);

    SipKernel::freeSipKernels(&pDevice->getRootDeviceEnvironmentRef(), pDevice->getMemoryManager());
}

TEST_F(DebugBuiltinSipTest, givenDumpSipHeaderFileWhenGettingSipKernelThenSipHeaderFileIsCreated) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DumpSipHeaderFile.set("sip");

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);

    auto &builtins = *pDevice->getBuiltIns();
    builtins.getSipKernel(SipKernelType::dbgCsr, *pDevice);

    EXPECT_EQ(1u, NEO::virtualFileList.size());
    EXPECT_TRUE(NEO::virtualFileList.find("sip_header.bin") != NEO::virtualFileList.end());
}

TEST(SipTest, whenForcingBuiltinSipClassThenPreemptionSurfaceSizeIsSetBasedOnStateSaveAreaHeader) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceSipClass.set(static_cast<int32_t>(SipClassType::builtins));
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    constexpr uint32_t initialPreemptionSurfaceSize = 0xdeadbeef;
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.requiredPreemptionSurfaceSize = initialPreemptionSurfaceSize;
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(&rootDeviceEnvironment, builtIns);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));

    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = rootDeviceEnvironment.builtins->getSipKernel(NEO::SipKernelType::csr, *mockDevice);

    auto preemptionSurfaceSize = rootDeviceEnvironment.getHardwareInfo()->capabilityTable.requiredPreemptionSurfaceSize;
    EXPECT_NE(initialPreemptionSurfaceSize, preemptionSurfaceSize);
    EXPECT_EQ(sipKernel.getStateSaveAreaSize(mockDevice.get()), preemptionSurfaceSize);
}

using DebugExternalLibSipTest = Test<DeviceFixture>;

HWTEST2_F(DebugExternalLibSipTest, givenDebuggerWhenInitSipKernelThenDbgSipIsLoadedFromBuiltIns, IsAtMostXe3Core) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GetSipBinaryFromExternalLib.set(1);
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(pDevice);
    std::string fileName = "unk";
    MockSipKernel::selectSipClassType(fileName, *pDevice);
    EXPECT_EQ(MockSipKernel::classType, SipClassType::builtins);
}

TEST_F(DebugExternalLibSipTest, givenGetSipBinaryFromExternalLibRetunsTrueWhenGetSipExternalLibInterfaceIsCalledThenNullptrReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GetSipBinaryFromExternalLib.set(1);
    EXPECT_EQ(nullptr, pDevice->getSipExternalLibInterface());
}
