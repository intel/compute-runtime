/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_compile.h"

#include "ocloc_api.h"

#include <cstring>

namespace LevelZeroBlackBoxTests {

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, std::string &outCompilerLog) {
    std::vector<uint8_t> ret;

    const char *mainFileName = "main.cl";
    const char *argv[] = {"ocloc", "-q", "-spv_only", "-file", mainFileName, "", ""};
    uint32_t numArgs = sizeof(argv) / sizeof(argv[0]) - 2;
    if (options.size() > 0) {
        argv[5] = "-options";
        argv[6] = options.c_str();
        numArgs += 2;
    }
    const unsigned char *sources[] = {reinterpret_cast<const unsigned char *>(src.c_str())};
    size_t sourcesLengths[] = {src.size() + 1};
    const char *sourcesNames[] = {mainFileName};
    unsigned int numOutputs = 0U;
    unsigned char **outputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    int result = oclocInvoke(numArgs, argv,
                             1, sources, sourcesLengths, sourcesNames,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputs, &ouputLengths, &outputNames);

    unsigned char *spirV = nullptr;
    size_t spirVlen = 0;
    const char *log = nullptr;
    size_t logLen = 0;
    for (unsigned int i = 0; i < numOutputs; ++i) {
        std::string spvExtension = ".spv";
        std::string logFileName = "stdout.log";
        auto nameLen = std::strlen(outputNames[i]);
        if ((nameLen > spvExtension.size()) && (std::strstr(&outputNames[i][nameLen - spvExtension.size()], spvExtension.c_str()) != nullptr)) {
            spirV = outputs[i];
            spirVlen = ouputLengths[i];
        } else if ((nameLen >= logFileName.size()) && (std::strstr(outputNames[i], logFileName.c_str()) != nullptr)) {
            log = reinterpret_cast<const char *>(outputs[i]);
            logLen = ouputLengths[i];
            break;
        }
    }

    if ((result != 0) && (logLen == 0)) {
        outCompilerLog = "Unknown error, ocloc returned : " + std::to_string(result) + "\n";
        return ret;
    }

    if (logLen != 0) {
        outCompilerLog = std::string(log, logLen).c_str();
    }

    ret.assign(spirV, spirV + spirVlen);
    oclocFreeOutput(&numOutputs, &outputs, &ouputLengths, &outputNames);
    return ret;
}

std::vector<uint8_t> compileToNative(const std::string &src, const std::string &deviceName, const std::string &revisionId, const std::string &options, const std::string &internalOptions, std::string &outCompilerLog) {
    std::vector<uint8_t> ret;

    const char *mainFileName = "main.cl";
    const char *argv[] = {"ocloc", "-q", "-device", deviceName.c_str(), "-revision_id", revisionId.c_str(), "-file", mainFileName, "-o", "output.bin", "", "", "", ""};
    uint32_t numArgs = sizeof(argv) / sizeof(argv[0]) - 4;
    int argIndex = 10;
    if (options.size() > 0) {
        argv[argIndex++] = "-options";
        argv[argIndex++] = options.c_str();
        numArgs += 2;
    }
    if (internalOptions.size() > 0) {
        argv[argIndex++] = "-internal_options";
        argv[argIndex++] = internalOptions.c_str();
        numArgs += 2;
    }
    const unsigned char *sources[] = {reinterpret_cast<const unsigned char *>(src.c_str())};
    size_t sourcesLengths[] = {src.size() + 1};
    const char *sourcesNames[] = {mainFileName};
    unsigned int numOutputs = 0U;
    unsigned char **outputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    int result = oclocInvoke(numArgs, argv,
                             1, sources, sourcesLengths, sourcesNames,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputs, &ouputLengths, &outputNames);

    unsigned char *binary = nullptr;
    size_t binaryLen = 0;
    const char *log = nullptr;
    size_t logLen = 0;
    for (unsigned int i = 0; i < numOutputs; ++i) {
        std::string spvExtension = ".spv";
        std::string logFileName = "stdout.log";
        auto nameLen = std::strlen(outputNames[i]);
        if (std::strstr(outputNames[i], "output.bin") != nullptr) {
            binary = outputs[i];
            binaryLen = ouputLengths[i];
        } else if ((nameLen >= logFileName.size()) && (std::strstr(outputNames[i], logFileName.c_str()) != nullptr)) {
            log = reinterpret_cast<const char *>(outputs[i]);
            logLen = ouputLengths[i];
            break;
        }
    }

    if ((result != 0) && (logLen == 0)) {
        outCompilerLog = "Unknown error, ocloc returned : " + std::to_string(result) + "\n";
        return ret;
    }

    if (logLen != 0) {
        outCompilerLog = std::string(log, logLen).c_str();
    }

    ret.assign(binary, binary + binaryLen);
    oclocFreeOutput(&numOutputs, &outputs, &ouputLengths, &outputNames);
    return ret;
}

const char *memcpyBytesTestKernelSrc = R"===(
kernel void memcpy_bytes(__global char *dst, const __global char *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

const char *memcpyBytesWithPrintfTestKernelSrc = R"==(
__kernel void memcpy_bytes(__global uchar *dst, const __global uchar *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = (uchar)(src[gid] + gid);
    if (gid == 0) {
        printf("gid =  %d \n", gid);
    }
}
)==";

} // namespace LevelZeroBlackBoxTests
