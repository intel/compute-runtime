/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_compiler.h"

#include <map>
#include <optional>
#include <string>

class MockOclocArgHelper;

namespace NEO {
class MockOclocFclFacade;
class MockOclocIgcFacade;

class MockOfflineCompiler : public OfflineCompiler {
  public:
    using OfflineCompiler::addressingMode;
    using OfflineCompiler::allowCaching;
    using OfflineCompiler::appendExtraInternalOptions;
    using OfflineCompiler::argHelper;
    using OfflineCompiler::binaryOutputFile;
    using OfflineCompiler::cache;
    using OfflineCompiler::compilerProductHelper;
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
    using OfflineCompiler::initialize;
    using OfflineCompiler::inputCodeType;
    using OfflineCompiler::inputFile;
    using OfflineCompiler::inputFileLlvm;
    using OfflineCompiler::inputFileSpirV;
    using OfflineCompiler::intermediateRepresentation;
    using OfflineCompiler::internalOptions;
    using OfflineCompiler::irBinary;
    using OfflineCompiler::irBinarySize;
    using OfflineCompiler::irHash;
    using OfflineCompiler::loadSpecializationConstants;
    using OfflineCompiler::onlySpirV;
    using OfflineCompiler::options;
    using OfflineCompiler::outputDirectory;
    using OfflineCompiler::outputFile;
    using OfflineCompiler::outputNoSuffix;
    using OfflineCompiler::parseCommandLine;
    using OfflineCompiler::parseCommandLineExt;
    using OfflineCompiler::parseDebugSettings;
    using OfflineCompiler::perDeviceOptions;
    using OfflineCompiler::releaseHelper;
    using OfflineCompiler::revisionId;
    using OfflineCompiler::setStatelessToStatefulBufferOffsetFlag;
    using OfflineCompiler::sourceCode;
    using OfflineCompiler::specConstants;
    using OfflineCompiler::specConstantsFile;
    using OfflineCompiler::storeBinary;
    using OfflineCompiler::updateBuildLog;
    using OfflineCompiler::useGenFile;
    using OfflineCompiler::useIgcAsFcl;
    using OfflineCompiler::useOptionsSuffix;

    MockOfflineCompiler();

    ~MockOfflineCompiler() override;

    int initialize(size_t numArgs, const std::vector<std::string> &argv);

    void storeGenBinary(const void *pSrc, const size_t srcSize);

    int build() override;

    int buildToIrBinary() override;

    int buildSourceCode() override;

    bool generateElfBinary() override;

    void writeOutAllFiles() override;

    void clearLog();

    int createDir(const std::string &path) override;
    void createTempSourceFileForDebug() override;

    std::map<std::string, std::string> filesMap{};
    int buildToIrBinaryStatus = 0;
    bool overrideBuildToIrBinaryStatus = false;
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
    int createTempSourceFileForDebugCalled{0};
};

} // namespace NEO
