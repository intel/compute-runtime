/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO::Elf;

TEST(ElfFormat, WhenAccessingFieldsFromElfSymbolEntryThenAdequateValueIsSetOrReturned) {
    // fields info and other have same size in both 32bit and 64bit elf
    ElfSymbolEntry<EI_CLASS_64> elfSym{0};

    uint8_t binding = 1U;
    elfSym.info = 0U;
    elfSym.info = (elfSym.info & 0xFU) | (binding << 4U);
    EXPECT_EQ(binding, elfSym.getBinding());

    elfSym.info = 0U;
    elfSym.setBinding(binding);
    EXPECT_EQ(binding, elfSym.info >> 4);

    uint8_t type = 2U;
    elfSym.info = 0U;
    elfSym.info = (elfSym.info & (~0xFU)) | type;
    EXPECT_EQ(type, elfSym.getType());

    elfSym.info = 0U;
    elfSym.setType(type);
    EXPECT_EQ(type, elfSym.info & 0xF);

    uint8_t visibility = 3U;
    elfSym.other = 0U;
    elfSym.other = (elfSym.other & (~0x3)) | visibility;
    EXPECT_EQ(visibility, elfSym.getVisibility());

    elfSym.other = 0U;
    elfSym.setVisibility(visibility);
    EXPECT_EQ(visibility, elfSym.other & 0x3);
}

TEST(ElfFormat, Elf32SymbolsAreReadCorrectly) {
    uint8_t elfSymbolData32b[0x10] = {0};
    auto &name = *reinterpret_cast<uint32_t *>(elfSymbolData32b);
    auto &info = *reinterpret_cast<uint8_t *>(elfSymbolData32b + 4);
    auto &other = *reinterpret_cast<uint8_t *>(elfSymbolData32b + 5);
    auto &shndx = *reinterpret_cast<uint16_t *>(elfSymbolData32b + 6);
    auto &value = *reinterpret_cast<uint32_t *>(elfSymbolData32b + 8);
    auto &size = *reinterpret_cast<uint32_t *>(elfSymbolData32b + 0xC);

    uint8_t type = 1U;
    uint8_t bind = 2U;
    uint8_t visibility = 3U;
    name = std::numeric_limits<uint32_t>::max();
    info = (bind << 4) | (type & 0xF);
    other = (visibility & 0x3);
    shndx = std::numeric_limits<uint16_t>::max();
    value = std::numeric_limits<uint32_t>::max();
    size = std::numeric_limits<uint32_t>::max();

    auto elfSymbolEntry = reinterpret_cast<ElfSymbolEntry<EI_CLASS_32> *>(elfSymbolData32b);
    EXPECT_EQ(name, elfSymbolEntry->name);
    EXPECT_EQ(info, elfSymbolEntry->info);
    EXPECT_EQ(type, elfSymbolEntry->getType());
    EXPECT_EQ(bind, elfSymbolEntry->getBinding());
    EXPECT_EQ(other, elfSymbolEntry->other);
    EXPECT_EQ(visibility, elfSymbolEntry->getVisibility());
    EXPECT_EQ(shndx, elfSymbolEntry->shndx);
    EXPECT_EQ(value, elfSymbolEntry->value);
    EXPECT_EQ(size, elfSymbolEntry->size);
}

TEST(ElfFormat, Elf64SymbolsAreReadCorrectly) {
    uint8_t elfSymbolData64b[0x18] = {0};
    auto &name = *reinterpret_cast<uint32_t *>(elfSymbolData64b);
    auto &info = *reinterpret_cast<uint8_t *>(elfSymbolData64b + 4);
    auto &other = *reinterpret_cast<uint8_t *>(elfSymbolData64b + 5);
    auto &shndx = *reinterpret_cast<uint16_t *>(elfSymbolData64b + 6);
    auto &value = *reinterpret_cast<uint64_t *>(elfSymbolData64b + 8);
    auto &size = *reinterpret_cast<uint64_t *>(elfSymbolData64b + 0x10);

    uint8_t type = 1U;
    uint8_t bind = 2U;
    uint8_t visibility = 3U;
    name = std::numeric_limits<uint32_t>::max();
    info = (bind << 4) | (type & 0xF);
    other = (visibility & 0x3);
    shndx = std::numeric_limits<uint16_t>::max();
    value = std::numeric_limits<uint64_t>::max();
    size = std::numeric_limits<uint64_t>::max();

    auto elfSymbolEntry = reinterpret_cast<ElfSymbolEntry<EI_CLASS_64> *>(elfSymbolData64b);
    EXPECT_EQ(name, elfSymbolEntry->name);
    EXPECT_EQ(info, elfSymbolEntry->info);
    EXPECT_EQ(type, elfSymbolEntry->getType());
    EXPECT_EQ(bind, elfSymbolEntry->getBinding());
    EXPECT_EQ(other, elfSymbolEntry->other);
    EXPECT_EQ(visibility, elfSymbolEntry->getVisibility());
    EXPECT_EQ(shndx, elfSymbolEntry->shndx);
    EXPECT_EQ(value, elfSymbolEntry->value);
    EXPECT_EQ(size, elfSymbolEntry->size);
}

TEST(ElfFormat, WhenAccessingInfoFieldFromElf32RelThenAdequateValueIsSetOrReturned) {
    ElfRel<EI_CLASS_32> elfRel{0};

    uint8_t relocType = 0x8;
    elfRel.info = relocType;

    EXPECT_EQ(relocType, elfRel.getRelocationType());

    elfRel.info = 0U;
    elfRel.setRelocationType(relocType);
    EXPECT_EQ(relocType, static_cast<uint8_t>(elfRel.info));

    uint32_t symTableIdx = 0x10;
    elfRel.info = symTableIdx << 8;
    EXPECT_EQ(symTableIdx, elfRel.getSymbolTableIndex());

    elfRel.info = 0U;
    elfRel.setSymbolTableIndex(symTableIdx);
    EXPECT_EQ(symTableIdx, (elfRel.info >> 8));
}

TEST(ElfFormat, WhenAccessingInfoFieldFromElf64RelThenAdequateValueIsSetOrReturned) {
    ElfRel<EI_CLASS_64> elfRel{0};

    uint64_t relocType = 0x1000;
    elfRel.info = relocType;
    EXPECT_EQ(relocType, elfRel.getRelocationType());

    elfRel.info = 0U;
    elfRel.setRelocationType(relocType);
    EXPECT_EQ(relocType, static_cast<uint32_t>(elfRel.info));

    uint32_t symTableIdx = 0x2000;
    elfRel.info = static_cast<uint64_t>(symTableIdx) << 32;
    EXPECT_EQ(symTableIdx, elfRel.getSymbolTableIndex());

    elfRel.info = 0U;
    elfRel.setSymbolTableIndex(symTableIdx);
    EXPECT_EQ(symTableIdx, (elfRel.info >> 32));
}

TEST(ElfFormat, Elf32RelAreReadCorrectly) {
    uint8_t elfRel32Data[0x8] = {0};
    auto &offset = *reinterpret_cast<uint32_t *>(elfRel32Data);
    auto &info = *reinterpret_cast<uint32_t *>(elfRel32Data + 4);

    uint32_t relocType = 0x8;
    uint32_t symTabIdx = 0x100;

    offset = 0x1000;
    info = static_cast<uint8_t>(relocType) | (symTabIdx << 8);

    auto elfRel = reinterpret_cast<ElfRel<EI_CLASS_32> *>(elfRel32Data);
    EXPECT_EQ(offset, elfRel->offset);
    EXPECT_EQ(info, elfRel->info);
    EXPECT_EQ(relocType, elfRel->getRelocationType());
    EXPECT_EQ(symTabIdx, elfRel->getSymbolTableIndex());
}

TEST(ElfFormat, Elf32RelaAreReadCorrectly) {
    uint8_t elfRela32Data[0xC] = {0};
    auto &offset = *reinterpret_cast<uint32_t *>(elfRela32Data);
    auto &info = *reinterpret_cast<uint32_t *>(elfRela32Data + 4);
    auto &addend = *reinterpret_cast<int32_t *>(elfRela32Data + 8);

    uint32_t relocType = 0x8;
    uint32_t symTabIdx = 0x100;

    offset = 0x1000;
    info = static_cast<uint8_t>(relocType) | (symTabIdx << 8);
    addend = 0x2000;

    auto elfRela = reinterpret_cast<ElfRela<EI_CLASS_32> *>(elfRela32Data);
    EXPECT_EQ(offset, elfRela->offset);
    EXPECT_EQ(info, elfRela->info);
    EXPECT_EQ(relocType, elfRela->getRelocationType());
    EXPECT_EQ(symTabIdx, elfRela->getSymbolTableIndex());
    EXPECT_EQ(addend, elfRela->addend);
}

TEST(ElfFormat, Elf64RelAreReadCorrectly) {
    uint8_t elfRel64Data[0x10] = {0};
    auto &offset = *reinterpret_cast<uint64_t *>(elfRel64Data);
    auto &info = *reinterpret_cast<uint64_t *>(elfRel64Data + 0x8);

    uint64_t relocType = 0x800;
    uint64_t symTabIdx = 0x100;

    offset = 0x1000;
    info = static_cast<uint32_t>(relocType) + (symTabIdx << 32);

    auto elfRel = reinterpret_cast<ElfRel<EI_CLASS_64> *>(elfRel64Data);
    EXPECT_EQ(offset, elfRel->offset);
    EXPECT_EQ(info, elfRel->info);
    EXPECT_EQ(relocType, elfRel->getRelocationType());
    EXPECT_EQ(symTabIdx, elfRel->getSymbolTableIndex());
}

TEST(ElfFormat, Elf64RelaAreReadCorrectly) {
    uint8_t elfRela64Data[0x18] = {0};
    auto &offset = *reinterpret_cast<uint64_t *>(elfRela64Data);
    auto &info = *reinterpret_cast<uint64_t *>(elfRela64Data + 0x8);
    auto &addend = *reinterpret_cast<int64_t *>(elfRela64Data + 0x10);

    uint64_t relocType = 0x800;
    uint64_t symTabIdx = 0x100;

    offset = 0x1000;
    info = static_cast<uint32_t>(relocType) + (symTabIdx << 32);
    addend = 0x2000;

    auto elfRela = reinterpret_cast<ElfRela<EI_CLASS_64> *>(elfRela64Data);
    EXPECT_EQ(offset, elfRela->offset);
    EXPECT_EQ(info, elfRela->info);
    EXPECT_EQ(relocType, elfRela->getRelocationType());
    EXPECT_EQ(symTabIdx, elfRela->getSymbolTableIndex());
    EXPECT_EQ(addend, elfRela->addend);
}
