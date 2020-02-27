/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/offline_compiler.h"

#include <string>

namespace NEO {

class MockOfflineCompiler : public OfflineCompiler {
  public:
    using OfflineCompiler::deviceName;
    using OfflineCompiler::elfBinary;
    using OfflineCompiler::fclDeviceCtx;
    using OfflineCompiler::generateFilePathForIr;
    using OfflineCompiler::generateOptsSuffix;
    using OfflineCompiler::igcDeviceCtx;
    using OfflineCompiler::inputFileLlvm;
    using OfflineCompiler::inputFileSpirV;
    using OfflineCompiler::isSpirV;
    using OfflineCompiler::options;
    using OfflineCompiler::outputDirectory;
    using OfflineCompiler::outputFile;
    using OfflineCompiler::sourceCode;
    using OfflineCompiler::useLlvmText;
    using OfflineCompiler::useOptionsSuffix;

    MockOfflineCompiler() : OfflineCompiler() {
        argHelper.reset(new OclocArgHelper(
            0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    }

    int initialize(size_t numArgs, const std::vector<std::string> &argv) {
        return OfflineCompiler::initialize(numArgs, argv, true);
    }

    int parseCommandLine(size_t numArgs, const std::vector<std::string> &argv) {
        return OfflineCompiler::parseCommandLine(numArgs, argv);
    }

    void parseDebugSettings() {
        return OfflineCompiler::parseDebugSettings();
    }

    std::string &getOptions() {
        return options;
    }

    std::string &getInternalOptions() {
        return internalOptions;
    }

    std::string getStringWithinDelimiters(const std::string &src) {
        return OfflineCompiler::getStringWithinDelimiters(src);
    }

    void updateBuildLog(const char *pErrorString, const size_t errorStringSize) {
        OfflineCompiler::updateBuildLog(pErrorString, errorStringSize);
    }

    int getHardwareInfo(const char *pDeviceName) {
        return OfflineCompiler::getHardwareInfo(pDeviceName);
    }

    void storeBinary(char *&pDst, size_t &dstSize, const void *pSrc, const size_t srcSize) {
        OfflineCompiler::storeBinary(pDst, dstSize, pSrc, srcSize);
    }

    void storeGenBinary(const void *pSrc, const size_t srcSize) {
        OfflineCompiler::storeBinary(genBinary, genBinarySize, pSrc, srcSize);
    }

    int buildSourceCode() {
        return OfflineCompiler::buildSourceCode();
    }

    bool generateElfBinary() {
        return OfflineCompiler::generateElfBinary();
    }

    char *getGenBinary() {
        return genBinary;
    }

    size_t getGenBinarySize() {
        return genBinarySize;
    }
};
} // namespace NEO
