/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"

#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"

#include <memory>

namespace NEO {

class OclocFatBinaryGetTargetConfigsForFatbinary : public ::testing::Test {
  public:
    OclocFatBinaryGetTargetConfigsForFatbinary() {
        oclocArgHelperWithoutInput = std::make_unique<OclocArgHelper>();
        oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{true};
    }
    std::unique_ptr<OclocArgHelper> oclocArgHelperWithoutInput;
};

class OclocFatBinaryTest : public ::testing::Test {
  public:
    OclocFatBinaryTest() {
        mockArgHelperFilesMap[spirvFilename] = spirvFileContent;
        mockArgHelper.interceptOutput = true;
    }

  protected:
    constexpr static ConstStringRef archiveGenericIrName{"generic_ir"};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};

    std::string outputArchiveName{"output_archive"};
    std::string spirvFilename{"input_file.spv"};
    std::string spirvFileContent{"\x07\x23\x02\x03"};
};

} // namespace NEO
