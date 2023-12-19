/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmUuidTest, GivenDrmWhenGeneratingUUIDThenCorrectStringsAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto uuid1 = drm.generateUUID();
    auto uuid2 = drm.generateUUID();

    std::string uuidff;
    for (int i = 0; i < 0xff - 2; i++) {
        uuidff = drm.generateUUID();
    }

    EXPECT_STREQ("00000000-0000-0000-0000-000000000001", uuid1.c_str());
    EXPECT_STREQ("00000000-0000-0000-0000-000000000002", uuid2.c_str());
    EXPECT_STREQ("00000000-0000-0000-0000-0000000000ff", uuidff.c_str());
}

TEST(DrmUuidTest, GivenDrmWhenGeneratingElfUUIDThenCorrectStringsAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    std::string elfClassUuid = classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::elf)].second;
    std::string uuid1stElfClass = elfClassUuid.substr(0, 18);

    char data[] = "abc";
    auto uuid1 = drm.generateElfUUID(static_cast<const void *>(data));
    std::string uuid1stElfBin1 = uuid1.substr(0, 18);
    EXPECT_STREQ(uuid1stElfClass.c_str(), uuid1stElfBin1.c_str());

    char data2[] = "123";
    auto uuid2 = drm.generateElfUUID(static_cast<const void *>(data2));
    std::string uuid1stElfBin2 = uuid2.substr(0, 18);
    EXPECT_STREQ(uuid1stElfClass.c_str(), uuid1stElfBin2.c_str());

    auto uuid3 = drm.generateElfUUID(reinterpret_cast<const void *>(0xFFFFFFFFFFFFFFFF));
    std::string uuidElf = uuid1stElfClass + "-ffff-ffffffffffff";
    EXPECT_STREQ(uuidElf.c_str(), uuid3.c_str());
}

TEST(DrmUuidTest, whenResourceClassIsUsedToIndexClassNamesThenCorrectNamesAreReturned) {
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::elf)].first, "I915_UUID_CLASS_ELF_BINARY");
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::isa)].first, "I915_UUID_CLASS_ISA_BYTECODE");
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::contextSaveArea)].first, "I915_UUID_L0_SIP_AREA");
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::moduleHeapDebugArea)].first, "I915_UUID_L0_MODULE_AREA");
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::sbaTrackingBuffer)].first, "I915_UUID_L0_SBA_AREA");
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::contextID)].first, "I915_UUID_L0_CONTEXT_ID");
    EXPECT_STREQ(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::l0ZebinModule)].first, "L0_ZEBIN_MODULE");
}

TEST(DrmUuidTest, givenUuidStringWhenGettingClassIndexThenCorrectIndexForValidStringsIsReturned) {
    uint32_t index = 100;
    auto validUuid = DrmUuid::getClassUuidIndex(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::contextSaveArea)].second, index);

    EXPECT_TRUE(validUuid);
    EXPECT_EQ(static_cast<uint32_t>(DrmResourceClass::contextSaveArea), index);

    validUuid = DrmUuid::getClassUuidIndex(classNamesToUuid[static_cast<uint32_t>(DrmResourceClass::moduleHeapDebugArea)].second, index);

    EXPECT_TRUE(validUuid);
    EXPECT_EQ(static_cast<uint32_t>(DrmResourceClass::moduleHeapDebugArea), index);

    index = 100;
    validUuid = DrmUuid::getClassUuidIndex("invalid", index);

    EXPECT_FALSE(validUuid);
    EXPECT_EQ(100u, index);
}
