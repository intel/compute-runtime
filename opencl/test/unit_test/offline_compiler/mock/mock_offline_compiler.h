/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/compiler_interface/default_cache_config.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_fcl_facade.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_igc_facade.h"

#include <optional>
#include <string>

namespace NEO {

class MockOfflineCompiler : public OfflineCompiler {
  public:
    using OfflineCompiler::allowCaching;
    using OfflineCompiler::appendExtraInternalOptions;
    using OfflineCompiler::argHelper;
    using OfflineCompiler::cache;
    using OfflineCompiler::dbgHash;
    using OfflineCompiler::debugDataBinary;
    using OfflineCompiler::debugDataBinarySize;
    using OfflineCompiler::deviceConfig;
    using OfflineCompiler::deviceName;
    using OfflineCompiler::dumpFiles;
    using OfflineCompiler::elfBinary;
    using OfflineCompiler::elfBinarySize;
    using OfflineCompiler::elfHash;
    using OfflineCompiler::excludeIr;
    using OfflineCompiler::fclFacade;
    using OfflineCompiler::forceStatelessToStatefulOptimization;
    using OfflineCompiler::genBinary;
    using OfflineCompiler::genBinarySize;
    using OfflineCompiler::generateFilePathForIr;
    using OfflineCompiler::generateOptsSuffix;
    using OfflineCompiler::genHash;
    using OfflineCompiler::getStringWithinDelimiters;
    using OfflineCompiler::hwInfo;
    using OfflineCompiler::hwInfoConfig;
    using OfflineCompiler::igcFacade;
    using OfflineCompiler::initHardwareInfo;
    using OfflineCompiler::initHardwareInfoForProductConfig;
    using OfflineCompiler::inputFile;
    using OfflineCompiler::inputFileLlvm;
    using OfflineCompiler::inputFileSpirV;
    using OfflineCompiler::internalOptions;
    using OfflineCompiler::irBinary;
    using OfflineCompiler::irBinarySize;
    using OfflineCompiler::irHash;
    using OfflineCompiler::isSpirV;
    using OfflineCompiler::options;
    using OfflineCompiler::outputDirectory;
    using OfflineCompiler::outputFile;
    using OfflineCompiler::outputNoSuffix;
    using OfflineCompiler::parseCommandLine;
    using OfflineCompiler::parseDebugSettings;
    using OfflineCompiler::revisionId;
    using OfflineCompiler::setStatelessToStatefulBufferOffsetFlag;
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

        auto uniqueFclFacadeMock = std::make_unique<MockOclocFclFacade>(argHelper);
        mockFclFacade = uniqueFclFacadeMock.get();
        fclFacade = std::move(uniqueFclFacadeMock);

        auto uniqueIgcFacadeMock = std::make_unique<MockOclocIgcFacade>(argHelper);
        mockIgcFacade = uniqueIgcFacadeMock.get();
        igcFacade = std::move(uniqueIgcFacadeMock);
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

    int buildIrBinary() override {
        if (overrideBuildIrBinaryStatus) {
            return buildIrBinaryStatus;
        }
        return OfflineCompiler::buildIrBinary();
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

    void createDir(const std::string &path) override {
        if (interceptCreatedDirs) {
            createdDirs.push_back(path);
        } else {
            OfflineCompiler::createDir(path);
        }
    }

    std::map<std::string, std::string> filesMap{};
    int buildIrBinaryStatus = 0;
    bool overrideBuildIrBinaryStatus = false;
    int buildSourceCodeStatus = 0;
    bool overrideBuildSourceCodeStatus = false;
    uint32_t generateElfBinaryCalled = 0u;
    uint32_t writeOutAllFilesCalled = 0u;
    std::unique_ptr<MockOclocArgHelper> uniqueHelper;
    MockOclocIgcFacade *mockIgcFacade = nullptr;
    MockOclocFclFacade *mockFclFacade = nullptr;
    int buildCalledCount{0};
    std::optional<int> buildReturnValue{};
    bool interceptCreatedDirs{false};
    std::vector<std::string> createdDirs{};
};

} // namespace NEO
