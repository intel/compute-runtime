/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_indirect_data_fixture.h"

namespace L0 {
namespace ult {

using MutableIndirectDataTest = Test<MutableIndirectDataFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenNoOffsetsDefinedWhenProgramingMutableParametersThenNoDataWritten) {

    std::array<uint32_t, 3> data = {0xFF, 0xFF, 0xFF};

    this->crossThreadDataSize = 256;
    auto expectedData = std::make_unique<uint8_t[]>(this->crossThreadDataSize);
    memset(expectedData.get(), 0, this->crossThreadDataSize);
    undefineOffsets();
    createMutableIndirectOffset();

    this->indirectData->setAddress(undefinedOffset, 0xFF000, sizeof(uint64_t));
    this->indirectData->setEnqLocalWorkSize(data);
    this->indirectData->setGlobalWorkOffset(data);
    this->indirectData->setGlobalWorkSize(data);
    this->indirectData->setLocalWorkSize(data);
    this->indirectData->setLocalWorkSize2(data);
    this->indirectData->setNumWorkGroups(data);
    this->indirectData->setWorkDimensions(3);

    EXPECT_EQ(0, memcmp(expectedData.get(), this->crossThreadData.get(), this->crossThreadDataSize));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenNoInlineDataWhenSettingAddressThenAddressSetInCrossThread) {

    this->crossThreadDataSize = 256;

    createMutableIndirectOffset();

    constexpr uint64_t address = 0xFF000;
    constexpr L0::MCL::CrossThreadDataOffset offset = 64;

    this->indirectData->setAddress(offset, address, sizeof(uint64_t));

    uint64_t crossThreadAddress = 0;
    memcpy(&crossThreadAddress, ptrOffset(this->crossThreadData.get(), offset), sizeof(uint64_t));
    EXPECT_EQ(address, crossThreadAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenInlineDataWhenSettingAddressInInlineOffsetThenAddressSetInInline) {

    this->crossThreadDataSize = 256;
    this->inlineSize = 64;

    createMutableIndirectOffset();

    constexpr uint64_t address = 0xFF000;
    constexpr L0::MCL::CrossThreadDataOffset offset = 32;

    this->indirectData->setAddress(offset, address, sizeof(uint64_t));

    uint64_t inlineAddress = 0;
    memcpy(&inlineAddress, ptrOffset(this->inlineData.get(), offset), sizeof(uint64_t));
    EXPECT_EQ(address, inlineAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenInlineDataWhenSettingAddressInCrossThreadOffsetThenAddressSetInCrossThreadWithShiftedOffsetByInlineSize) {

    constexpr L0::MCL::CrossThreadDataOffset inlineSize = 64;
    this->crossThreadDataSize = 256;
    this->inlineSize = inlineSize;

    createMutableIndirectOffset();

    constexpr uint64_t address = 0xFF000;
    constexpr L0::MCL::CrossThreadDataOffset offset = 128;

    this->indirectData->setAddress(offset, address, sizeof(uint64_t));

    uint64_t crossThreadAddress = 0;
    memcpy(&crossThreadAddress, ptrOffset(this->crossThreadData.get(), offset - inlineSize), sizeof(uint64_t));
    EXPECT_EQ(address, crossThreadAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenNoInlineDataWhenSettingDataThenDataSetInCrossThread) {

    this->crossThreadDataSize = 256;
    this->offsets.globalWorkSize = 64;
    createMutableIndirectOffset();

    std::array<uint32_t, 3> data = {0xF0, 0x0F, 0x7F};

    this->indirectData->setGlobalWorkSize(data);

    auto gwsCrossThread = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.globalWorkSize));
    EXPECT_EQ(data[0], gwsCrossThread[0]);
    EXPECT_EQ(data[1], gwsCrossThread[1]);
    EXPECT_EQ(data[2], gwsCrossThread[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenInlineDataWhenSettingDataInInlineOffsetThenDataSetInInline) {
    constexpr L0::MCL::CrossThreadDataOffset inlineSize = 64;

    this->crossThreadDataSize = 256;
    this->inlineSize = inlineSize;
    this->offsets.globalWorkSize = 32;
    createMutableIndirectOffset();

    std::array<uint32_t, 3> data = {0xF0, 0x0F, 0x7F};

    this->indirectData->setGlobalWorkSize(data);

    auto gwsInline = reinterpret_cast<uint32_t *>(ptrOffset(this->inlineData.get(), this->offsets.globalWorkSize));
    EXPECT_EQ(data[0], gwsInline[0]);
    EXPECT_EQ(data[1], gwsInline[1]);
    EXPECT_EQ(data[2], gwsInline[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenInlineDataWhenSettingDataInCrossThreadOffsetThenDataSetInCrossThreadWithShiftedOffsetByInlineSize) {
    constexpr L0::MCL::CrossThreadDataOffset inlineSize = 64;

    this->crossThreadDataSize = 256;
    this->inlineSize = inlineSize;
    this->offsets.globalWorkSize = 72;
    createMutableIndirectOffset();

    std::array<uint32_t, 3> data = {0xF0, 0x0F, 0x7F};

    this->indirectData->setGlobalWorkSize(data);

    auto gwsCrossThread = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.globalWorkSize - inlineSize));
    EXPECT_EQ(data[0], gwsCrossThread[0]);
    EXPECT_EQ(data[1], gwsCrossThread[1]);
    EXPECT_EQ(data[2], gwsCrossThread[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableIndirectDataTest,
            givenInlineDataWhenSettingDataPartiallyInCrossThreadAndInlineOffsetThenDataSetInBothInlineAndCrossThreadStart) {
    constexpr L0::MCL::CrossThreadDataOffset inlineSize = 64;

    this->crossThreadDataSize = 256;
    this->inlineSize = inlineSize;
    this->offsets.globalWorkSize = inlineSize - sizeof(uint32_t);
    createMutableIndirectOffset();

    std::array<uint32_t, 3> data = {0xF0, 0x0F, 0x7F};

    this->indirectData->setGlobalWorkSize(data);

    auto gwsInline = reinterpret_cast<uint32_t *>(ptrOffset(this->inlineData.get(), this->offsets.globalWorkSize));
    auto gwsCrossThread = reinterpret_cast<uint32_t *>(this->crossThreadData.get());
    EXPECT_EQ(data[0], gwsInline[0]);
    EXPECT_EQ(data[1], gwsCrossThread[0]);
    EXPECT_EQ(data[2], gwsCrossThread[1]);
}

} // namespace ult
} // namespace L0
