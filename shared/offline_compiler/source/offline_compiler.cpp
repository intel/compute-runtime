/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler.h"

#include "shared/offline_compiler/source/queries.h"
#include "shared/offline_compiler/source/utilities/get_git_version_info.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/validators.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_library.h"

#include "cif/common/cif_main.h"
#include "cif/helpers/error.h"
#include "cif/import/library_api.h"
#include "compiler_options.h"
#include "igfxfmid.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/platform_helper.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <list>

#ifdef _WIN32
#include <direct.h>
#define MakeDirectory _mkdir
#define GetCurrentWorkingDirectory _getcwd
#else
#include <sys/stat.h>
#define MakeDirectory(dir) mkdir(dir, 0777)
#define GetCurrentWorkingDirectory getcwd
#endif

namespace NEO {

CIF::CIFMain *createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc);

std::string convertToPascalCase(const std::string &inString) {
    std::string outString;
    bool capitalize = true;

    for (unsigned int i = 0; i < inString.length(); i++) {
        if (isalpha(inString[i]) && capitalize == true) {
            outString += toupper(inString[i]);
            capitalize = false;
        } else if (inString[i] == '_') {
            capitalize = true;
        } else {
            outString += inString[i];
        }
    }
    return outString;
}

OfflineCompiler::OfflineCompiler() = default;
OfflineCompiler::~OfflineCompiler() {
    pBuildInfo.reset();
    delete[] irBinary;
    delete[] genBinary;
    delete[] debugDataBinary;
}

OfflineCompiler *OfflineCompiler::create(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles, int &retVal, OclocArgHelper *helper) {
    retVal = SUCCESS;
    auto pOffCompiler = new OfflineCompiler();

    if (pOffCompiler) {
        pOffCompiler->argHelper = helper;
        retVal = pOffCompiler->initialize(numArgs, allArgs, dumpFiles);
    }

    if (retVal != SUCCESS) {
        delete pOffCompiler;
        pOffCompiler = nullptr;
    }

    return pOffCompiler;
}

void printQueryHelp(OclocArgHelper *helper) {
    helper->printf(OfflineCompiler::queryHelp.data());
}

int OfflineCompiler::query(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper) {
    if (allArgs.size() != 3) {
        helper->printf("Error: Invalid command line. Expected ocloc query <argument>");
        return INVALID_COMMAND_LINE;
    }

    auto retVal = SUCCESS;
    auto &arg = allArgs[2];

    if (Queries::queryNeoRevision == arg) {
        auto revision = NEO::getRevision();
        helper->saveOutput(Queries::queryNeoRevision.data(), revision.c_str(), revision.size() + 1);
    } else if (Queries::queryOCLDriverVersion == arg) {
        auto driverVersion = NEO::getOclDriverVersion();
        helper->saveOutput(Queries::queryOCLDriverVersion.data(), driverVersion.c_str(), driverVersion.size() + 1);
    } else if ("--help" == arg) {
        printQueryHelp(helper);
    } else {
        helper->printf("Error: Invalid command line. Uknown argument %s.", arg.c_str());
        retVal = INVALID_COMMAND_LINE;
    }

    return retVal;
}

struct OfflineCompiler::buildInfo {
    std::unique_ptr<CIF::Builtins::BufferLatest, CIF::RAII::ReleaseHelper<CIF::Builtins::BufferLatest>> fclOptions;
    std::unique_ptr<CIF::Builtins::BufferLatest, CIF::RAII::ReleaseHelper<CIF::Builtins::BufferLatest>> fclInternalOptions;
    std::unique_ptr<IGC::OclTranslationOutputTagOCL, CIF::RAII::ReleaseHelper<IGC::OclTranslationOutputTagOCL>> fclOutput;
    IGC::CodeType::CodeType_t intermediateRepresentation;
};

int OfflineCompiler::buildIrBinary() {
    int retVal = SUCCESS;
    UNRECOVERABLE_IF(fclDeviceCtx == nullptr);
    pBuildInfo->intermediateRepresentation = useLlvmText ? IGC::CodeType::llvmLl
                                                         : (useLlvmBc ? IGC::CodeType::llvmBc : preferredIntermediateRepresentation);

    //sourceCode.size() returns the number of characters without null terminated char
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> fclSrc = nullptr;
    pBuildInfo->fclOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), options.c_str(), options.size());
    pBuildInfo->fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), internalOptions.c_str(), internalOptions.size());
    auto err = CIF::Builtins::CreateConstBuffer(fclMain.get(), nullptr, 0);

    auto srcType = IGC::CodeType::undefined;
    std::vector<uint8_t> tempSrcStorage;
    if (this->argHelper->hasHeaders()) {
        srcType = IGC::CodeType::elf;

        NEO::Elf::ElfEncoder<> elfEncoder(true, true, 1U);
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_SOURCE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SOURCE, "CLMain", sourceCode);

        for (const auto &header : this->argHelper->getHeaders()) {
            ArrayRef<const uint8_t> headerData(header.data, header.length);
            ConstStringRef headerName = header.name;

            elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_HEADER, headerName, headerData);
        }
        tempSrcStorage = elfEncoder.encode();
        fclSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), tempSrcStorage.data(), tempSrcStorage.size());
    } else {
        srcType = IGC::CodeType::oclC;
        fclSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), sourceCode.c_str(), sourceCode.size() + 1);
    }

    auto fclTranslationCtx = fclDeviceCtx->CreateTranslationCtx(srcType, pBuildInfo->intermediateRepresentation, err.get());

    if (true == NEO::areNotNullptr(err->GetMemory<char>())) {
        updateBuildLog(err->GetMemory<char>(), err->GetSizeRaw());
        retVal = BUILD_PROGRAM_FAILURE;
        return retVal;
    }

    if (false == NEO::areNotNullptr(fclSrc.get(), pBuildInfo->fclOptions.get(), pBuildInfo->fclInternalOptions.get(),
                                    fclTranslationCtx.get())) {
        retVal = OUT_OF_HOST_MEMORY;
        return retVal;
    }

    pBuildInfo->fclOutput = fclTranslationCtx->Translate(fclSrc.get(), pBuildInfo->fclOptions.get(),
                                                         pBuildInfo->fclInternalOptions.get(), nullptr, 0);

    if (pBuildInfo->fclOutput == nullptr) {
        retVal = OUT_OF_HOST_MEMORY;
        return retVal;
    }

    UNRECOVERABLE_IF(pBuildInfo->fclOutput->GetBuildLog() == nullptr);
    UNRECOVERABLE_IF(pBuildInfo->fclOutput->GetOutput() == nullptr);

    if (pBuildInfo->fclOutput->Successful() == false) {
        updateBuildLog(pBuildInfo->fclOutput->GetBuildLog()->GetMemory<char>(), pBuildInfo->fclOutput->GetBuildLog()->GetSizeRaw());
        retVal = BUILD_PROGRAM_FAILURE;
        return retVal;
    }

    storeBinary(irBinary, irBinarySize, pBuildInfo->fclOutput->GetOutput()->GetMemory<char>(), pBuildInfo->fclOutput->GetOutput()->GetSizeRaw());
    isSpirV = pBuildInfo->intermediateRepresentation == IGC::CodeType::spirV;

    updateBuildLog(pBuildInfo->fclOutput->GetBuildLog()->GetMemory<char>(), pBuildInfo->fclOutput->GetBuildLog()->GetSizeRaw());

    return retVal;
}

std::string OfflineCompiler::validateInputType(const std::string &input, bool isLlvm, bool isSpirv) {
    auto asBitcode = ArrayRef<const uint8_t>::fromAny(input.data(), input.size());
    if (isSpirv) {
        if (NEO::isSpirVBitcode(asBitcode)) {
            return "";
        }
        return "Warning : file does not look like spirv bitcode (wrong magic numbers)";
    }

    if (isLlvm) {
        if (NEO::isLlvmBitcode(asBitcode)) {
            return "";
        }
        return "Warning : file does not look like llvm bitcode (wrong magic numbers)";
    }

    if (NEO::isSpirVBitcode(asBitcode)) {
        return "Warning : file looks like spirv bitcode (based on magic numbers) - please make sure proper CLI flags are present";
    }

    if (NEO::isLlvmBitcode(asBitcode)) {
        return "Warning : file looks like llvm bitcode (based on magic numbers) - please make sure proper CLI flags are present";
    }

    return "";
}

int OfflineCompiler::buildSourceCode() {
    int retVal = SUCCESS;

    do {
        if (sourceCode.empty()) {
            retVal = INVALID_PROGRAM;
            break;
        }
        UNRECOVERABLE_IF(igcDeviceCtx == nullptr);
        auto inputTypeWarnings = validateInputType(sourceCode, inputFileLlvm, inputFileSpirV);
        this->argHelper->printf(inputTypeWarnings.c_str());

        CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> igcOutput;
        bool inputIsIntermediateRepresentation = inputFileLlvm || inputFileSpirV;
        if (false == inputIsIntermediateRepresentation) {
            retVal = buildIrBinary();
            if (retVal != SUCCESS)
                break;

            auto igcTranslationCtx = igcDeviceCtx->CreateTranslationCtx(pBuildInfo->intermediateRepresentation, IGC::CodeType::oclGenBin);
            igcOutput = igcTranslationCtx->Translate(pBuildInfo->fclOutput->GetOutput(), pBuildInfo->fclOptions.get(),
                                                     pBuildInfo->fclInternalOptions.get(),
                                                     nullptr, 0);

        } else {
            storeBinary(irBinary, irBinarySize, sourceCode.c_str(), sourceCode.size());
            isSpirV = inputFileSpirV;
            auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), sourceCode.c_str(), sourceCode.size());
            auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), options.c_str(), options.size());
            auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), internalOptions.c_str(), internalOptions.size());
            auto igcTranslationCtx = igcDeviceCtx->CreateTranslationCtx(inputFileSpirV ? IGC::CodeType::spirV : IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin);
            igcOutput = igcTranslationCtx->Translate(igcSrc.get(), igcOptions.get(), igcInternalOptions.get(), nullptr, 0);
        }
        if (igcOutput == nullptr) {
            retVal = OUT_OF_HOST_MEMORY;
            break;
        }
        UNRECOVERABLE_IF(igcOutput->GetBuildLog() == nullptr);
        UNRECOVERABLE_IF(igcOutput->GetOutput() == nullptr);
        updateBuildLog(igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());

        if (igcOutput->GetOutput()->GetSizeRaw() != 0) {
            storeBinary(genBinary, genBinarySize, igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetSizeRaw());
        }
        if (igcOutput->GetDebugData()->GetSizeRaw() != 0) {
            storeBinary(debugDataBinary, debugDataBinarySize, igcOutput->GetDebugData()->GetMemory<char>(), igcOutput->GetDebugData()->GetSizeRaw());
        }
        retVal = igcOutput->Successful() ? SUCCESS : BUILD_PROGRAM_FAILURE;
    } while (0);

    return retVal;
}

int OfflineCompiler::build() {
    int retVal = SUCCESS;
    if (isOnlySpirV()) {
        retVal = buildIrBinary();
    } else {
        retVal = buildSourceCode();
    }
    generateElfBinary();
    if (dumpFiles) {
        writeOutAllFiles();
    }

    return retVal;
}

void OfflineCompiler::updateBuildLog(const char *pErrorString, const size_t errorStringSize) {
    std::string errorString = (errorStringSize && pErrorString) ? std::string(pErrorString, pErrorString + errorStringSize) : "";
    if (errorString[0] != '\0') {
        if (buildLog.empty()) {
            buildLog.assign(errorString.c_str());
        } else {
            buildLog.append("\n");
            buildLog.append(errorString.c_str());
        }
    }
}

std::string &OfflineCompiler::getBuildLog() {
    return buildLog;
}

int OfflineCompiler::getHardwareInfo(std::string deviceName) {
    int retVal = INVALID_DEVICE;

    overridePlatformName(deviceName);
    std::transform(deviceName.begin(), deviceName.end(), deviceName.begin(), ::tolower);

    const char hexPrefix = 2;
    std::string product("");

    if (deviceName.substr(0, hexPrefix) == "0x" && std::all_of(deviceName.begin() + hexPrefix, deviceName.end(), (::isxdigit))) {
        product = argHelper->returnProductNameForDevice(stoi(deviceName, 0, 16));
        if (!product.empty()) {
            argHelper->printf("Auto-detected target based on %s device id: %s\n", deviceName.c_str(), product.c_str());
            deviceName = product;
        } else {
            argHelper->printf("Could not determine target based on device id: %s\n", deviceName.c_str());
            return retVal;
        }
    }

    for (unsigned int productId = 0; productId < IGFX_MAX_PRODUCT; ++productId) {
        if (hardwarePrefix[productId] && (0 == strcmp(deviceName.c_str(), hardwarePrefix[productId]))) {
            if (hardwareInfoTable[productId]) {
                hwInfo = *hardwareInfoTable[productId];
                if (revisionId != -1) {
                    hwInfo.platform.usRevId = revisionId;
                }
                auto hwInfoConfig = defaultHardwareInfoConfigTable[hwInfo.platform.eProductFamily];
                setHwInfoValuesFromConfig(hwInfoConfig, hwInfo);
                hardwareInfoSetup[hwInfo.platform.eProductFamily](&hwInfo, true, hwInfoConfig);
                familyNameWithType.clear();
                familyNameWithType.append(familyName[hwInfo.platform.eRenderCoreFamily]);
                familyNameWithType.append(hwInfo.capabilityTable.platformType);
                retVal = SUCCESS;
                break;
            }
        }
    }

    return retVal;
}

std::string OfflineCompiler::getStringWithinDelimiters(const std::string &src) {
    size_t start = src.find("R\"===(");
    size_t stop = src.find(")===\"");

    DEBUG_BREAK_IF(std::string::npos == start);
    DEBUG_BREAK_IF(std::string::npos == stop);

    start += strlen("R\"===(");
    size_t size = stop - start;

    std::string dst(src, start, size + 1);
    dst[size] = '\0'; // put null char at the end

    return dst;
}

int OfflineCompiler::initialize(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles) {
    this->dumpFiles = dumpFiles;
    int retVal = SUCCESS;
    const char *source = nullptr;
    std::unique_ptr<char[]> sourceFromFile;
    size_t sourceFromFileSize = 0;
    this->pBuildInfo = std::make_unique<buildInfo>();
    retVal = parseCommandLine(numArgs, allArgs);
    if (showHelp) {
        printUsage();
        return retVal;
    } else if (retVal != SUCCESS) {
        return retVal;
    }

    if (options.empty()) {
        // try to read options from file if not provided by commandline
        size_t ext_start = inputFile.find_last_of(".");
        if (ext_start != std::string::npos) {
            std::string oclocOptionsFileName = inputFile.substr(0, ext_start);
            oclocOptionsFileName.append("_ocloc_options.txt");

            std::string oclocOptionsFromFile;
            bool oclocOptionsRead = readOptionsFromFile(oclocOptionsFromFile, oclocOptionsFileName, argHelper);
            if (oclocOptionsRead && !isQuiet()) {
                argHelper->printf("Building with ocloc options:\n%s\n", oclocOptionsFromFile.c_str());
            }

            if (oclocOptionsRead) {
                std::istringstream iss(allArgs[0] + " " + oclocOptionsFromFile);
                std::vector<std::string> tokens{
                    std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

                retVal = parseCommandLine(tokens.size(), tokens);
                if (retVal != SUCCESS) {
                    return retVal;
                }
            }

            std::string optionsFileName = inputFile.substr(0, ext_start);
            optionsFileName.append("_options.txt");

            bool optionsRead = readOptionsFromFile(options, optionsFileName, argHelper);
            if (optionsRead && !isQuiet()) {
                argHelper->printf("Building with options:\n%s\n", options.c_str());
            }

            std::string internalOptionsFileName = inputFile.substr(0, ext_start);
            internalOptionsFileName.append("_internal_options.txt");

            std::string internalOptionsFromFile;
            bool internalOptionsRead = readOptionsFromFile(internalOptionsFromFile, internalOptionsFileName, argHelper);
            if (internalOptionsRead && !isQuiet()) {
                argHelper->printf("Building with internal options:\n%s\n", internalOptionsFromFile.c_str());
            }
            CompilerOptions::concatenateAppend(internalOptions, internalOptionsFromFile);
        }
    }

    retVal = deviceName.empty() ? SUCCESS : getHardwareInfo(deviceName.c_str());
    if (retVal != SUCCESS) {
        argHelper->printf("Error: Cannot get HW Info for device %s.\n", deviceName.c_str());
        return retVal;
    }

    if (CompilerOptions::contains(options, CompilerOptions::generateDebugInfo.str())) {
        if (hwInfo.platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
            internalOptions = CompilerOptions::concatenate(internalOptions, CompilerOptions::debugKernelEnable);
        }
    }

    if (deviceName.empty()) {
        internalOptions = CompilerOptions::concatenate("-ocl-version=300 -cl-ext=-all,+cl_khr_3d_image_writes", internalOptions);
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableImageSupport);
    } else {
        appendExtensionsToInternalOptions(hwInfo, options, internalOptions);
    }

    parseDebugSettings();

    // set up the device inside the program
    sourceFromFile = argHelper->loadDataFromFile(inputFile, sourceFromFileSize);
    if (sourceFromFileSize == 0) {
        retVal = INVALID_FILE;
        return retVal;
    }

    if (inputFileLlvm || inputFileSpirV) {
        // use the binary input "as is"
        sourceCode.assign(sourceFromFile.get(), sourceFromFileSize);
    } else {
        // for text input, we also accept files used as runtime builtins
        source = strstr((const char *)sourceFromFile.get(), "R\"===(");
        sourceCode = (source != nullptr) ? getStringWithinDelimiters(sourceFromFile.get()) : sourceFromFile.get();
    }

    if ((inputFileSpirV == false) && (inputFileLlvm == false)) {
        auto fclLibFile = OsLibrary::load(Os::frontEndDllName);
        if (fclLibFile == nullptr) {
            argHelper->printf("Error: Failed to load %s\n", Os::frontEndDllName);
            return OUT_OF_HOST_MEMORY;
        }

        this->fclLib.reset(fclLibFile);
        if (this->fclLib == nullptr) {
            return OUT_OF_HOST_MEMORY;
        }

        auto fclCreateMain = reinterpret_cast<CIF::CreateCIFMainFunc_t>(this->fclLib->getProcAddress(CIF::CreateCIFMainFuncName));
        if (fclCreateMain == nullptr) {
            return OUT_OF_HOST_MEMORY;
        }

        this->fclMain = CIF::RAII::UPtr(createMainNoSanitize(fclCreateMain));
        if (this->fclMain == nullptr) {
            return OUT_OF_HOST_MEMORY;
        }

        if (false == this->fclMain->IsCompatible<IGC::FclOclDeviceCtx>()) {
            argHelper->printf("Incompatible interface in FCL : %s\n", CIF::InterfaceIdCoder::Dec(this->fclMain->FindIncompatible<IGC::FclOclDeviceCtx>()).c_str());
            DEBUG_BREAK_IF(true);
            return OUT_OF_HOST_MEMORY;
        }

        this->fclDeviceCtx = this->fclMain->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
        if (this->fclDeviceCtx == nullptr) {
            return OUT_OF_HOST_MEMORY;
        }

        fclDeviceCtx->SetOclApiVersion(hwInfo.capabilityTable.clVersionSupport * 10);
        preferredIntermediateRepresentation = fclDeviceCtx->GetPreferredIntermediateRepresentation();
        if (this->fclDeviceCtx->GetUnderlyingVersion() > 4U) {
            auto igcPlatform = fclDeviceCtx->GetPlatformHandle();
            if (nullptr == igcPlatform) {
                return OUT_OF_HOST_MEMORY;
            }
            IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform, hwInfo.platform);
        }
    } else {
        if (!isQuiet()) {
            argHelper->printf("Compilation from IR - skipping loading of FCL\n");
        }
        preferredIntermediateRepresentation = IGC::CodeType::spirV;
    }

    this->igcLib.reset(OsLibrary::load(Os::igcDllName));
    if (this->igcLib == nullptr) {
        return OUT_OF_HOST_MEMORY;
    }

    auto igcCreateMain = reinterpret_cast<CIF::CreateCIFMainFunc_t>(this->igcLib->getProcAddress(CIF::CreateCIFMainFuncName));
    if (igcCreateMain == nullptr) {
        return OUT_OF_HOST_MEMORY;
    }

    this->igcMain = CIF::RAII::UPtr(createMainNoSanitize(igcCreateMain));
    if (this->igcMain == nullptr) {
        return OUT_OF_HOST_MEMORY;
    }

    std::vector<CIF::InterfaceId_t> interfacesToIgnore = {IGC::OclGenBinaryBase::GetInterfaceId()};
    if (false == this->igcMain->IsCompatible<IGC::IgcOclDeviceCtx>(&interfacesToIgnore)) {
        argHelper->printf("Incompatible interface in IGC : %s\n", CIF::InterfaceIdCoder::Dec(this->igcMain->FindIncompatible<IGC::IgcOclDeviceCtx>(&interfacesToIgnore)).c_str());
        DEBUG_BREAK_IF(true);
        return OUT_OF_HOST_MEMORY;
    }

    CIF::Version_t verMin = 0, verMax = 0;
    if (false == this->igcMain->FindSupportedVersions<IGC::IgcOclDeviceCtx>(IGC::OclGenBinaryBase::GetInterfaceId(), verMin, verMax)) {
        argHelper->printf("Patchtoken interface is missing");
        return OUT_OF_HOST_MEMORY;
    }

    this->igcDeviceCtx = this->igcMain->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
    if (this->igcDeviceCtx == nullptr) {
        return OUT_OF_HOST_MEMORY;
    }
    this->igcDeviceCtx->SetProfilingTimerResolution(static_cast<float>(hwInfo.capabilityTable.defaultProfilingTimerResolution));
    auto igcPlatform = this->igcDeviceCtx->GetPlatformHandle();
    auto igcGtSystemInfo = this->igcDeviceCtx->GetGTSystemInfoHandle();
    auto igcFeWa = this->igcDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle();
    if ((igcPlatform == nullptr) || (igcGtSystemInfo == nullptr) || (igcFeWa == nullptr)) {
        return OUT_OF_HOST_MEMORY;
    }
    IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform.get(), hwInfo.platform);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo.get(), hwInfo.gtSystemInfo);
    // populate with features
    igcFeWa.get()->SetFtrDesktop(hwInfo.featureTable.ftrDesktop);
    igcFeWa.get()->SetFtrChannelSwizzlingXOREnabled(hwInfo.featureTable.ftrChannelSwizzlingXOREnabled);

    igcFeWa.get()->SetFtrGtBigDie(hwInfo.featureTable.ftrGtBigDie);
    igcFeWa.get()->SetFtrGtMediumDie(hwInfo.featureTable.ftrGtMediumDie);
    igcFeWa.get()->SetFtrGtSmallDie(hwInfo.featureTable.ftrGtSmallDie);

    igcFeWa.get()->SetFtrGT1(hwInfo.featureTable.ftrGT1);
    igcFeWa.get()->SetFtrGT1_5(hwInfo.featureTable.ftrGT1_5);
    igcFeWa.get()->SetFtrGT2(hwInfo.featureTable.ftrGT2);
    igcFeWa.get()->SetFtrGT3(hwInfo.featureTable.ftrGT3);
    igcFeWa.get()->SetFtrGT4(hwInfo.featureTable.ftrGT4);

    igcFeWa.get()->SetFtrIVBM0M1Platform(hwInfo.featureTable.ftrIVBM0M1Platform);
    igcFeWa.get()->SetFtrGTL(hwInfo.featureTable.ftrGT1);
    igcFeWa.get()->SetFtrGTM(hwInfo.featureTable.ftrGT2);
    igcFeWa.get()->SetFtrGTH(hwInfo.featureTable.ftrGT3);

    igcFeWa.get()->SetFtrSGTPVSKUStrapPresent(hwInfo.featureTable.ftrSGTPVSKUStrapPresent);
    igcFeWa.get()->SetFtrGTA(hwInfo.featureTable.ftrGTA);
    igcFeWa.get()->SetFtrGTC(hwInfo.featureTable.ftrGTC);
    igcFeWa.get()->SetFtrGTX(hwInfo.featureTable.ftrGTX);
    igcFeWa.get()->SetFtr5Slice(hwInfo.featureTable.ftr5Slice);

    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (compilerHwInfoConfig) {
        igcFeWa.get()->SetFtrGpGpuMidThreadLevelPreempt(compilerHwInfoConfig->isMidThreadPreemptionSupported(hwInfo));
    }
    igcFeWa.get()->SetFtrIoMmuPageFaulting(hwInfo.featureTable.ftrIoMmuPageFaulting);
    igcFeWa.get()->SetFtrWddm2Svm(hwInfo.featureTable.ftrWddm2Svm);
    igcFeWa.get()->SetFtrPooledEuEnabled(hwInfo.featureTable.ftrPooledEuEnabled);

    igcFeWa.get()->SetFtrResourceStreamer(hwInfo.featureTable.ftrResourceStreamer);

    return retVal;
}

int OfflineCompiler::parseCommandLine(size_t numArgs, const std::vector<std::string> &argv) {
    int retVal = SUCCESS;
    bool compile32 = false;
    bool compile64 = false;

    if (numArgs < 2) {
        showHelp = true;
        return INVALID_COMMAND_LINE;
    }

    for (uint32_t argIndex = 1; argIndex < numArgs; argIndex++) {
        const auto &currArg = argv[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < numArgs);
        if ("compile" == currArg) {
            //skip it
        } else if (("-file" == currArg) && hasMoreArgs) {
            inputFile = argv[argIndex + 1];
            argIndex++;
        } else if (("-output" == currArg) && hasMoreArgs) {
            outputFile = argv[argIndex + 1];
            argIndex++;
        } else if ((CompilerOptions::arch32bit == currArg) || ("-32" == currArg)) {
            compile32 = true;
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
        } else if ((CompilerOptions::arch64bit == currArg) || ("-64" == currArg)) {
            compile64 = true;
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch64bit);
        } else if (CompilerOptions::greaterThan4gbBuffersRequired == currArg) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
        } else if (("-device" == currArg) && hasMoreArgs) {
            deviceName = argv[argIndex + 1];
            argIndex++;
        } else if ("-llvm_text" == currArg) {
            useLlvmText = true;
        } else if ("-llvm_bc" == currArg) {
            useLlvmBc = true;
        } else if ("-llvm_input" == currArg) {
            inputFileLlvm = true;
        } else if ("-spirv_input" == currArg) {
            inputFileSpirV = true;
        } else if ("-cpp_file" == currArg) {
            useCppFile = true;
        } else if (("-options" == currArg) && hasMoreArgs) {
            options = argv[argIndex + 1];
            argIndex++;
        } else if (("-internal_options" == currArg) && hasMoreArgs) {
            CompilerOptions::concatenateAppend(internalOptions, argv[argIndex + 1]);
            argIndex++;
        } else if ("-options_name" == currArg) {
            useOptionsSuffix = true;
        } else if ("-force_stos_opt" == currArg) {
            forceStatelessToStatefulOptimization = true;
        } else if (("-out_dir" == currArg) && hasMoreArgs) {
            outputDirectory = argv[argIndex + 1];
            argIndex++;
        } else if ("-q" == currArg) {
            argHelper->getPrinterRef() = MessagePrinter(true);
            quiet = true;
        } else if ("-spv_only" == currArg) {
            onlySpirV = true;
        } else if ("-output_no_suffix" == currArg) {
            outputNoSuffix = true;
        } else if ("--help" == currArg) {
            showHelp = true;
            return SUCCESS;
        } else if (("-revision_id" == currArg) && hasMoreArgs) {
            revisionId = std::stoi(argv[argIndex + 1], nullptr, 0);
            argIndex++;
        } else {
            argHelper->printf("Invalid option (arg %d): %s\n", argIndex, argv[argIndex].c_str());
            retVal = INVALID_COMMAND_LINE;
            break;
        }
    }

    if (retVal == SUCCESS) {
        if (compile32 && compile64) {
            argHelper->printf("Error: Cannot compile for 32-bit and 64-bit, please choose one.\n");
            retVal |= INVALID_COMMAND_LINE;
        }

        if (deviceName.empty() && (false == onlySpirV)) {
            argHelper->printf("Error: Device name missing.\n");
            retVal = INVALID_COMMAND_LINE;
        }

        if (inputFile.empty()) {
            argHelper->printf("Error: Input file name missing.\n");
            retVal = INVALID_COMMAND_LINE;
        } else if (!argHelper->fileExists(inputFile)) {
            argHelper->printf("Error: Input file %s missing.\n", inputFile.c_str());
            retVal = INVALID_FILE;
        }
    }

    return retVal;
}

void OfflineCompiler::setStatelessToStatefullBufferOffsetFlag() {
    bool isStatelessToStatefulBufferOffsetSupported = true;
    if (deviceName == "bdw") {
        isStatelessToStatefulBufferOffsetSupported = false;
    }
    if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
        isStatelessToStatefulBufferOffsetSupported = DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
    }
    if (isStatelessToStatefulBufferOffsetSupported) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::hasBufferOffsetArg);
    }
}

void OfflineCompiler::parseDebugSettings() {
    setStatelessToStatefullBufferOffsetFlag();
    resolveExtraSettings();
}

std::string OfflineCompiler::parseBinAsCharArray(uint8_t *binary, size_t size, std::string &fileName) {
    std::string builtinName = convertToPascalCase(fileName);
    std::ostringstream out;

    // Convert binary to cpp
    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n\n";
    out << "size_t " << builtinName << "BinarySize_" << familyNameWithType << " = " << size << ";\n";
    out << "uint32_t " << builtinName << "Binary_" << familyNameWithType << "[" << (size + 3) / 4 << "] = {"
        << std::endl
        << "    ";

    uint32_t *binaryUint = (uint32_t *)binary;
    for (size_t i = 0; i < (size + 3) / 4; i++) {
        if (i != 0) {
            out << ", ";
            if (i % 8 == 0) {
                out << std::endl
                    << "    ";
            }
        }
        if (i < size / 4) {
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << binaryUint[i];
        } else {
            uint32_t lastBytes = size & 0x3;
            uint32_t lastUint = 0;
            uint8_t *pLastUint = (uint8_t *)&lastUint;
            for (uint32_t j = 0; j < lastBytes; j++) {
                pLastUint[sizeof(uint32_t) - 1 - j] = binary[i * 4 + j];
            }
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << lastUint;
        }
    }
    out << "};" << std::endl;

    out << std::endl
        << "#include \"shared/source/built_ins/registry/built_ins_registry.h\"\n"
        << std::endl;
    out << "namespace NEO {" << std::endl;
    out << "static RegisterEmbeddedResource register" << builtinName << "Bin(" << std::endl;
    out << "    \"" << familyNameWithType << "_0_" << fileName.c_str() << ".builtin_kernel.bin\"," << std::endl;
    out << "    (const char *)" << builtinName << "Binary_" << familyNameWithType << "," << std::endl;
    out << "    " << builtinName << "BinarySize_" << familyNameWithType << ");" << std::endl;
    out << "}" << std::endl;

    return out.str();
}

std::string OfflineCompiler::getFileNameTrunk(std::string &filePath) {
    size_t slashPos = filePath.find_last_of("\\/", filePath.size()) + 1;
    size_t extPos = filePath.find_last_of(".", filePath.size());
    if (extPos == std::string::npos) {
        extPos = filePath.size();
    }

    std::string fileTrunk = filePath.substr(slashPos, (extPos - slashPos));

    return fileTrunk;
}
//
std::string getDevicesTypes() {
    std::list<std::string> prefixes;
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (hardwarePrefix[j] == nullptr)
            continue;
        prefixes.push_back(hardwarePrefix[j]);
    }

    std::ostringstream os;
    for (auto it = prefixes.begin(); it != prefixes.end(); it++) {
        if (it != prefixes.begin())
            os << ", ";
        os << *it;
    }

    return os.str();
}

std::string getDevicesFamilies() {
    std::list<std::string> prefixes;
    for (unsigned int i = 0; i < IGFX_MAX_CORE; ++i) {
        if (familyName[i] == nullptr)
            continue;
        prefixes.push_back(familyName[i]);
    }

    std::ostringstream os;
    for (auto it = prefixes.begin(); it != prefixes.end(); it++) {
        if (it != prefixes.begin())
            os << ", ";
        os << *it;
    }

    return os.str();
}

void OfflineCompiler::printUsage() {
    argHelper->printf(R"===(Compiles input file to Intel Compute GPU device binary (*.bin).
Additionally, outputs intermediate representation (e.g. spirV).
Different input and intermediate file formats are available.

Usage: ocloc [compile] -file <filename> -device <device_type> [-output <filename>] [-out_dir <output_dir>] [-options <options>] [-32|-64] [-internal_options <options>] [-llvm_text|-llvm_input|-spirv_input] [-options_name] [-q] [-cpp_file] [-output_no_suffix] [--help]

  -file <filename>              The input file to be compiled
                                (by default input source format is
                                OpenCL C kernel language).

  -device <device_type>         Target device.
                                <device_type> can be: %s, %s or hexadecimal value with 0x prefix - can be single or multiple target devices.
                                The hexadecimal value represents device ID.
                                If such value is provided, ocloc will try to match it
                                with corresponding device type.
                                For example, 0xFF20 device ID will be translated to tgllp.
                                If multiple target devices are provided, ocloc
                                will compile for each of these targets and will
                                create a fatbinary archive that contains all of
                                device binaries produced this way.
                                Supported -device patterns examples:
                                -device 0xFF20     ; will compile 1 target (tgllp)
                                -device skl        ; will compile 1 target
                                -device skl,icllp  ; will compile 2 targets
                                -device skl-icllp  ; will compile all targets
                                                     in range (inclusive)
                                -device skl-       ; will compile all targets
                                                     newer/same as provided
                                -device -skl       ; will compile all targets
                                                     older/same as provided
                                -device gen9       ; will compile all targets
                                                     matching the same gen
                                -device gen9-gen11 ; will compile all targets
                                                     in range (inclusive)
                                -device gen9-      ; will compile all targets
                                                     newer/same as provided
                                -device -gen9      ; will compile all targets
                                                     older/same as provided
                                -device *          ; will compile all targets
                                                     known to ocloc

  -output <filename>            Optional output file base name.
                                Default is input file's base name.
                                This base name will be used for all output
                                files. Proper sufixes (describing file formats)
                                will be added automatically.

  -out_dir <output_dir>         Optional output directory.
                                Default is current working directory.

  -options <options>            Optional OpenCL C compilation options
                                as defined by OpenCL specification.
                                Special options for Vector Compute:
                                -vc-codegen <vc options> compile from SPIRV
                                -cmc <cm-options> compile from CM sources

  -32                           Forces target architecture to 32-bit pointers.
                                Default pointer size is inherited from
                                ocloc's pointer size.
                                This option is exclusive with -64.

  -64                           Forces target architecture to 64-bit pointers.
                                Default pointer size is inherited from
                                ocloc's pointer size.
                                This option is exclusive with -32.

  -internal_options <options>   Optional compiler internal options
                                as defined by compilers used underneath.
                                Check intel-graphics-compiler (IGC) project
                                for details on available internal options.
                                You also may provide explicit --help to inquire
                                information about option, mentioned in -options

  -llvm_text                    Forces intermediate representation (IR) format
                                to human-readable LLVM IR (.ll).
                                This option affects only output files
                                and should not be used in combination with
                                '-llvm_input' option.
                                Default IR is spirV.
                                This option is exclusive with -spirv_input.
                                This option is exclusive with -llvm_input.

  -llvm_input                   Indicates that input file is an llvm binary.
                                Default is OpenCL C kernel language.
                                This option is exclusive with -spirv_input.
                                This option is exclusive with -llvm_text.

  -spirv_input                  Indicates that input file is a spirV binary.
                                Default is OpenCL C kernel language format.
                                This option is exclusive with -llvm_input.
                                This option is exclusive with -llvm_text.

  -options_name                 Will add suffix to output files.
                                This suffix will be generated based on input
                                options (useful when rebuilding with different 
                                set of options so that results won't get
                                overwritten).
                                This suffix is added always as the last part
                                of the filename (even after file's extension).
                                It does not affect '--output' parameter and can
                                be used along with it ('--output' parameter
                                defines the base name - i.e. prefix).

  -force_stos_opt               Will forcibly enable stateless to stateful optimization,
                                i.e. skip "-cl-intel-greater-than-4GB-buffer-required".

  -q                            Will silence most of output messages.

  -spv_only                     Will generate only spirV file.

  -cpp_file                     Will generate c++ file with C-array
                                containing Intel Compute device binary.

  -output_no_suffix             Prevents ocloc from adding family name suffix.

  --help                        Print this usage message.

  -revision_id <revision_id>    Target stepping. Can be decimal or hexadecimal value.

Examples :
  Compile file to Intel Compute GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl
)===",
                      NEO::getDevicesTypes().c_str(),
                      NEO::getDevicesFamilies().c_str());
}

void OfflineCompiler::storeBinary(
    char *&pDst,
    size_t &dstSize,
    const void *pSrc,
    const size_t srcSize) {
    dstSize = 0;

    DEBUG_BREAK_IF(!(pSrc && srcSize > 0));

    delete[] pDst;
    pDst = new char[srcSize];

    dstSize = static_cast<uint32_t>(srcSize);
    memcpy_s(pDst, dstSize, pSrc, srcSize);
}

bool OfflineCompiler::generateElfBinary() {
    if (!genBinary || !genBinarySize) {
        return false;
    }

    // return "as is" if zebin format
    if (isDeviceBinaryFormat<DeviceBinaryFormat::Zebin>(ArrayRef<uint8_t>(reinterpret_cast<uint8_t *>(genBinary), genBinarySize))) {
        this->elfBinary = std::vector<uint8_t>(genBinary, genBinary + genBinarySize);
        return true;
    }

    SingleDeviceBinary binary = {};
    binary.buildOptions = this->options;
    binary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary), this->irBinarySize);
    binary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->genBinary), this->genBinarySize);
    binary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->debugDataBinary), this->debugDataBinarySize);
    std::string packErrors;
    std::string packWarnings;

    using namespace NEO::Elf;
    ElfEncoder<EI_CLASS_64> ElfEncoder;
    ElfEncoder.getElfFileHeader().type = ET_OPENCL_EXECUTABLE;
    if (binary.buildOptions.empty() == false) {
        ElfEncoder.appendSection(SHT_OPENCL_OPTIONS, SectionNamesOpenCl::buildOptions,
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.buildOptions.data()), binary.buildOptions.size()));
    }

    if (binary.intermediateRepresentation.empty() == false) {
        if (isSpirV) {
            ElfEncoder.appendSection(SHT_OPENCL_SPIRV, SectionNamesOpenCl::spirvObject, binary.intermediateRepresentation);
        } else {
            ElfEncoder.appendSection(SHT_OPENCL_LLVM_BINARY, SectionNamesOpenCl::llvmObject, binary.intermediateRepresentation);
        }
    }

    if (binary.debugData.empty() == false) {
        ElfEncoder.appendSection(SHT_OPENCL_DEV_DEBUG, SectionNamesOpenCl::deviceDebug, binary.debugData);
    }

    if (binary.deviceBinary.empty() == false) {
        ElfEncoder.appendSection(SHT_OPENCL_DEV_BINARY, SectionNamesOpenCl::deviceBinary, binary.deviceBinary);
    }

    this->elfBinary = ElfEncoder.encode();

    return true;
}

void OfflineCompiler::writeOutAllFiles() {
    std::string fileBase;
    std::string fileTrunk = getFileNameTrunk(inputFile);
    if (outputNoSuffix) {
        if (outputFile.empty()) {
            fileBase = fileTrunk;
        } else {
            fileBase = outputFile;
        }
    } else {
        if (outputFile.empty()) {
            fileBase = fileTrunk + "_" + familyNameWithType;
        } else {
            fileBase = outputFile + "_" + familyNameWithType;
        }
    }

    if (outputDirectory != "") {
        std::list<std::string> dirList;
        std::string tmp = outputDirectory;
        size_t pos = outputDirectory.size() + 1;

        do {
            dirList.push_back(tmp);
            pos = tmp.find_last_of("/\\", pos);
            tmp = tmp.substr(0, pos);
        } while (pos != std::string::npos);

        while (!dirList.empty()) {
            MakeDirectory(dirList.back().c_str());
            dirList.pop_back();
        }
    }

    if (irBinary && !inputFileSpirV) {
        std::string irOutputFileName = generateFilePathForIr(fileBase) + generateOptsSuffix();

        argHelper->saveOutput(irOutputFileName, irBinary, irBinarySize);
    }

    if (genBinary) {
        std::string genOutputFile = generateFilePath(outputDirectory, fileBase, ".gen") + generateOptsSuffix();

        argHelper->saveOutput(genOutputFile, genBinary, genBinarySize);

        if (useCppFile) {
            std::string cppOutputFile = generateFilePath(outputDirectory, fileBase, ".cpp");
            std::string cpp = parseBinAsCharArray((uint8_t *)genBinary, genBinarySize, fileTrunk);
            argHelper->saveOutput(cppOutputFile, cpp.c_str(), cpp.size());
        }
    }

    if (!elfBinary.empty()) {
        std::string elfOutputFile;
        if (outputNoSuffix) {
            elfOutputFile = generateFilePath(outputDirectory, fileBase, "");
        } else {
            elfOutputFile = generateFilePath(outputDirectory, fileBase, ".bin") + generateOptsSuffix();
        }
        argHelper->saveOutput(
            elfOutputFile,
            elfBinary.data(),
            elfBinary.size());
    }

    if (debugDataBinary) {
        std::string debugOutputFile = generateFilePath(outputDirectory, fileBase, ".dbg") + generateOptsSuffix();

        argHelper->saveOutput(
            debugOutputFile,
            debugDataBinary,
            debugDataBinarySize);
    }
}

bool OfflineCompiler::readOptionsFromFile(std::string &options, const std::string &file, OclocArgHelper *helper) {
    if (!helper->fileExists(file)) {
        return false;
    }
    size_t optionsSize = 0U;
    auto optionsFromFile = helper->loadDataFromFile(file, optionsSize);
    if (optionsSize > 0) {
        // Remove comment containing copyright header
        options = optionsFromFile.get();
        size_t commentBegin = options.find("/*");
        size_t commentEnd = options.rfind("*/");
        if (commentBegin != std::string::npos && commentEnd != std::string::npos) {
            auto sizeToReplace = commentEnd - commentBegin + 2;
            options = options.replace(commentBegin, sizeToReplace, "");
            size_t optionsBegin = options.find_first_not_of(" \t\n\r");
            if (optionsBegin != std::string::npos) {
                options = options.substr(optionsBegin, options.length());
            }
        }
        auto trimPos = options.find_last_not_of(" \n\r");
        options = options.substr(0, trimPos + 1);
    }
    return true;
}

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension) {
    UNRECOVERABLE_IF(extension == nullptr);

    if (directory.empty()) {
        return fileNameBase + extension;
    }

    bool hasTrailingSlash = (*directory.rbegin() == '/');
    std::string ret;
    ret.reserve(directory.size() + (hasTrailingSlash ? 0 : 1) + fileNameBase.size() + strlen(extension) + 1);
    ret.append(directory);
    if (false == hasTrailingSlash) {
        ret.append("/", 1);
    }
    ret.append(fileNameBase);
    ret.append(extension);

    return ret;
}

} // namespace NEO
