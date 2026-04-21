/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "neo_aot_platforms.h"

using PvcUnpackSingleDeviceBinaryAr = ::testing::Test;

PVCTEST_F(PvcUnpackSingleDeviceBinaryAr, WhenFailedToUnpackMatchWithPvcProductConfigThenTryUnpackAnotherBestMatch) {
    ZebinTestData::ValidEmptyProgram zebinValid;
    ZebinTestData::ValidEmptyProgram zebinWrongMachine;

    zebinValid.elfHeader->machine = static_cast<uint16_t>(productFamily);
    zebinWrongMachine.elfHeader->machine = 0U;

    NEO::HardwareIpVersion aotConfig0{}, aotConfig1{};
    aotConfig0.value = AOT::PVC_XL_A0;
    aotConfig1.value = AOT::PVC_XL_A0P;

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig0);
    std::string anotherProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig1);

    NEO::Ar::ArEncoder encoder{true};
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredPointerSize = "64";

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebinValid.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, zebinValid.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + anotherProductConfig, zebinValid.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, zebinWrongMachine.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk.0", zebinValid.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(productFamily);
    target.aotConfig = aotConfig0;
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_FALSE(unpackWarnings.empty());
    EXPECT_STREQ("Couldn't find perfectly matched binary in AR, using best usable", unpackWarnings.c_str());

    EXPECT_FALSE(unpacked.deviceBinary.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    bool foundMatch = false;
    for (const auto &file : decodedAr.files) {
        if (file.fileData.begin() == unpacked.deviceBinary.begin()) {
            foundMatch = true;
            EXPECT_TRUE(file.fileName.startsWith((requiredPointerSize + "." + anotherProductConfig).c_str()));
            break;
        }
    }
    EXPECT_TRUE(foundMatch);
}
