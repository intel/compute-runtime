/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include "environment.h"
#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"

#include <memory>

extern Environment *gEnvironment;

namespace NEO {

class OclocTest : public ::testing::Test {
    void SetUp() override {
        std::string spvFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".spv";
        std::string binFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".bin";
        std::string dbgFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".dbg";

        constexpr unsigned char mockByteArray[] = {0x01, 0x02, 0x03, 0x04};
        std::string_view byteArrayView(reinterpret_cast<const char *>(mockByteArray), sizeof(mockByteArray));

        writeDataToFile(spvFile.c_str(), byteArrayView);
        writeDataToFile(binFile.c_str(), byteArrayView);
        writeDataToFile(dbgFile.c_str(), byteArrayView);
    }

  protected:
    const std::string clCopybufferFilename = "some_kernel.cl";
    std::string copyKernelSources = "example_kernel(){}";
};

class OclocEnabledAcronyms : public OclocTest {
  public:
    std::vector<DeviceAotInfo> enabledProducts{};
    std::vector<ConstStringRef> enabledProductsAcronyms{};
    std::vector<ConstStringRef> enabledFamiliesAcronyms{};
    std::vector<ConstStringRef> enabledReleasesAcronyms{};
};

class OclocFatBinaryProductAcronymsTests : public OclocEnabledAcronyms {
  public:
    OclocFatBinaryProductAcronymsTests() {
        mockArgHelperFilesMap[clCopybufferFilename] = copyKernelSources;
        oclocArgHelperWithoutInput = std::make_unique<MockOclocArgHelper>(mockArgHelperFilesMap);
        oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(true);

        enabledProducts = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
        enabledProductsAcronyms = oclocArgHelperWithoutInput->productConfigHelper->getRepresentativeProductAcronyms();
        enabledFamiliesAcronyms = oclocArgHelperWithoutInput->productConfigHelper->getFamiliesAcronyms();
        enabledReleasesAcronyms = oclocArgHelperWithoutInput->productConfigHelper->getReleasesAcronyms();
    }

  protected:
    std::unique_ptr<MockOclocArgHelper> oclocArgHelperWithoutInput;
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
};

class OclocFatBinaryTest : public OclocEnabledAcronyms {
  public:
    OclocFatBinaryTest() {
        mockArgHelperFilesMap[spirvFilename] = spirvFileContent;
        mockArgHelper.interceptOutput = true;

        enabledProducts = mockArgHelper.productConfigHelper->getDeviceAotInfo();
        enabledProductsAcronyms = mockArgHelper.productConfigHelper->getRepresentativeProductAcronyms();
        enabledFamiliesAcronyms = mockArgHelper.productConfigHelper->getFamiliesAcronyms();
        enabledReleasesAcronyms = mockArgHelper.productConfigHelper->getReleasesAcronyms();
    }

  protected:
    constexpr static ConstStringRef archiveGenericIrName{"generic_ir"};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};

    std::string outputArchiveName{"output_archive"};
    std::string spirvFilename{"input_file.spv"};
    std::string spirvFileContent{"\x07\x23\x02\x03"};
};

struct OclocFatbinaryPerProductTests : public ::testing::TestWithParam<std::tuple<std::string, PRODUCT_FAMILY>> {
    void SetUp() override {
        std::tie(release, productFamily) = GetParam();
        argHelper = std::make_unique<OclocArgHelper>();
        argHelper->getPrinterRef().setSuppressMessages(true);
    }

    std::string release;
    PRODUCT_FAMILY productFamily;
    std::unique_ptr<OclocArgHelper> argHelper;
};

} // namespace NEO