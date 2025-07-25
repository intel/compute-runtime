/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/multi_command.h"
#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "environment.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

namespace NEO {
class MultiCommand;
class OfflineCompiler;
} // namespace NEO

extern Environment *gEnvironment;

namespace NEO {

class OfflineCompilerTests : public ::testing::Test {
  public:
    OfflineCompiler *pOfflineCompiler = nullptr;
    int retVal = OCLOC_SUCCESS;
    std::unique_ptr<MockOclocArgHelper> oclocArgHelperWithoutInput = std::make_unique<MockOclocArgHelper>(filesMap);

  protected:
    void SetUp() override {
        std::string spvFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".spv";
        std::string binFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".bin";
        std::string dbgFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".dbg";

        constexpr unsigned char mockByteArray[] = {0x01, 0x02, 0x03, 0x04};
        std::string_view byteArrayView(reinterpret_cast<const char *>(mockByteArray), sizeof(mockByteArray));

        writeDataToFile(spvFile.c_str(), byteArrayView);
        writeDataToFile(binFile.c_str(), byteArrayView);
        writeDataToFile(dbgFile.c_str(), byteArrayView);

        filesMap[clCopybufferFilename] = OfflineCompilerTests::kernelSources;
        oclocArgHelperWithoutInput->setAllCallBase(false);
    }
    MockOclocArgHelper::FilesMap filesMap{};
    std::string kernelSources = "example_kernel(){}";
    const std::string clCopybufferFilename = "some_kernel.cl";
};

class MultiCommandTests : public ::testing::Test {
  public:
    void createFileWithArgs(const std::vector<std::string> &, int numOfBuild);
    void deleteFileWithArgs();
    void deleteOutFileList();
    MultiCommand *pMultiCommand = nullptr;
    std::string nameOfFileWithArgs;
    std::string outFileList;
    int retVal = OCLOC_SUCCESS;
    std::unique_ptr<MockOclocArgHelper> oclocArgHelperWithoutInput = std::make_unique<MockOclocArgHelper>(filesMap);

  protected:
    void SetUp() override {
        std::string spvFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".spv";
        std::string binFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".bin";
        std::string dbgFile = std::string("copybuffer") + "_" + gEnvironment->devicePrefix + ".dbg";

        constexpr unsigned char mockByteArray[] = {0x01, 0x02, 0x03, 0x04};
        std::string_view byteArrayView(reinterpret_cast<const char *>(mockByteArray), sizeof(mockByteArray));

        writeDataToFile(spvFile.c_str(), byteArrayView);
        writeDataToFile(binFile.c_str(), byteArrayView);
        writeDataToFile(dbgFile.c_str(), byteArrayView);

        filesMap[clCopybufferFilename] = kernelSources;
        oclocArgHelperWithoutInput->setAllCallBase(false);
    }

    MockOclocArgHelper::FilesMap filesMap{};
    const std::string clCopybufferFilename = "some_kernel.cl";
    std::string kernelSources = "example_kernel(){}";
};
} // namespace NEO