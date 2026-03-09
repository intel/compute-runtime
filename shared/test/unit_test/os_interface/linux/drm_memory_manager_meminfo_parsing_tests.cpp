/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

class DrmMemoryManagerParseMeminfoAccessor : public DrmMemoryManager {
  public:
    using DrmMemoryManager::parseMeminfo;
};

TEST(DrmMemoryManagerParseMeminfoTest, givenValidMeminfoContentWhenParsingThenExtractsCorrectValues) {
    std::string meminfoContent = R"(MemTotal:       32044356 kB
MemFree:        12591564 kB
MemAvailable:   30397864 kB
Buffers:         1193952 kB
Cached:         15536900 kB
)";

    uint64_t totalBytes = 0, freeBytes = 0;
    bool result = DrmMemoryManagerParseMeminfoAccessor::parseMeminfo(meminfoContent, totalBytes, freeBytes);

    EXPECT_TRUE(result);
    EXPECT_EQ(32044356 * 1024ULL, totalBytes);
    EXPECT_EQ(30397864 * 1024ULL, freeBytes);
}

TEST(DrmMemoryManagerParseMeminfoTest, givenMeminfoMissingMemTotalWhenParsingThenReturnsFalse) {
    std::string meminfoContent = R"(MemAvailable:   20123456 kB
Buffers:          123456 kB
)";

    uint64_t totalBytes = 0, freeBytes = 0;
    bool result = DrmMemoryManagerParseMeminfoAccessor::parseMeminfo(meminfoContent, totalBytes, freeBytes);

    EXPECT_FALSE(result);
}

TEST(DrmMemoryManagerParseMeminfoTest, givenMeminfoMissingMemAvailableWhenParsingThenReturnsFalse) {
    std::string meminfoContent = R"(MemTotal:       32874152 kB
MemFree:        12345678 kB
Buffers:          123456 kB
)";

    uint64_t totalBytes = 0, freeBytes = 0;
    bool result = DrmMemoryManagerParseMeminfoAccessor::parseMeminfo(meminfoContent, totalBytes, freeBytes);

    EXPECT_FALSE(result);
}

TEST(DrmMemoryManagerParseMeminfoTest, givenEmptyContentWhenParsingThenReturnsFalse) {
    std::string meminfoContent = "";

    uint64_t totalBytes = 0, freeBytes = 0;
    bool result = DrmMemoryManagerParseMeminfoAccessor::parseMeminfo(meminfoContent, totalBytes, freeBytes);

    EXPECT_FALSE(result);
}

TEST(DrmMemoryManagerParseMeminfoTest, givenMeminfoWithInvalidNumberWhenParsingThenReturnsFalse) {
    std::string meminfoContent = R"(MemTotal:       invalid kB
MemAvailable:   invalid kB
)";

    uint64_t totalBytes = 0, freeBytes = 0;
    bool result = DrmMemoryManagerParseMeminfoAccessor::parseMeminfo(meminfoContent, totalBytes, freeBytes);

    EXPECT_FALSE(result);
}

TEST(DrmMemoryManagerParseMeminfoTest, givenMeminfoWithoutTrailingNewlineInTheEndWhenParsingThenSucceeds) {
    std::string meminfoContent = "MemTotal:       1024 kB\nMemAvailable:   512 kB";

    uint64_t totalBytes = 0, freeBytes = 0;
    bool result = DrmMemoryManagerParseMeminfoAccessor::parseMeminfo(meminfoContent, totalBytes, freeBytes);

    EXPECT_TRUE(result);
    EXPECT_EQ(1024 * 1024ULL, totalBytes);
    EXPECT_EQ(512 * 1024ULL, freeBytes);
}
