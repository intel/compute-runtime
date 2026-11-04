/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/windows/mock_shared_section.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/test.h"

#include <array>

using namespace NEO;

struct SharedSectionTest : public ::testing::Test {
    static constexpr size_t sectionSize = 2 * MemoryConstants::pageSize;
    static constexpr size_t tailSize = sizeof(SharedSection::SharedMemoryContentTail);

    VariableBackup<MEMORY_BASIC_INFORMATION> virtualQueryMemoryBasicInformationBackup{&SysCalls::virtualQueryMemoryBasicInformation};
    VariableBackup<SIZE_T> virtualQueryResultBackup{&SysCalls::virtualQueryResult};
};

TEST_F(SharedSectionTest, givenExportedParamsWhenInitializingThenExportedParamsAreReturnedWithExporterFieldsReset) {
    std::array<uint8_t, sectionSize> memory = {};
    GMM_RESCREATE_PARAMS exported = {};
    exported.ExistingSysMemSize = MemoryConstants::pageSize;
    exported.Flags.Info.Shared = 1;
    exported.pExistingSysMem = 0x1234000u;
    exported.pPreallocatedResInfo = reinterpret_cast<GMM_RESOURCE_INFO *>(0x5678);
    exported.pPrivateData = 0x9abcu;
    exported.PrivateDataSize = 16u;
    writeSharedSectionTail(memory.data(), memory.size(), exported, 0u);
    SysCalls::virtualQueryMemoryBasicInformation.RegionSize = memory.size();
    SharedSection section(memory.data());

    ASSERT_TRUE(section.initialize());

    auto expected = exported;
    expected.pExistingSysMem = castToUint64(memory.data());
    expected.Flags.Info.Shared = 0;
    expected.pPreallocatedResInfo = nullptr;
    expected.pPrivateData = 0u;
    expected.PrivateDataSize = 0u;
    EXPECT_EQ(0, memcmp(&expected, &section.getGmmParams(), sizeof(expected)));
}

TEST_F(SharedSectionTest, givenQueryingViewSizeFailsWhenInitializingThenFalseIsReturned) {
    std::array<uint8_t, sectionSize> memory = {};
    SysCalls::virtualQueryResult = 0u;
    SharedSection section(memory.data());

    EXPECT_FALSE(section.initialize());
}

TEST_F(SharedSectionTest, givenViewSmallerThanTailWhenInitializingThenFalseIsReturned) {
    std::array<uint8_t, sectionSize> memory = {};
    SysCalls::virtualQueryMemoryBasicInformation.RegionSize = tailSize - 1;
    SharedSection section(memory.data());

    EXPECT_FALSE(section.initialize());
}

TEST_F(SharedSectionTest, givenSecurityDescriptorLargerThanViewWhenInitializingThenFalseIsReturned) {
    std::array<uint8_t, sectionSize> memory = {};
    GMM_RESCREATE_PARAMS exported = {};
    exported.ExistingSysMemSize = 1u;
    writeSharedSectionTail(memory.data(), memory.size(), exported, static_cast<uint32_t>(sectionSize - tailSize + 1));
    SysCalls::virtualQueryMemoryBasicInformation.RegionSize = memory.size();
    SharedSection section(memory.data());

    EXPECT_FALSE(section.initialize());
}

TEST_F(SharedSectionTest, givenExistingSysMemSizeWhenInitializingThenItMustFitInViewAndUsableSizeExcludesMetadata) {
    constexpr uint32_t securityDescriptorSize = 64u;
    constexpr size_t sizeBeforeMetadata = sectionSize - tailSize - securityDescriptorSize;

    struct {
        uint64_t existingSysMemSize;
        bool expectedResult;
        size_t expectedUsableSize;
    } cases[] = {{0u, false, 0u},
                 {1u, true, 1u},
                 {sizeBeforeMetadata, true, sizeBeforeMetadata},
                 {sectionSize, true, sizeBeforeMetadata},
                 {sectionSize + 1, false, 0u}};

    for (auto &testCase : cases) {
        std::array<uint8_t, sectionSize> memory = {};
        GMM_RESCREATE_PARAMS exported = {};
        exported.ExistingSysMemSize = testCase.existingSysMemSize;
        writeSharedSectionTail(memory.data(), memory.size(), exported, securityDescriptorSize);
        SysCalls::virtualQueryMemoryBasicInformation.RegionSize = memory.size();
        SharedSection section(memory.data());

        EXPECT_EQ(testCase.expectedResult, section.initialize()) << testCase.existingSysMemSize;
        EXPECT_EQ(testCase.expectedUsableSize, section.getUsableSize()) << testCase.existingSysMemSize;
    }
}

struct SharedSectionIsSectionTest : public ::testing::Test {
    const HANDLE handle = reinterpret_cast<HANDLE>(0x4321);
    VariableBackup<size_t> ntQueryObjectCalledBackup{&SysCalls::ntQueryObjectCalled, 0u};
    VariableBackup<NTSTATUS> ntQueryObjectResultBackup{&SysCalls::ntQueryObjectResult, STATUS_SUCCESS};
    VariableBackup<const wchar_t *> ntQueryObjectTypeNameBackup{&SysCalls::ntQueryObjectTypeName, nullptr};
    VariableBackup<HANDLE> ntQueryObjectLastHandleBackup{&SysCalls::ntQueryObjectLastHandle, nullptr};
    VariableBackup<OBJECT_INFORMATION_CLASS> ntQueryObjectLastInformationClassBackup{&SysCalls::ntQueryObjectLastInformationClass, ObjectBasicInformation};
    VariableBackup<ULONG> ntQueryObjectLastInformationLengthBackup{&SysCalls::ntQueryObjectLastInformationLength, 0u};
};

TEST_F(SharedSectionIsSectionTest, givenHandleOfSectionTypeWhenCheckingIfHandleIsSectionThenTypeInformationIsQueriedAndTrueIsReturned) {
    SysCalls::ntQueryObjectTypeName = L"Section";

    EXPECT_TRUE(SharedSection::isSection(handle));
    EXPECT_EQ(1u, SysCalls::ntQueryObjectCalled);
    EXPECT_EQ(handle, SysCalls::ntQueryObjectLastHandle);
    EXPECT_EQ(ObjectTypeInformation, SysCalls::ntQueryObjectLastInformationClass);
    EXPECT_LE(sizeof(PUBLIC_OBJECT_TYPE_INFORMATION) + sizeof(L"Section"), SysCalls::ntQueryObjectLastInformationLength);
}

TEST_F(SharedSectionIsSectionTest, givenQueryingTypeInformationFailsWhenCheckingIfHandleIsSectionThenFalseIsReturned) {
    SysCalls::ntQueryObjectTypeName = L"Section";
    SysCalls::ntQueryObjectResult = STATUS_INVALID_HANDLE;

    EXPECT_FALSE(SharedSection::isSection(handle));
    EXPECT_EQ(1u, SysCalls::ntQueryObjectCalled);
}

TEST_F(SharedSectionIsSectionTest, givenTypeNameWithoutBufferWhenCheckingIfHandleIsSectionThenFalseIsReturned) {
    SysCalls::ntQueryObjectTypeName = nullptr;

    EXPECT_FALSE(SharedSection::isSection(handle));
    EXPECT_EQ(1u, SysCalls::ntQueryObjectCalled);
}

TEST_F(SharedSectionIsSectionTest, givenHandleOfOtherTypeWhenCheckingIfHandleIsSectionThenFalseIsReturned) {
    for (auto typeName : {L"Event", L"Sectio", L"SectionX", L"Sxction"}) {
        SysCalls::ntQueryObjectTypeName = typeName;
        EXPECT_FALSE(SharedSection::isSection(handle));
    }
}
