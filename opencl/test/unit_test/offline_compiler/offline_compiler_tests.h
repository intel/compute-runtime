/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/multi_command.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/offline_compiler.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

namespace NEO {

class OfflineCompilerTests : public ::testing::Test {
  public:
    OfflineCompiler *pOfflineCompiler = nullptr;
    int retVal = OclocErrorCode::SUCCESS;
    std::map<std::string, std::string> filesMap;
    std::unique_ptr<MockOclocArgHelper> oclocArgHelperWithoutInput = std::make_unique<MockOclocArgHelper>(filesMap);

  protected:
    void SetUp() override {
        oclocArgHelperWithoutInput->setAllCallBase(true);
    }
};

class MultiCommandTests : public ::testing::Test {
  public:
    void createFileWithArgs(const std::vector<std::string> &, int numOfBuild);
    void deleteFileWithArgs();
    void deleteOutFileList();
    MultiCommand *pMultiCommand = nullptr;
    std::string nameOfFileWithArgs;
    std::string outFileList;
    int retVal = OclocErrorCode::SUCCESS;
    std::map<std::string, std::string> filesMap;
    std::unique_ptr<MockOclocArgHelper> oclocArgHelperWithoutInput = std::make_unique<MockOclocArgHelper>(filesMap);

  protected:
    void SetUp() override {
        oclocArgHelperWithoutInput->setAllCallBase(true);
    }
};
} // namespace NEO
