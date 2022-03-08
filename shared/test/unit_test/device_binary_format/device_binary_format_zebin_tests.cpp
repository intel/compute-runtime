/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

TEST(IsDeviceBinaryFormatZebin, GivenValidExecutableTypeBinaryThenReturnTrue) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U)));
}

TEST(IsDeviceBinaryFormatZebin, GivenValidRelocatableTypeBinaryThenReturnTrue) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_REL;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U)));
}

TEST(IsDeviceBinaryFormatZebin, GivenInvalidBinaryThenReturnFalse) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> someElf;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&someElf, 1U)));
}

TEST(UnpackSingleDeviceBinaryZebin, WhenInvalidElfThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Invalid or missing ELF header", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenUnhandledElfTypeThenUnpackingFails) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> someElf;
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&someElf, 1U), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Unhandled elf type", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenValidBinaryAndMatchedWithRequestedTargetDeviceThenReturnSelf) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_SKYLAKE);
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_GEN9_CORE);
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetDevice.productFamily = IGFX_UNKNOWN;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(zebin.machine);
    targetFlags.machineEntryUsesGfxCoreInsteadOfProductFamily = true;
    zebin.flags = targetFlags.packed;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenValidBinaryForDifferentDeviceThenUnpackingFails) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_SKYLAKE);
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine + 1);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8U;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());

    zebin.machine = static_cast<decltype(zebin.machine)>(IGFX_GEN9_CORE);
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetDevice.productFamily = IGFX_UNKNOWN;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(zebin.machine + 1);
    targetFlags.machineEntryUsesGfxCoreInsteadOfProductFamily = true;
    zebin.flags = targetFlags.packed;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenValidBinaryWithUnsupportedPointerSizeThenUnpackingFails) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 4U;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenNotRequestedThenDontValidateDeviceRevision) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8U;

    std::string unpackErrors;
    std::string unpackWarnings;
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetFlags.validateRevisionId = false;
    targetFlags.minHwRevisionId = 5;
    targetFlags.maxHwRevisionId = 7;
    zebin.flags = targetFlags.packed;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    targetDevice.stepping = 8U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenRequestedThenValidateRevision) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    zebin.machine = IGFX_SKYLAKE;
    NEO::TargetDevice targetDevice;
    targetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin.machine);
    targetDevice.stepping = 0U;
    targetDevice.maxPointerSizeInBytes = 8U;

    std::string unpackErrors;
    std::string unpackWarnings;
    NEO::Elf::ZebinTargetFlags targetFlags;
    targetFlags.validateRevisionId = true;
    targetFlags.minHwRevisionId = 5;
    targetFlags.maxHwRevisionId = 7;
    zebin.flags = targetFlags.packed;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());

    targetDevice.stepping = 8U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());

    unpackErrors.clear();
    targetDevice.stepping = 5U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    targetDevice.stepping = 6U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    targetDevice.stepping = 7U;
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U), "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(&zebin), unpackResult.deviceBinary.begin());
    EXPECT_EQ(sizeof(zebin), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryZebin, WhenMachineIsIntelGTAndIntelGTNoteSectionIsValidThenReturnSelf) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.elfHeader->type = NEO::Elf::ET_REL;
    zebin.elfHeader->machine = NEO::Elf::ELF_MACHINE::EM_INTELGT;

    NEO::TargetDevice targetDevice;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.productFamily = IGFX_SKYLAKE;
    targetDevice.coreFamily = IGFX_GEN9_CORE;
    targetDevice.stepping = 6;

    NEO::Elf::IntelGTNote notes[3];
    for (int i = 0; i < 3; ++i) {
        notes[i].nameSize = 8;
        notes[i].descSize = 4;
        strcpy_s(notes[i].ownerName, notes[i].nameSize, NEO::Elf::IntelGtNoteOwnerName.str().c_str());
    }

    notes[0].type = NEO::Elf::IntelGTSectionType::ProductFamily;
    notes[0].desc = targetDevice.productFamily;

    notes[1].type = NEO::Elf::IntelGTSectionType::GfxCore;
    notes[1].desc = targetDevice.coreFamily;

    NEO::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = targetDevice.stepping - 1;
    targetMetadata.maxHwRevisionId = targetDevice.stepping + 1;
    notes[2].type = NEO::Elf::IntelGTSectionType::TargetMetadata;
    notes[2].desc = targetMetadata.packed;

    zebin.appendSection(NEO::Elf::SHT_NOTE, NEO::Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(notes, 3));

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(zebin.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Zebin, unpackResult.format);
    EXPECT_EQ(targetDevice.productFamily, unpackResult.targetDevice.productFamily);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(zebin.storage.data(), unpackResult.deviceBinary.begin());
    EXPECT_EQ(zebin.storage.size(), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}
