/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "offline_compiler/offline_compiler.h"
#include <string>

namespace OCLRT {

class MockOfflineCompiler : public OfflineCompiler {
  public:
    using OfflineCompiler::generateFilePathForIr;
    using OfflineCompiler::generateOptsSuffix;
    using OfflineCompiler::inputFileLlvm;
    using OfflineCompiler::isSpirV;
    using OfflineCompiler::options;
    using OfflineCompiler::outputDirectory;
    using OfflineCompiler::outputFile;
    using OfflineCompiler::useLlvmText;
    using OfflineCompiler::useOptionsSuffix;

    MockOfflineCompiler() : OfflineCompiler() {
    }

    int initialize(uint32_t numArgs, const char **argv) {
        return OfflineCompiler::initialize(numArgs, argv);
    }

    int parseCommandLine(uint32_t numArgs, const char **argv) {
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
        return elfBinary;
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
