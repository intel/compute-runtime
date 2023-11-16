/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include "ocl_igc_interface/code_type.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class OclocArgHelper;

namespace NEO {

class CompilerCache;
class CompilerProductHelper;
class OclocFclFacade;
class OclocIgcFacade;

std::string convertToPascalCase(const std::string &inString);

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension);
std::string getSupportedDevices(OclocArgHelper *helper);

struct NameVersionPair : ocloc_name_version {
    NameVersionPair(ConstStringRef name, unsigned int version);
};
static_assert(sizeof(NameVersionPair) == sizeof(ocloc_name_version));

class OfflineCompiler {
  public:
    static std::vector<NameVersionPair> getExtensions(ConstStringRef product, bool needVersions, OclocArgHelper *helper);
    static std::vector<NameVersionPair> getOpenCLCVersions(ConstStringRef product, OclocArgHelper *helper);
    static std::vector<NameVersionPair> getOpenCLCFeatures(ConstStringRef product, OclocArgHelper *helper);
    static int query(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper);
    static int queryAcronymIds(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper);

    static OfflineCompiler *create(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles, int &retVal, OclocArgHelper *helper);

    MOCKABLE_VIRTUAL int build();
    std::string &getBuildLog();
    void printUsage();

    static constexpr ConstStringRef queryHelp =
        R"OCLOC_HELP(Depending on <query_option> will generate file
(with a name identical to query_option) containing requested information.

Usage: ocloc query <query_option> [-device device_filter]

-device device_filter defines optional filter for which devices the query is being made (where applicable)."
                      For allowed combinations of devices see "ocloc compile --help".
                      When filter matches multiple devices, then query will return common traits
                      supported by all matched devices.

Supported query options:
  OCL_DRIVER_VERSION                ; driver version
  NEO_REVISION                      ; NEO revision hash
  IGC_REVISION                      ; IGC revision hash
  CL_DEVICE_EXTENSIONS              ; list of extensions supported by device_filter
  CL_DEVICE_EXTENSIONS_WITH_VERSION ; list of extensions and their versions supported by device_filter
  CL_DEVICE_PROFILE                 ; OpenCL device profile supported by device_filter
  CL_DEVICE_OPENCL_C_ALL_VERSIONS   ; OpenCL C versions supported by device_filter
  CL_DEVICE_OPENCL_C_FEATURES       ; OpenCL C features supported by device_filter

Examples:
  ocloc query OCL_DRIVER_VERSION
  ocloc query CL_DEVICE_EXTENSIONS -device tgllp
  ocloc query CL_DEVICE_OPENCL_C_ALL_VERSIONS -device "*"
)OCLOC_HELP";

    static constexpr ConstStringRef idsHelp = R"OCLOC_HELP(
Depending on <acronym> will return all
matched versions (<major>.<minor>.<revision>)
that correspond to the given name.
All supported acronyms: %s.
)OCLOC_HELP";

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
    bool showHelpOnly() const { return showHelp; }

    std::string getOptions() {
        return options;
    }

  protected:
    OfflineCompiler();

    int initHardwareInfo(std::string deviceName);
    int initHardwareInfoForProductConfig(std::string deviceName);
    int initHardwareInfoForDeprecatedAcronyms(std::string deviceName, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper);
    bool isArgumentDeviceId(const std::string &argument) const;
    std::string getStringWithinDelimiters(const std::string &src);
    int initialize(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles);
    int parseCommandLine(size_t numArgs, const std::vector<std::string> &allArgs);
    void setStatelessToStatefulBufferOffsetFlag();
    void appendExtraInternalOptions(std::string &internalOptions);
    void parseDebugSettings();
    void storeBinary(char *&pDst, size_t &dstSize, const void *pSrc, const size_t srcSize);
    MOCKABLE_VIRTUAL int buildSourceCode();
    MOCKABLE_VIRTUAL std::string validateInputType(const std::string &input, bool isLlvm, bool isSpirv);
    MOCKABLE_VIRTUAL int buildIrBinary();
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
    MOCKABLE_VIRTUAL void createDir(const std::string &path);
    void unifyExcludeIrFlags();
    void enforceFormat(std::string &format);
    HardwareInfo hwInfo{};

    uint32_t deviceConfig = {};
    std::string deviceName;
    std::string productFamilyName;
    std::string inputFile;
    std::string outputFile;
    std::string binaryOutputFile;
    std::string outputDirectory;
    std::string options;
    std::unordered_map<std::string, std::string> perDeviceOptions;
    std::string internalOptions;
    std::string sourceCode;
    std::string buildLog;
    std::string optionsReadFromFile = "";
    std::string internalOptionsReadFromFile = "";
    std::string formatToEnforce = "";
    std::string irHash, genHash, dbgHash, elfHash;
    std::string cacheDir;

    bool allowCaching = false;
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
    size_t elfBinarySize = 0;
    char *genBinary = nullptr;
    size_t genBinarySize = 0;
    char *irBinary = nullptr;
    size_t irBinarySize = 0;
    char *debugDataBinary = nullptr;
    size_t debugDataBinarySize = 0;
    struct buildInfo;
    std::unique_ptr<buildInfo> pBuildInfo;
    int revisionId = -1;
    uint64_t hwInfoConfig = 0u;

    std::unique_ptr<OclocIgcFacade> igcFacade;
    std::unique_ptr<OclocFclFacade> fclFacade;
    std::unique_ptr<CompilerCache> cache;
    std::unique_ptr<CompilerProductHelper> compilerProductHelper;
    std::unique_ptr<ReleaseHelper> releaseHelper;
    IGC::CodeType::CodeType_t preferredIntermediateRepresentation;

    OclocArgHelper *argHelper = nullptr;
};

} // namespace NEO
