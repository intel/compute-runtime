/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/source/helpers/product_config_helper.h"

#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"

#include <memory>

namespace NEO {
class OclocEnabledAcronyms : public ::testing::Test {
  public:
    std::vector<DeviceAotInfo> enabledProducts{};
    std::vector<ConstStringRef> enabledProductsAcronyms{};
    std::vector<ConstStringRef> enabledFamiliesAcronyms{};
    std::vector<ConstStringRef> enabledReleasesAcronyms{};
};

class OclocFatBinaryProductAcronymsTests : public OclocEnabledAcronyms {
  public:
    OclocFatBinaryProductAcronymsTests() {
        oclocArgHelperWithoutInput = std::make_unique<OclocArgHelper>();
        oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(true);

        enabledProducts = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
        enabledProductsAcronyms = oclocArgHelperWithoutInput->productConfigHelper->getRepresentativeProductAcronyms();
        enabledFamiliesAcronyms = oclocArgHelperWithoutInput->productConfigHelper->getFamiliesAcronyms();
        enabledReleasesAcronyms = oclocArgHelperWithoutInput->productConfigHelper->getReleasesAcronyms();
    }

    std::unique_ptr<OclocArgHelper> oclocArgHelperWithoutInput;
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
