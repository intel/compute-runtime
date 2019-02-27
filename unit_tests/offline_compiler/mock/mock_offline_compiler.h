/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "offline_compiler/offline_compiler.h"

#include <string>

namespace OCLRT {

class MockOfflineCompiler : public OfflineCompiler {
  public:
    using OfflineCompiler::deviceName;
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
    }

    int initialize(size_t numArgs, const char *const *argv) {
        return OfflineCompiler::initialize(numArgs, argv);
    }

    int parseCommandLine(size_t numArgs, const char *const *argv) {
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

    char *getElfBinary() {
        return elfBinary.data();
    }

    size_t getElfBinarySize() {
        return elfBinarySize;
    }

    char *getGenBinary() {
        return genBinary;
    }

    size_t getGenBinarySize() {
        return genBinarySize;
    }
};
} // namespace OCLRT
