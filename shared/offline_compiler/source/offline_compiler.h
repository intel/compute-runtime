/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include "cif/common/cif_main.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {

struct HardwareInfo;
class OsLibrary;

std::string convertToPascalCase(const std::string &inString);

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension);
std::string getDevicesTypes();

class OfflineCompiler {
  public:
    static int query(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper);

    static OfflineCompiler *create(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles, int &retVal, OclocArgHelper *helper);
    MOCKABLE_VIRTUAL int build();
    std::string &getBuildLog();
    void printUsage();
    std::string getDevicesConfigs();

    static constexpr ConstStringRef queryHelp =
        "Depending on <query_option> will generate file\n"
        "(with a name adequate to <query_option>)\n"
        "containing either driver version or NEO revision hash.\n\n"
        "Usage: ocloc query <query_option>\n\n"
        "Supported query options:\n"
        "  OCL_DRIVER_VERSION  ; returns driver version\n"
        "  NEO_REVISION        ; returns NEO revision hash\n\n"
        "Examples:\n"
        "  Extract driver version\n"
        "    ocloc query OCL_DRIVER_VERSION\n";

    OfflineCompiler &operator=(const OfflineCompiler &) = delete;
    OfflineCompiler(const OfflineCompiler &) = delete;
    MOCKABLE_VIRTUAL ~OfflineCompiler();

    bool isQuiet() const {
        return quiet;
    }

    bool isOnlySpirV() const {
        return onlySpirV;
    }

    std::string parseBinAsCharArray(uint8_t *binary, size_t size, std::string &fileName);

    static bool readOptionsFromFile(std::string &optionsOut, const std::string &file, OclocArgHelper *helper);

    ArrayRef<const uint8_t> getPackedDeviceBinaryOutput() {
        return this->elfBinary;
    }

    static std::string getFileNameTrunk(std::string &filePath);
    const HardwareInfo &getHardwareInfo() const {
        return hwInfo;
    }

    std::string getOptionsReadFromFile() const {
        return optionsReadFromFile;
    }

    std::string getInternalOptionsReadFromFile() const {
        return internalOptionsReadFromFile;
    }

  protected:
    OfflineCompiler();

    void setFamilyType();
    int initHardwareInfo(std::string deviceName);
    std::string getStringWithinDelimiters(const std::string &src);
    int initialize(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles);
    int parseCommandLine(size_t numArgs, const std::vector<std::string> &allArgs);
    void setStatelessToStatefullBufferOffsetFlag();
    void appendExtraInternalOptions(std::string &internalOptions);
    void parseDebugSettings();
    void storeBinary(char *&pDst, size_t &dstSize, const void *pSrc, const size_t srcSize);
    MOCKABLE_VIRTUAL int buildSourceCode();
    MOCKABLE_VIRTUAL std::string validateInputType(const std::string &input, bool isLlvm, bool isSpirv);
    int buildIrBinary();
    void updateBuildLog(const char *pErrorString, const size_t errorStringSize);
    MOCKABLE_VIRTUAL bool generateElfBinary();
    std::string generateFilePathForIr(const std::string &fileNameBase) {
        const char *ext = (isSpirV) ? ".spv" : ".bc";
        return generateFilePath(outputDirectory, fileNameBase, useLlvmText ? ".ll" : ext);
    }

    std::string generateOptsSuffix() {
        std::string suffix{useOptionsSuffix ? options : ""};
        std::replace(suffix.begin(), suffix.end(), ' ', '_');
        return suffix;
    }
    MOCKABLE_VIRTUAL void writeOutAllFiles();
    void unifyExcludeIrFlags();
    HardwareInfo hwInfo;

    PRODUCT_CONFIG deviceConfig = UNKNOWN_ISA;
    std::string deviceName;
    std::string familyNameWithType;
    std::string inputFile;
    std::string outputFile;
    std::string outputDirectory;
    std::string options;
    std::string internalOptions;
    std::string sourceCode;
    std::string buildLog;
    std::string optionsReadFromFile = "";
    std::string internalOptionsReadFromFile = "";

    bool dumpFiles = true;
    bool useLlvmText = false;
    bool useLlvmBc = false;
    bool useCppFile = false;
    bool useGenFile = false;
    bool useOptionsSuffix = false;
    bool quiet = false;
    bool onlySpirV = false;
    bool inputFileLlvm = false;
    bool inputFileSpirV = false;
    bool outputNoSuffix = false;
    bool forceStatelessToStatefulOptimization = false;
    bool isSpirV = false;
    bool showHelp = false;
    bool excludeIr = false;

    std::vector<uint8_t> elfBinary;
    char *genBinary = nullptr;
    size_t genBinarySize = 0;
    char *irBinary = nullptr;
    size_t irBinarySize = 0;
    char *debugDataBinary = nullptr;
    size_t debugDataBinarySize = 0;
    struct buildInfo;
    std::unique_ptr<buildInfo> pBuildInfo;
    std::unique_ptr<OsLibrary> igcLib = nullptr;
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain = nullptr;
    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igcDeviceCtx = nullptr;
    int revisionId = -1;

    std::unique_ptr<OsLibrary> fclLib = nullptr;
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain = nullptr;
    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fclDeviceCtx = nullptr;
    IGC::CodeType::CodeType_t preferredIntermediateRepresentation;

    OclocArgHelper *argHelper = nullptr;
};
} // namespace NEO
