/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/pnext.h"

#include "gtest/gtest.h"

TEST(PNextRange, GivenNullPnextThenCreatesEmptyRange) {
    L0::PNextRange range{static_cast<void *>(nullptr)};
    EXPECT_EQ(0U, range.size());
    EXPECT_FALSE(range.contains(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC));
    EXPECT_EQ(nullptr, range.get(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC));
    for (const auto &ext : range) {
        EXPECT_FALSE(true) << ext.stype; // should be unreachable sinc range is empty
    }
}

TEST(PNextRange, GivenSinglePnextThenCreatesRangeWithSingleElement) {
    ze_module_program_exp_desc_t modProgDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    L0::PNextRange range{&modProgDesc};
    EXPECT_EQ(1U, range.size());
    EXPECT_TRUE(range.contains(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC));
    EXPECT_EQ(&modProgDesc, range.get<ze_module_program_exp_desc_t>(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC));
    EXPECT_FALSE(range.contains(ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC));
    EXPECT_EQ(nullptr, range.get(ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC));
    auto iterCount = 0;
    for (const auto &ext : range) {
        ++iterCount;
        EXPECT_EQ(reinterpret_cast<ze_base_desc_t *>(&modProgDesc), &ext);
    }
    EXPECT_EQ(1, iterCount);
}

TEST(PNextRange, GivenPnextChainThenCreatesRangeOverPnextChain) {
    ze_module_program_exp_desc_t modProgDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    ze_image_bindless_exp_desc_t bindlessImageDesc = {ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC};
    modProgDesc.pNext = &bindlessImageDesc;
    L0::PNextRange range{&modProgDesc};
    EXPECT_EQ(2U, range.size());
    EXPECT_TRUE(range.contains(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC));
    EXPECT_EQ(&modProgDesc, range.get<ze_module_program_exp_desc_t>(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC));
    EXPECT_TRUE(range.contains(ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC));
    EXPECT_EQ(&bindlessImageDesc, range.get<ze_image_bindless_exp_desc_t>(ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC));
    EXPECT_FALSE(range.contains(ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC));
    EXPECT_EQ(nullptr, range.get(ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC));
    auto iterCount = 0;
    ze_base_desc_t *expectedEntries[] = {reinterpret_cast<ze_base_desc_t *>(&modProgDesc),
                                         reinterpret_cast<ze_base_desc_t *>(&bindlessImageDesc)};
    for (const auto &ext : range) {
        EXPECT_EQ(expectedEntries[iterCount], &ext);
        ++iterCount;
    }
    EXPECT_EQ(2, iterCount);

    // iterator tests
    auto it = range.begin();
    EXPECT_EQ(expectedEntries[0], &*it);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC, it->stype);

    auto it2 = it;
    EXPECT_EQ(it, it2);

    EXPECT_EQ(it, it2++);
    EXPECT_NE(it, it2);
    EXPECT_EQ(++it, it2);
    EXPECT_EQ(expectedEntries[1], &*it);

    EXPECT_NE(range.end(), it);
    ++it;
    EXPECT_EQ(range.end(), it);
}
