/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;
using namespace SWTags;

constexpr static uint32_t testOpCode = 42;

struct TestTag : public BaseTag {
    TestTag() : BaseTag(static_cast<OpCode>(testOpCode), sizeof(TestTag)) {}

    char testString[5] = "Test";
    bool testBool = true;
    uint16_t testWord = 42;
    uint32_t testDword = 42;
};

struct VeryLargeTag : public BaseTag {
    VeryLargeTag() : BaseTag(static_cast<OpCode>(testOpCode), sizeof(VeryLargeTag)) {}

    char largeBuffer[1025] = {};
};

struct SoftwareTagsManagerTests : public DeviceFixture, public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableSWTags.set(true);
        DeviceFixture::setUp();

        tagsManager = pDevice->getRootDeviceEnvironment().tagsManager.get();

        ASSERT_TRUE(tagsManager->isInitialized());
        ASSERT_NE(nullptr, tagsManager->getBXMLHeapAllocation());
        ASSERT_NE(nullptr, tagsManager->getSWTagHeapAllocation());
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    template <typename GfxFamily>
    void initializeTestCmdStream() {
        const AllocationProperties properties{
            pDevice->getRootDeviceIndex(),
            SWTagsManager::estimateSpaceForSWTags<GfxFamily>(),
            AllocationType::linearStream,
            pDevice->getDeviceBitfield()};

        GraphicsAllocation *allocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        testCmdStream = std::make_unique<LinearStream>(allocation);
    }

    void freeTestCmdStream() {
        pDevice->getMemoryManager()->freeGraphicsMemory(testCmdStream->getGraphicsAllocation());
    }

    SWTagsManager *tagsManager;
    std::unique_ptr<LinearStream> testCmdStream;
    DebugManagerStateRestore dbgRestorer;
};

TEST_F(SoftwareTagsManagerTests, whenSWTagsMangerIsInitializedThenHeapAllocationsAreCorrect) {
    auto memoryMgr = pDevice->getMemoryManager();
    SWTagBXML bxml;
    BXMLHeapInfo bxmlInfo((sizeof(BXMLHeapInfo) + bxml.str.size() + 1) / sizeof(uint32_t));
    SWTagHeapInfo tagInfo(SWTagsManager::maxTagHeapSize / sizeof(uint32_t));
    auto bxmlHeap = tagsManager->getBXMLHeapAllocation();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    auto ptr = memoryMgr->lockResource(bxmlHeap);
    auto pBxmlInfo = reinterpret_cast<BXMLHeapInfo *>(ptr);

    EXPECT_EQ(bxmlInfo.component, pBxmlInfo->component);
    EXPECT_EQ(bxmlInfo.heapSize, pBxmlInfo->heapSize);
    EXPECT_EQ(bxmlInfo.magicNumber, pBxmlInfo->magicNumber);
    EXPECT_EQ(0, memcmp(bxml.str.c_str(), ptrOffset(ptr, sizeof(BXMLHeapInfo)), bxml.str.size()));

    memoryMgr->unlockResource(bxmlHeap);

    ptr = memoryMgr->lockResource(tagHeap);
    auto pTagInfo = reinterpret_cast<SWTagHeapInfo *>(ptr);

    EXPECT_EQ(tagInfo.component, pTagInfo->component);
    EXPECT_EQ(tagInfo.heapSize, pTagInfo->heapSize);
    EXPECT_EQ(tagInfo.magicNumber, pTagInfo->magicNumber);

    memoryMgr->unlockResource(tagHeap);
}

HWTEST_F(SoftwareTagsManagerTests, whenHeapsAddressesAreInsertedThenCmdStreamHasCorrectContents) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    initializeTestCmdStream<FamilyType>();

    tagsManager->insertBXMLHeapAddress<FamilyType>(*testCmdStream.get());
    tagsManager->insertSWTagHeapAddress<FamilyType>(*testCmdStream.get());

    EXPECT_EQ(testCmdStream->getUsed(), 2 * sizeof(MI_STORE_DATA_IMM));

    void *bufferBase = testCmdStream->getCpuBase();
    auto sdi1 = reinterpret_cast<MI_STORE_DATA_IMM *>(bufferBase);
    auto sdi2 = reinterpret_cast<MI_STORE_DATA_IMM *>(ptrOffset(bufferBase, sizeof(MI_STORE_DATA_IMM)));
    auto bxmlHeap = tagsManager->getBXMLHeapAllocation();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    EXPECT_EQ(sdi1->getAddress(), bxmlHeap->getGpuAddress());
    EXPECT_EQ(sdi2->getAddress(), tagHeap->getGpuAddress());

    freeTestCmdStream();
}

HWTEST_F(SoftwareTagsManagerTests, whenTestTagIsInsertedThenItIsSuccessful) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    initializeTestCmdStream<FamilyType>();

    tagsManager->insertTag<FamilyType, TestTag>(*testCmdStream.get(), *pDevice);

    EXPECT_EQ(testCmdStream->getUsed(), 2 * sizeof(MI_NOOP));

    void *bufferBase = testCmdStream->getCpuBase();
    auto markerNoop = reinterpret_cast<MI_NOOP *>(bufferBase);
    auto offsetNoop = reinterpret_cast<MI_NOOP *>(ptrOffset(bufferBase, sizeof(MI_NOOP)));

    EXPECT_EQ(BaseTag::getMarkerNoopID(static_cast<OpCode>(testOpCode)), markerNoop->getIdentificationNumber());
    EXPECT_EQ(true, markerNoop->getIdentificationNumberRegisterWriteEnable());

    uint32_t firstTagOffset = sizeof(SWTagHeapInfo); // SWTagHeapInfo is always on offset 0, first tag is inserted immediately after.

    EXPECT_EQ(BaseTag::getOffsetNoopID(firstTagOffset), offsetNoop->getIdentificationNumber());
    EXPECT_EQ(false, offsetNoop->getIdentificationNumberRegisterWriteEnable());

    auto memoryMgr = pDevice->getMemoryManager();
    auto tagHeap = tagsManager->getSWTagHeapAllocation();

    TestTag tag;
    auto ptr = memoryMgr->lockResource(tagHeap);
    auto pTag = reinterpret_cast<TestTag *>(ptrOffset(ptr, firstTagOffset));

    EXPECT_EQ(0, strcmp(tag.testString, pTag->testString));
    EXPECT_EQ(tag.testBool, pTag->testBool);
    EXPECT_EQ(tag.testWord, pTag->testWord);
    EXPECT_EQ(tag.testDword, pTag->testDword);

    memoryMgr->unlockResource(tagHeap);
    freeTestCmdStream();
}

HWTEST_F(SoftwareTagsManagerTests, givenSoftwareManagerWithMaxTagsReachedWhenTagIsInsertedThenItIsNotSuccessful) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    initializeTestCmdStream<FamilyType>();

    EXPECT_TRUE(tagsManager->maxTagHeapSize > (tagsManager->maxTagCount + 1) * sizeof(TestTag));

    for (unsigned int i = 0; i <= tagsManager->maxTagCount; ++i) {
        tagsManager->insertTag<FamilyType, TestTag>(*testCmdStream.get(), *pDevice);
    }

    EXPECT_EQ(testCmdStream->getUsed(), tagsManager->maxTagCount * 2 * sizeof(MI_NOOP));

    tagsManager->insertTag<FamilyType, TestTag>(*testCmdStream.get(), *pDevice);

    EXPECT_EQ(testCmdStream->getUsed(), tagsManager->maxTagCount * 2 * sizeof(MI_NOOP));

    freeTestCmdStream();
}

HWTEST_F(SoftwareTagsManagerTests, givenSoftwareManagerWithMaxHeapReachedWhenTagIsInsertedThenItIsNotSuccessful) {

    initializeTestCmdStream<FamilyType>();

    size_t prevHeapOffset = tagsManager->getCurrentHeapOffset();

    uint32_t i = 0;
    while (tagsManager->getCurrentHeapOffset() + sizeof(VeryLargeTag) <= NEO::SWTagsManager::maxTagHeapSize) {
        tagsManager->insertTag<FamilyType, VeryLargeTag>(*testCmdStream.get(), *pDevice);
        i++;
    }

    EXPECT_EQ(tagsManager->getCurrentHeapOffset(), prevHeapOffset + (i * sizeof(VeryLargeTag)));

    tagsManager->insertTag<FamilyType, VeryLargeTag>(*testCmdStream.get(), *pDevice);

    EXPECT_EQ(tagsManager->getCurrentHeapOffset(), prevHeapOffset + (i * sizeof(VeryLargeTag)));

    freeTestCmdStream();
}

TEST(SoftwareTagsManagerMultiDeviceTests, givenEnableSWTagsAndCreateMultipleSubDevicesWhenDeviceCreatedThenSWTagsManagerIsInitializedOnlyOnce) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableSWTags.set(true);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    // This test checks if UNRECOVERABLE_IF(...) was not called
    MockDevice *device = nullptr;
    EXPECT_NO_THROW(device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_NE(device, nullptr);

    delete device;
}

struct SoftwareTagsParametrizedTests : public ::testing::TestWithParam<SWTags::OpCode> {
    void SetUp() override {
        tagMap.emplace(OpCode::kernelName, std::make_unique<KernelNameTag>("", 0u));
        tagMap.emplace(OpCode::pipeControlReason, std::make_unique<PipeControlReasonTag>("", 0u));
        tagMap.emplace(OpCode::arbitraryString, std::make_unique<ArbitraryStringTag>(""));
    }

    std::map<OpCode, std::unique_ptr<BaseTag>> tagMap;
};

INSTANTIATE_TEST_SUITE_P(
    SoftwareTags,
    SoftwareTagsParametrizedTests,
    testing::Values(
        OpCode::kernelName,
        OpCode::pipeControlReason,
        OpCode::arbitraryString));

TEST_P(SoftwareTagsParametrizedTests, whenGetOpCodeIsCalledThenCorrectValueIsReturned) {
    auto opcode = GetParam();
    auto tag = tagMap.at(opcode).get();

    EXPECT_EQ(opcode, tag->getOpCode());
}

TEST(SoftwareTagsTests, whenGetMarkerNoopIDCalledThenCorectValueIsReturned) {
    uint32_t id = SWTags::BaseTag::getMarkerNoopID(static_cast<OpCode>(testOpCode)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    EXPECT_EQ(testOpCode, id);
}

TEST(SoftwareTagsTests, whenGetOffsetNoopIDCalledThenCorrectValueIsReturned) {
    uint32_t address1 = 0;
    uint32_t address2 = 1234;
    uint32_t id1 = BaseTag::getOffsetNoopID(address1);
    uint32_t id2 = BaseTag::getOffsetNoopID(address2);

    EXPECT_EQ(static_cast<uint32_t>(1 << 21) | address1 / sizeof(uint32_t), id1);
    EXPECT_EQ(static_cast<uint32_t>(1 << 21) | address2 / sizeof(uint32_t), id2);
}

TEST(SoftwareTagsBXMLTests, givenDumpSWTagsBXMLWhenConstructingBXMLThenAFileIsDumped) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DumpSWTagsBXML.set(true);

    uint32_t mockFopenCalledBefore = IoFunctions::mockFopenCalled;
    uint32_t mockFwriteCalledBefore = IoFunctions::mockFwriteCalled;
    uint32_t mockFcloseCalledBefore = IoFunctions::mockFcloseCalled;

    VariableBackup<size_t> mockFwriteReturnBackup(&IoFunctions::mockFwriteReturn, 10'000);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(IoFunctions::mockFwriteReturn);
    VariableBackup<char *> mockFwriteBufferBackup(&IoFunctions::mockFwriteBuffer, buffer.get());
    SWTagBXML bxml1;

    EXPECT_EQ(IoFunctions::mockFopenCalled, mockFopenCalledBefore + 1);
    EXPECT_EQ(IoFunctions::mockFwriteCalled, mockFwriteCalledBefore + 1);
    EXPECT_EQ(IoFunctions::mockFcloseCalled, mockFcloseCalledBefore + 1);
    EXPECT_GE(IoFunctions::mockFwriteReturn, bxml1.str.size());
    EXPECT_STREQ(buffer.get(), bxml1.str.c_str());

    VariableBackup<FILE *> backup(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    SWTagBXML bxml2;

    EXPECT_EQ(IoFunctions::mockFopenCalled, mockFopenCalledBefore + 2);
    EXPECT_EQ(IoFunctions::mockFwriteCalled, mockFwriteCalledBefore + 1);
    EXPECT_EQ(IoFunctions::mockFcloseCalled, mockFcloseCalledBefore + 1);
}
