/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_compiler.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include <optional>
#include <string>

namespace NEO {

class MockOfflineCompiler : public OfflineCompiler {
  public:
    using OfflineCompiler::appendExtraInternalOptions;
    using OfflineCompiler::argHelper;
    using OfflineCompiler::deviceName;
    using OfflineCompiler::elfBinary;
    using OfflineCompiler::excludeIr;
    using OfflineCompiler::fclDeviceCtx;
    using OfflineCompiler::forceStatelessToStatefulOptimization;
    using OfflineCompiler::genBinary;
    using OfflineCompiler::genBinarySize;
    using OfflineCompiler::generateFilePathForIr;
    using OfflineCompiler::generateOptsSuffix;
    using OfflineCompiler::getStringWithinDelimiters;
    using OfflineCompiler::hwInfo;
    using OfflineCompiler::igcDeviceCtx;
    using OfflineCompiler::initHardwareInfo;
    using OfflineCompiler::inputFileLlvm;
    using OfflineCompiler::inputFileSpirV;
    using OfflineCompiler::internalOptions;
    using OfflineCompiler::irBinary;
    using OfflineCompiler::irBinarySize;
    using OfflineCompiler::isSpirV;
    using OfflineCompiler::options;
    using OfflineCompiler::outputDirectory;
    using OfflineCompiler::outputFile;
    using OfflineCompiler::parseCommandLine;
    using OfflineCompiler::parseDebugSettings;
    using OfflineCompiler::setStatelessToStatefullBufferOffsetFlag;
    using OfflineCompiler::sourceCode;
    using OfflineCompiler::storeBinary;
    using OfflineCompiler::updateBuildLog;
    using OfflineCompiler::useGenFile;
    using OfflineCompiler::useLlvmBc;
    using OfflineCompiler::useLlvmText;
    using OfflineCompiler::useOptionsSuffix;

    MockOfflineCompiler() : OfflineCompiler() {
        uniqueHelper = std::make_unique<MockOclocArgHelper>(filesMap);
        uniqueHelper->setAllCallBase(true);
        argHelper = uniqueHelper.get();
    }
    ~MockOfflineCompiler() override = default;

    int initialize(size_t numArgs, const std::vector<std::string> &argv) {
        return OfflineCompiler::initialize(numArgs, argv, true);
    }

    void storeGenBinary(const void *pSrc, const size_t srcSize) {
        OfflineCompiler::storeBinary(genBinary, genBinarySize, pSrc, srcSize);
    }

    int build() override {
        ++buildCalledCount;

        if (buildReturnValue.has_value()) {
            return *buildReturnValue;
        }

        return OfflineCompiler::build();
    }

    int buildSourceCode() override {
        if (overrideBuildSourceCodeStatus) {
            return buildSourceCodeStatus;
        }
        return OfflineCompiler::buildSourceCode();
    }

    bool generateElfBinary() override {
        generateElfBinaryCalled++;
        return OfflineCompiler::generateElfBinary();
    }

    void writeOutAllFiles() override {
        writeOutAllFilesCalled++;
        OfflineCompiler::writeOutAllFiles();
    }

    void clearLog() {
        uniqueHelper = std::make_unique<MockOclocArgHelper>(filesMap);
        uniqueHelper->setAllCallBase(true);
        argHelper = uniqueHelper.get();
    }

    std::map<std::string, std::string> filesMap{};
    int buildSourceCodeStatus = 0;
    bool overrideBuildSourceCodeStatus = false;
    uint32_t generateElfBinaryCalled = 0u;
    uint32_t writeOutAllFilesCalled = 0u;
    std::unique_ptr<MockOclocArgHelper> uniqueHelper;
    int buildCalledCount{0};
    std::optional<int> buildReturnValue{};
};

} // namespace NEO
