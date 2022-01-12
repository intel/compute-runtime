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

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

namespace NEO {

class OfflineCompilerTests : public ::testing::Test {
  public:
    OfflineCompiler *pOfflineCompiler = nullptr;
    int retVal = OclocErrorCode::SUCCESS;
    std::unique_ptr<OclocArgHelper> oclocArgHelperWithoutInput = std::make_unique<OclocArgHelper>();
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
    std::unique_ptr<OclocArgHelper> oclocArgHelperWithoutInput = std::make_unique<OclocArgHelper>();
};
} // namespace NEO
