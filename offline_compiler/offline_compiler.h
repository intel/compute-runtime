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
#include "cif/common/cif_main.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include <cstdint>
#include <string>
#include <memory>

namespace OCLRT {

struct HardwareInfo;
class OsLibrary;

std::string convertToPascalCase(const std::string &inString);

enum ErrorCode {
    INVALID_COMMAND_LINE = -5150,
    INVALID_FILE = -5151,
    PRINT_USAGE = -5152,
};

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension);

class OfflineCompiler {
  public:
    static OfflineCompiler *create(uint32_t numArgs, const char **argv, int &retVal);
    int build();
    std::string &getBuildLog();
    void printUsage();

    OfflineCompiler &operator=(const OfflineCompiler &) = delete;
    OfflineCompiler(const OfflineCompiler &) = delete;
    ~OfflineCompiler();

    bool isQuiet() const {
        return quiet;
    }

    std::string parseBinAsCharArray(uint8_t *binary, size_t size, std::string &fileName);
    static bool readOptionsFromFile(std::string &optionsOut, const std::string &file);

  protected:
    OfflineCompiler();

    int getHardwareInfo(const char *pDeviceName);
    std::string getFileNameTrunk(std::string &filePath);
    std::string getStringWithinDelimiters(const std::string &src);
    int initialize(uint32_t numArgs, const char **argv);
    int parseCommandLine(uint32_t numArgs, const char **argv);
    void parseDebugSettings();
    void storeBinary(char *&pDst, size_t &dstSize, const void *pSrc, const size_t srcSize);
    int buildSourceCode();
    void updateBuildLog(const char *pErrorString, const size_t errorStringSize);
    bool generateElfBinary();
    std::string generateFilePathForIr(const std::string &fileNameBase) {
        const char *ext = (isSpirV) ? ".spv" : ".bc";
        return generateFilePath(outputDirectory, fileNameBase, useLlvmText ? ".ll" : ext);
    }

    std::string generateOptsSuffix() {
        std::string suffix{useOptionsSuffix ? options : ""};
        std::replace(suffix.begin(), suffix.end(), ' ', '_');
        return suffix;
    }
    void writeOutAllFiles();
    const HardwareInfo *hwInfo = nullptr;

    std::string deviceName;
    std::string familyNameWithType;
    std::string inputFile;
    std::string outputFile;
    std::string outputDirectory;
    std::string options;
    std::string internalOptions;
    std::string sourceCode;
    std::string buildLog;

    bool useLlvmText = false;
    bool useCppFile = false;
    bool useOptionsSuffix = false;
    bool quiet = false;
    bool inputFileLlvm = false;

    char *elfBinary = nullptr;
    size_t elfBinarySize = 0;
    char *genBinary = nullptr;
    size_t genBinarySize = 0;
    char *irBinary = nullptr;
    size_t irBinarySize = 0;
    bool isSpirV = false;
    char *debugDataBinary = nullptr;
    size_t debugDataBinarySize = 0;

    std::unique_ptr<OsLibrary> igcLib = nullptr;
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain = nullptr;
    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igcDeviceCtx = nullptr;

    std::unique_ptr<OsLibrary> fclLib = nullptr;
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain = nullptr;
    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fclDeviceCtx = nullptr;
    IGC::CodeType::CodeType_t preferredIntermediateRepresentation;
};
} // namespace OCLRT
