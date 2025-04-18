/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include "ocl_igc_interface/code_type.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class OclocArgHelper;

namespace Ocloc {
enum class SupportedDevicesMode;
};

namespace NEO {

class CompilerCache;
class CompilerProductHelper;
class OclocFclFacadeBase;
class OclocIgcFacade;

std::string convertToPascalCase(const std::string &inString);

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension);
std::string getSupportedDevices(OclocArgHelper *helper);

struct NameVersionPair : ocloc_name_version {
    NameVersionPair(ConstStringRef name, unsigned int version);
};
static_assert(sizeof(NameVersionPair) == sizeof(ocloc_name_version));

const HardwareInfo *getHwInfoForDeprecatedAcronym(const std::string &deviceName);

constexpr bool isIntermediateRepresentation(IGC::CodeType::CodeType_t codeType) {
    return false == ((IGC::CodeType::oclC == codeType) || (IGC::CodeType::oclCpp == codeType) || (IGC::CodeType::oclGenBin == codeType));
}

constexpr const char *getFileExtension(IGC::CodeType::CodeType_t codeType) {
    switch (codeType) {
    default:
        return ".bin";
    case IGC::CodeType::llvmBc:
        return ".bc";
    case IGC::CodeType::llvmLl:
        return ".ll";
    case IGC::CodeType::spirV:
        return ".spv";
    }
}

class OfflineCompiler : NEO::NonCopyableAndNonMovableClass {
  public:
    static std::vector<NameVersionPair> getExtensions(ConstStringRef product, bool needVersions, OclocArgHelper *helper);
    static std::vector<NameVersionPair> getOpenCLCVersions(ConstStringRef product, OclocArgHelper *helper);
    static std::vector<NameVersionPair> getOpenCLCFeatures(ConstStringRef product, OclocArgHelper *helper);
    static int query(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper);
    static int queryAcronymIds(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper);
    static int querySupportedDevices(Ocloc::SupportedDevicesMode mode, OclocArgHelper *helper);

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
  SUPPORTED_DEVICES                 ; Generates a YAML file with information about supported devices

SUPPORTED_DEVICES option:
  Linux:
    Description: Generates a YAML file containing information about supported devices
                 for the current and previous versions of ocloc.
    Usage: ocloc query SUPPORTED_DEVICES [<mode>]
    Supported Modes:
      -merge   - Combines supported devices from all ocloc versions into a single list (default if not specified)
      -concat  - Lists supported devices for each ocloc version separately
    Output file: <ocloc_version>_supported_devices_<mode>.yaml

  Windows:
    Description: Generates a YAML file containing information about supported devices
                 for the current version of ocloc.
    Usage: ocloc query SUPPORTED_DEVICES
    Output file: <ocloc_version>_supported_devices.yaml

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
    int initHardwareInfoForDeprecatedAcronyms(const std::string &deviceName, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper);
    bool isArgumentDeviceId(const std::string &argument) const;
    std::string getStringWithinDelimiters(const std::string &src);
    int initialize(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles);
    int parseCommandLine(size_t numArgs, const std::vector<std::string> &allArgs);
    int parseCommandLineExt(size_t numArgs, const std::vector<std::string> &allArgs, uint32_t &argIndex);
    void setStatelessToStatefulBufferOffsetFlag();
    void appendExtraInternalOptions(std::string &internalOptions);
    void parseDebugSettings();
    void storeBinary(char *&pDst, size_t &dstSize, const void *pSrc, const size_t srcSize);
    MOCKABLE_VIRTUAL int buildSourceCode();
    MOCKABLE_VIRTUAL std::string validateInputType(const std::string &input, bool isLlvm, bool isSpirv);
    MOCKABLE_VIRTUAL int buildToIrBinary();
    void updateBuildLog(const char *pErrorString, const size_t errorStringSize);
    MOCKABLE_VIRTUAL bool generateElfBinary();
    std::string generateFilePathForIr(const std::string &fileNameBase) {
        const char *ext = getFileExtension(intermediateRepresentation);
        return generateFilePath(outputDirectory, fileNameBase, ext);
    }

    std::string generateOptsSuffix() {
        std::string suffix{useOptionsSuffix ? options : ""};
        std::replace(suffix.begin(), suffix.end(), ' ', '_');
        return suffix;
    }

    MOCKABLE_VIRTUAL void writeOutAllFiles();
    MOCKABLE_VIRTUAL int createDir(const std::string &path);
    bool useIgcAsFcl();
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
    std::string addressingMode = "default";
    CompilerOptions::HeaplessMode heaplessMode = CompilerOptions::HeaplessMode::defaultMode;
    std::string irHash, genHash, dbgHash, elfHash;
    std::string cacheDir;

    bool allowCaching = false;
    bool dumpFiles = true;
    bool useCppFile = false;
    bool useGenFile = false;
    bool useOptionsSuffix = false;
    bool quiet = false;
    bool onlySpirV = false;
    bool useLlvmTxt = false;

    IGC::CodeType::CodeType_t inputCodeType = IGC::CodeType::oclC;

    bool inputFileLlvm() const {
        return (IGC::CodeType::llvmBc == inputCodeType) || (IGC::CodeType::llvmLl == inputCodeType);
    }

    bool inputFileSpirV() const {
        return IGC::CodeType::spirV == inputCodeType;
    }

    bool outputNoSuffix = false;
    bool forceStatelessToStatefulOptimization = false;
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
    struct BuildInfo;
    std::unique_ptr<BuildInfo> pBuildInfo;
    int revisionId = -1;
    uint64_t hwInfoConfig = 0u;

    std::unique_ptr<OclocIgcFacade> igcFacade;
    std::unique_ptr<OclocFclFacadeBase> fclFacade;
    std::unique_ptr<CompilerCache> cache;
    std::unique_ptr<CompilerProductHelper> compilerProductHelper;
    std::unique_ptr<ReleaseHelper> releaseHelper;
    IGC::CodeType::CodeType_t preferredIntermediateRepresentation;
    IGC::CodeType::CodeType_t intermediateRepresentation = IGC::CodeType::undefined;

    OclocArgHelper *argHelper = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<OfflineCompiler>);

} // namespace NEO
