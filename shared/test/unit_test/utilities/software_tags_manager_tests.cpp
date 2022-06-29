/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
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
        DebugManager.flags.EnableSWTags.set(true);
        DeviceFixture::SetUp();

        tagsManager = pDevice->getRootDeviceEnvironment().tagsManager.get();

        ASSERT_TRUE(tagsManager->isInitialized());
        ASSERT_NE(nullptr, tagsManager->getBXMLHeapAllocation());
        ASSERT_NE(nullptr, tagsManager->getSWTagHeapAllocation());
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    template <typename GfxFamily>
    void initializeTestCmdStream() {
        const AllocationProperties properties{
            pDevice->getRootDeviceIndex(),
            SWTagsManager::estimateSpaceForSWTags<GfxFamily>(),
            AllocationType::LINEAR_STREAM,
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
    using MI_NOOP = typename FamilyType::MI_NOOP;

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
    DebugManager.flags.EnableSWTags.set(true);
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    // This test checks if UNRECOVERABLE_IF(...) was not called
    MockDevice *device = nullptr;
    EXPECT_NO_THROW(device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_NE(device, nullptr);

    delete device;
}

struct SoftwareTagsParametrizedTests : public ::testing::TestWithParam<SWTags::OpCode> {
    void SetUp() override {
        tagMap.emplace(OpCode::KernelName, std::make_unique<KernelNameTag>("", 0u));
        tagMap.emplace(OpCode::PipeControlReason, std::make_unique<PipeControlReasonTag>("", 0u));
    }

    std::map<OpCode, std::unique_ptr<BaseTag>> tagMap;
};

INSTANTIATE_TEST_CASE_P(
    SoftwareTags,
    SoftwareTagsParametrizedTests,
    testing::Values(
        OpCode::KernelName,
        OpCode::PipeControlReason));

TEST_P(SoftwareTagsParametrizedTests, whenGetOpCodeIsCalledThenCorrectValueIsReturned) {
    auto opcode = GetParam();
    auto tag = tagMap.at(opcode).get();

    EXPECT_EQ(opcode, tag->getOpCode());
}

TEST(SoftwareTagsTests, whenGetMarkerNoopIDCalledThenCorectValueIsReturned) {
    uint32_t id = SWTags::BaseTag::getMarkerNoopID(static_cast<OpCode>(testOpCode));

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
    DebugManager.flags.DumpSWTagsBXML.set(true);

    const char *filename = "swtagsbxml_dump.xml";
    SWTagBXML bxml;

    size_t retSize;
    auto data = loadDataFromFile(filename, retSize);

    EXPECT_EQ(retSize, bxml.str.size());
    EXPECT_EQ(0, strcmp(data.get(), bxml.str.c_str()));

    writeDataToFile(filename, "", 1);
}
