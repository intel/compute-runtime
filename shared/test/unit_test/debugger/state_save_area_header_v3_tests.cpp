/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/state_save_area_header_v3.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(StateSaveAreaHeaderV3Test, givenVersionsWhenCheckingMinor1LayoutThenOnlyMajor3WithMinorAtLeast1UsesIt) {
    StateSaveAreaHeader header = {};

    header.versionHeader.version.major = 3;
    header.versionHeader.version.minor = 0;
    EXPECT_FALSE(isStateSaveAreaV3Minor1Layout(header));

    header.versionHeader.version.minor = 1;
    EXPECT_TRUE(isStateSaveAreaV3Minor1Layout(header));

    header.versionHeader.version.minor = 2;
    EXPECT_TRUE(isStateSaveAreaV3Minor1Layout(header));

    header.versionHeader.version.major = 5;
    header.versionHeader.version.minor = 1;
    EXPECT_FALSE(isStateSaveAreaV3Minor1Layout(header));
}

TEST(StateSaveAreaHeaderV3Test, givenV3Minor0HeaderWhenReadingFieldsThenV3LayoutIsUsed) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(3);
    const auto &header = *reinterpret_cast<const StateSaveAreaHeader *>(stateSaveAreaHeader.data());

    EXPECT_EQ(header.regHeaderV3.sr_magic_offset, getStateSaveAreaV3SrMagicOffset(header));
    EXPECT_EQ(header.regHeaderV3.sip_flags, getStateSaveAreaV3SipFlags(header));
    EXPECT_EQ(header.regHeaderV3.fifo_offset, getStateSaveAreaV3FifoOffset(header));
    EXPECT_EQ(header.regHeaderV3.fifo_size, getStateSaveAreaV3FifoSize(header));

    const auto fifoIndexOffsets = getStateSaveAreaV3FifoIndexOffsets(header);
    EXPECT_EQ(sizeof(SIP::StateSaveArea) + offsetof(SIP::intelgt_state_save_area_V3, fifo_size), fifoIndexOffsets.fifoSize);
    EXPECT_EQ(sizeof(SIP::StateSaveArea) + offsetof(SIP::intelgt_state_save_area_V3, fifo_head), fifoIndexOffsets.fifoHead);
    EXPECT_EQ(sizeof(SIP::StateSaveArea) + offsetof(SIP::intelgt_state_save_area_V3, fifo_tail), fifoIndexOffsets.fifoTail);
}

TEST(StateSaveAreaHeaderV3Test, givenV3Minor1HeaderWhenReadingFieldsThenV3Minor1LayoutIsUsed) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeaderV3Minor1();
    const auto &header = *reinterpret_cast<const StateSaveAreaHeader *>(stateSaveAreaHeader.data());

    EXPECT_EQ(header.regHeaderV3Minor1.sr_magic_offset, getStateSaveAreaV3SrMagicOffset(header));
    EXPECT_EQ(header.regHeaderV3Minor1.sip_flags, getStateSaveAreaV3SipFlags(header));
    EXPECT_EQ(header.regHeaderV3Minor1.fifo_offset, getStateSaveAreaV3FifoOffset(header));
    EXPECT_EQ(header.regHeaderV3Minor1.fifo_size, getStateSaveAreaV3FifoSize(header));

    const auto fifoIndexOffsets = getStateSaveAreaV3FifoIndexOffsets(header);
    EXPECT_EQ(sizeof(SIP::StateSaveArea) + offsetof(SIP::intelgt_state_save_area_V3_1, fifo_size), fifoIndexOffsets.fifoSize);
    EXPECT_EQ(sizeof(SIP::StateSaveArea) + offsetof(SIP::intelgt_state_save_area_V3_1, fifo_head), fifoIndexOffsets.fifoHead);
    EXPECT_EQ(sizeof(SIP::StateSaveArea) + offsetof(SIP::intelgt_state_save_area_V3_1, fifo_tail), fifoIndexOffsets.fifoTail);
}
