/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/program/program_info.h"

#include "test.h"

TEST(IsDeviceBinaryFormatZebin, GivenValidBinaryReturnTrue) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> zebin;
    zebin.type = NEO::Elf::ET_ZEBIN_EXE;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(&zebin, 1U)));
}

TEST(IsDeviceBinaryFormatZebin, GivenInvalidBinaryReturnFalse) {
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
