/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler.h"

#include "core/helpers/string.h"
#include "elf/writer.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/validators.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_inc_base.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/platform/extensions.h"

#include "cif/common/cif_main.h"
#include "cif/helpers/error.h"
#include "cif/import/library_api.h"
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

////////////////////////////////////////////////////////////////////////////////
// StringsAreEqual
////////////////////////////////////////////////////////////////////////////////
bool stringsAreEqual(const char *string1, const char *string2) {
    if (string2 == nullptr)
        return false;
    return (strcmp(string1, string2) == 0);
}

bool stringsAreEqual(std::string string1, std::string string2) {
    if (string2.empty())
        return false;
    return (string1 == string2);
}

////////////////////////////////////////////////////////////////////////////////
// convertToPascalCase
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// ctor
////////////////////////////////////////////////////////////////////////////////
OfflineCompiler::OfflineCompiler() = default;

////////////////////////////////////////////////////////////////////////////////
// dtor
////////////////////////////////////////////////////////////////////////////////
OfflineCompiler::~OfflineCompiler() {
    delete[] irBinary;
    delete[] genBinary;
}

////////////////////////////////////////////////////////////////////////////////
// Create
////////////////////////////////////////////////////////////////////////////////
OfflineCompiler *OfflineCompiler::create(size_t numArgs, const std::vector<std::string> &allArgs, int &retVal) {
    retVal = CL_SUCCESS;
    auto pOffCompiler = new OfflineCompiler();

    if (pOffCompiler) {
        retVal = pOffCompiler->initialize(numArgs, allArgs);
    }

    if (retVal != CL_SUCCESS) {
        delete pOffCompiler;
        pOffCompiler = nullptr;
    }

    return pOffCompiler;
}

////////////////////////////////////////////////////////////////////////////////
// buildSourceCode
////////////////////////////////////////////////////////////////////////////////
int OfflineCompiler::buildSourceCode() {
    int retVal = CL_SUCCESS;

    do {
        if (strcmp(sourceCode.c_str(), "") == 0) {
            retVal = CL_INVALID_PROGRAM;
            break;
        }
        UNRECOVERABLE_IF(igcDeviceCtx == nullptr);

        CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> igcOutput;
        bool inputIsIntermediateRepresentation = inputFileLlvm || inputFileSpirV;
        if (false == inputIsIntermediateRepresentation) {
            UNRECOVERABLE_IF(fclDeviceCtx == nullptr);
            IGC::CodeType::CodeType_t intermediateRepresentation = useLlvmText ? IGC::CodeType::llvmLl : preferredIntermediateRepresentation;
            // sourceCode.size() returns the number of characters without null terminated char
            auto fclSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), sourceCode.c_str(), sourceCode.size() + 1);
            auto fclOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), options.c_str(), options.size());
            auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), internalOptions.c_str(), internalOptions.size());

            auto fclTranslationCtx = fclDeviceCtx->CreateTranslationCtx(IGC::CodeType::oclC, intermediateRepresentation);
            auto igcTranslationCtx = igcDeviceCtx->CreateTranslationCtx(intermediateRepresentation, IGC::CodeType::oclGenBin);

            if (false == NEO::areNotNullptr(fclSrc.get(), fclOptions.get(), fclInternalOptions.get(),
                                            fclTranslationCtx.get(), igcTranslationCtx.get())) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            auto fclOutput = fclTranslationCtx->Translate(fclSrc.get(), fclOptions.get(),
                                                          fclInternalOptions.get(), nullptr, 0);

            if (fclOutput == nullptr) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            UNRECOVERABLE_IF(fclOutput->GetBuildLog() == nullptr);
            UNRECOVERABLE_IF(fclOutput->GetOutput() == nullptr);

            if (fclOutput->Successful() == false) {
                updateBuildLog(fclOutput->GetBuildLog()->GetMemory<char>(), fclOutput->GetBuildLog()->GetSizeRaw());
                retVal = CL_BUILD_PROGRAM_FAILURE;
                break;
            }

            storeBinary(irBinary, irBinarySize, fclOutput->GetOutput()->GetMemory<char>(), fclOutput->GetOutput()->GetSizeRaw());
            isSpirV = intermediateRepresentation == IGC::CodeType::spirV;
            updateBuildLog(fclOutput->GetBuildLog()->GetMemory<char>(), fclOutput->GetBuildLog()->GetSizeRaw());

            igcOutput = igcTranslationCtx->Translate(fclOutput->GetOutput(), fclOptions.get(),
                                                     fclInternalOptions.get(),
                                                     nullptr, 0);

        } else {
            auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), sourceCode.c_str(), sourceCode.size());
            auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), options.c_str(), options.size());
            auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), internalOptions.c_str(), internalOptions.size());
            auto igcTranslationCtx = igcDeviceCtx->CreateTranslationCtx(inputFileSpirV ? IGC::CodeType::spirV : IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin);
            igcOutput = igcTranslationCtx->Translate(igcSrc.get(), igcOptions.get(), igcInternalOptions.get(), nullptr, 0);
        }
        if (igcOutput == nullptr) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }
        UNRECOVERABLE_IF(igcOutput->GetBuildLog() == nullptr);
        UNRECOVERABLE_IF(igcOutput->GetOutput() == nullptr);
        storeBinary(genBinary, genBinarySize, igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetSizeRaw());
        updateBuildLog(igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());

        if (igcOutput->GetDebugData()->GetSizeRaw() != 0) {
            storeBinary(debugDataBinary, debugDataBinarySize, igcOutput->GetDebugData()->GetMemory<char>(), igcOutput->GetDebugData()->GetSizeRaw());
        }
        retVal = igcOutput->Successful() ? CL_SUCCESS : CL_BUILD_PROGRAM_FAILURE;
    } while (0);

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// build
////////////////////////////////////////////////////////////////////////////////
int OfflineCompiler::build() {
    int retVal = CL_SUCCESS;

    retVal = buildSourceCode();

    if (retVal == CL_SUCCESS) {
        generateElfBinary();
        writeOutAllFiles();
    }

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// updateBuildLog
////////////////////////////////////////////////////////////////////////////////
void OfflineCompiler::updateBuildLog(const char *pErrorString, const size_t errorStringSize) {
    std::string errorString = (errorStringSize && pErrorString) ? std::string(pErrorString, pErrorString + errorStringSize) : "";
    if (errorString[0] != '\0') {
        if (buildLog.empty()) {
            buildLog.assign(errorString);
        } else {
            buildLog.append("\n" + errorString);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// getBuildLog
////////////////////////////////////////////////////////////////////////////////
std::string &OfflineCompiler::getBuildLog() {
    return buildLog;
}

////////////////////////////////////////////////////////////////////////////////
// getHardwareInfo
////////////////////////////////////////////////////////////////////////////////
int OfflineCompiler::getHardwareInfo(const char *pDeviceName) {
    int retVal = CL_INVALID_DEVICE;

    for (unsigned int productId = 0; productId < IGFX_MAX_PRODUCT; ++productId) {
        if (stringsAreEqual(pDeviceName, hardwarePrefix[productId])) {
            if (hardwareInfoTable[productId]) {
                hwInfo = hardwareInfoTable[productId];
                familyNameWithType.clear();
                familyNameWithType.append(familyName[hwInfo->platform.eRenderCoreFamily]);
                familyNameWithType.append(hwInfo->capabilityTable.platformType);
                retVal = CL_SUCCESS;
                break;
            }
        }
    }

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// getStringWithinDelimiters
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Initialize
////////////////////////////////////////////////////////////////////////////////
int OfflineCompiler::initialize(size_t numArgs, const std::vector<std::string> &allArgs) {
    int retVal = CL_SUCCESS;
    const char *pSource = nullptr;
    void *pSourceFromFile = nullptr;
    size_t sourceFromFileSize = 0;

    retVal = parseCommandLine(numArgs, allArgs);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    parseDebugSettings();

    if (options.empty()) {
        // try to read options from file if not provided by commandline
        size_t ext_start = inputFile.find(".cl");
        if (ext_start != std::string::npos) {
            std::string optionsFileName = inputFile.substr(0, ext_start);
            optionsFileName.append("_options.txt");

            bool optionsRead = readOptionsFromFile(options, optionsFileName);
            if (optionsRead && !isQuiet()) {
                printf("Building with options:\n%s\n", options.c_str());
            }

            std::string internalOptionsFileName = inputFile.substr(0, ext_start);
            internalOptionsFileName.append("_internal_options.txt");

            std::string internalOptionsFromFile;
            bool internalOptionsRead = readOptionsFromFile(internalOptionsFromFile, internalOptionsFileName);
            if (internalOptionsRead && !isQuiet()) {
                printf("Building with internal options:\n%s\n", internalOptionsFromFile.c_str());
            }
            internalOptions.append(internalOptionsFromFile);
        }
    }

    // set up the device inside the program
    sourceFromFileSize = loadDataFromFile(inputFile.c_str(), pSourceFromFile);
    struct Helper {
        static void deleter(void *ptr) { deleteDataReadFromFile(ptr); }
    };
    auto sourceRaii = std::unique_ptr<void, decltype(&Helper::deleter)>{pSourceFromFile, Helper::deleter};
    if (sourceFromFileSize == 0) {
        retVal = INVALID_FILE;
        return retVal;
    }

    if (inputFileLlvm || inputFileSpirV) {
        // use the binary input "as is"
        sourceCode.assign(reinterpret_cast<char *>(pSourceFromFile), sourceFromFileSize);
    } else {
        // for text input, we also accept files used as runtime builtins
        pSource = strstr((const char *)pSourceFromFile, "R\"===(");
        sourceCode = (pSource != nullptr) ? getStringWithinDelimiters((char *)pSourceFromFile) : (char *)pSourceFromFile;
    }

    if ((inputFileSpirV == false) && (inputFileLlvm == false)) {
        auto fclLibFile = OsLibrary::load(Os::frontEndDllName);
        if (fclLibFile == nullptr) {
            printf("Error: Failed to load %s\n", Os::frontEndDllName);
            return CL_OUT_OF_HOST_MEMORY;
        }

        this->fclLib.reset(fclLibFile);
        if (this->fclLib == nullptr) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        auto fclCreateMain = reinterpret_cast<CIF::CreateCIFMainFunc_t>(this->fclLib->getProcAddress(CIF::CreateCIFMainFuncName));
        if (fclCreateMain == nullptr) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        this->fclMain = CIF::RAII::UPtr(createMainNoSanitize(fclCreateMain));
        if (this->fclMain == nullptr) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        if (false == this->fclMain->IsCompatible<IGC::FclOclDeviceCtx>()) {
            printf("Incompatible interface in FCL : %s\n", CIF::InterfaceIdCoder::Dec(this->fclMain->FindIncompatible<IGC::FclOclDeviceCtx>()).c_str());
            DEBUG_BREAK_IF(true);
            return CL_OUT_OF_HOST_MEMORY;
        }

        this->fclDeviceCtx = this->fclMain->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
        if (this->fclDeviceCtx == nullptr) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        fclDeviceCtx->SetOclApiVersion(hwInfo->capabilityTable.clVersionSupport * 10);
        preferredIntermediateRepresentation = fclDeviceCtx->GetPreferredIntermediateRepresentation();
    } else {
        if (!isQuiet()) {
            printf("Compilation from IR - skipping loading of FCL\n");
        }
        preferredIntermediateRepresentation = IGC::CodeType::spirV;
    }

    this->igcLib.reset(OsLibrary::load(Os::igcDllName));
    if (this->igcLib == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    auto igcCreateMain = reinterpret_cast<CIF::CreateCIFMainFunc_t>(this->igcLib->getProcAddress(CIF::CreateCIFMainFuncName));
    if (igcCreateMain == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    this->igcMain = CIF::RAII::UPtr(createMainNoSanitize(igcCreateMain));
    if (this->igcMain == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    std::vector<CIF::InterfaceId_t> interfacesToIgnore = {IGC::OclGenBinaryBase::GetInterfaceId()};
    if (false == this->igcMain->IsCompatible<IGC::IgcOclDeviceCtx>(&interfacesToIgnore)) {
        printf("Incompatible interface in IGC : %s\n", CIF::InterfaceIdCoder::Dec(this->igcMain->FindIncompatible<IGC::IgcOclDeviceCtx>(&interfacesToIgnore)).c_str());
        DEBUG_BREAK_IF(true);
        return CL_OUT_OF_HOST_MEMORY;
    }

    CIF::Version_t verMin = 0, verMax = 0;
    if (false == this->igcMain->FindSupportedVersions<IGC::IgcOclDeviceCtx>(IGC::OclGenBinaryBase::GetInterfaceId(), verMin, verMax)) {
        printf("Patchtoken interface is missing");
        return CL_OUT_OF_HOST_MEMORY;
    }

    this->igcDeviceCtx = this->igcMain->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
    if (this->igcDeviceCtx == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }
    this->igcDeviceCtx->SetProfilingTimerResolution(static_cast<float>(hwInfo->capabilityTable.defaultProfilingTimerResolution));
    auto igcPlatform = this->igcDeviceCtx->GetPlatformHandle();
    auto igcGtSystemInfo = this->igcDeviceCtx->GetGTSystemInfoHandle();
    auto igcFeWa = this->igcDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle();
    if ((igcPlatform == nullptr) || (igcGtSystemInfo == nullptr) || (igcFeWa == nullptr)) {
        return CL_OUT_OF_HOST_MEMORY;
    }
    IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform.get(), hwInfo->platform);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo.get(), hwInfo->gtSystemInfo);
    // populate with features
    igcFeWa.get()->SetFtrDesktop(hwInfo->featureTable.ftrDesktop);
    igcFeWa.get()->SetFtrChannelSwizzlingXOREnabled(hwInfo->featureTable.ftrChannelSwizzlingXOREnabled);

    igcFeWa.get()->SetFtrGtBigDie(hwInfo->featureTable.ftrGtBigDie);
    igcFeWa.get()->SetFtrGtMediumDie(hwInfo->featureTable.ftrGtMediumDie);
    igcFeWa.get()->SetFtrGtSmallDie(hwInfo->featureTable.ftrGtSmallDie);

    igcFeWa.get()->SetFtrGT1(hwInfo->featureTable.ftrGT1);
    igcFeWa.get()->SetFtrGT1_5(hwInfo->featureTable.ftrGT1_5);
    igcFeWa.get()->SetFtrGT2(hwInfo->featureTable.ftrGT2);
    igcFeWa.get()->SetFtrGT3(hwInfo->featureTable.ftrGT3);
    igcFeWa.get()->SetFtrGT4(hwInfo->featureTable.ftrGT4);

    igcFeWa.get()->SetFtrIVBM0M1Platform(hwInfo->featureTable.ftrIVBM0M1Platform);
    igcFeWa.get()->SetFtrGTL(hwInfo->featureTable.ftrGT1);
    igcFeWa.get()->SetFtrGTM(hwInfo->featureTable.ftrGT2);
    igcFeWa.get()->SetFtrGTH(hwInfo->featureTable.ftrGT3);

    igcFeWa.get()->SetFtrSGTPVSKUStrapPresent(hwInfo->featureTable.ftrSGTPVSKUStrapPresent);
    igcFeWa.get()->SetFtrGTA(hwInfo->featureTable.ftrGTA);
    igcFeWa.get()->SetFtrGTC(hwInfo->featureTable.ftrGTC);
    igcFeWa.get()->SetFtrGTX(hwInfo->featureTable.ftrGTX);
    igcFeWa.get()->SetFtr5Slice(hwInfo->featureTable.ftr5Slice);

    igcFeWa.get()->SetFtrGpGpuMidThreadLevelPreempt(hwInfo->featureTable.ftrGpGpuMidThreadLevelPreempt);
    igcFeWa.get()->SetFtrIoMmuPageFaulting(hwInfo->featureTable.ftrIoMmuPageFaulting);
    igcFeWa.get()->SetFtrWddm2Svm(hwInfo->featureTable.ftrWddm2Svm);
    igcFeWa.get()->SetFtrPooledEuEnabled(hwInfo->featureTable.ftrPooledEuEnabled);

    igcFeWa.get()->SetFtrResourceStreamer(hwInfo->featureTable.ftrResourceStreamer);

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// ParseCommandLine
////////////////////////////////////////////////////////////////////////////////
int OfflineCompiler::parseCommandLine(size_t numArgs, const std::vector<std::string> &argv) {
    int retVal = CL_SUCCESS;
    bool compile32 = false;
    bool compile64 = false;

    if (numArgs < 2) {
        printUsage();
        retVal = PRINT_USAGE;
    }

    for (uint32_t argIndex = 1; argIndex < numArgs; argIndex++) {
        if (stringsAreEqual(argv[argIndex], "compile")) {
            //skip it
        } else if ((stringsAreEqual(argv[argIndex], "-file")) &&
                   (argIndex + 1 < numArgs)) {
            inputFile = argv[argIndex + 1];
            argIndex++;
        } else if ((stringsAreEqual(argv[argIndex], "-output")) &&
                   (argIndex + 1 < numArgs)) {
            outputFile = argv[argIndex + 1];
            argIndex++;
        } else if (stringsAreEqual(argv[argIndex], "-32")) {
            compile32 = true;
            internalOptions.append(" -m32 ");
        } else if (stringsAreEqual(argv[argIndex], "-64")) {
            compile64 = true;
            internalOptions.append(" -m64 ");
        } else if (stringsAreEqual(argv[argIndex], "-cl-intel-greater-than-4GB-buffer-required")) {
            internalOptions.append(" -cl-intel-greater-than-4GB-buffer-required ");
        } else if ((stringsAreEqual(argv[argIndex], "-device")) &&
                   (argIndex + 1 < numArgs)) {
            deviceName = argv[argIndex + 1];
            argIndex++;
        } else if (stringsAreEqual(argv[argIndex], "-llvm_text")) {
            useLlvmText = true;
        } else if (stringsAreEqual(argv[argIndex], "-llvm_input")) {
            inputFileLlvm = true;
        } else if (stringsAreEqual(argv[argIndex], "-spirv_input")) {
            inputFileSpirV = true;
        } else if (stringsAreEqual(argv[argIndex], "-cpp_file")) {
            useCppFile = true;
        } else if ((stringsAreEqual(argv[argIndex], "-options")) &&
                   (argIndex + 1 < numArgs)) {
            options = argv[argIndex + 1];
            argIndex++;
        } else if ((stringsAreEqual(argv[argIndex], "-internal_options")) &&
                   (argIndex + 1 < numArgs)) {
            internalOptions.append(argv[argIndex + 1]);
            argIndex++;
        } else if (stringsAreEqual(argv[argIndex], "-options_name")) {
            useOptionsSuffix = true;
        } else if ((stringsAreEqual(argv[argIndex], "-out_dir")) &&
                   (argIndex + 1 < numArgs)) {
            outputDirectory = argv[argIndex + 1];
            argIndex++;
        } else if (stringsAreEqual(argv[argIndex], "-q")) {
            quiet = true;
        } else if (stringsAreEqual(argv[argIndex], "--help")) {
            printUsage();
            retVal = PRINT_USAGE;
        } else {
            printf("Invalid option (arg %d): %s\n", argIndex, argv[argIndex].c_str());
            retVal = INVALID_COMMAND_LINE;
            break;
        }
    }

    if (retVal == CL_SUCCESS) {
        if (compile32 && compile64) {
            printf("Error: Cannot compile for 32-bit and 64-bit, please choose one.\n");
            retVal = INVALID_COMMAND_LINE;
        } else if (inputFile.empty()) {
            printf("Error: Input file name missing.\n");
            retVal = INVALID_COMMAND_LINE;
        } else if (deviceName.empty()) {
            printf("Error: Device name missing.\n");
            retVal = INVALID_COMMAND_LINE;
        } else if (!fileExists(inputFile)) {
            printf("Error: Input file %s missing.\n", inputFile.c_str());
            retVal = INVALID_FILE;
        } else {
            retVal = getHardwareInfo(deviceName.c_str());
            if (retVal != CL_SUCCESS) {
                printf("Error: Cannot get HW Info for device %s.\n", deviceName.c_str());
            } else {
                std::string extensionsList = getExtensionsList(*hwInfo);
                internalOptions.append(convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str()));
            }
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
        internalOptions += "-cl-intel-has-buffer-offset-arg ";
    }
}

void OfflineCompiler::parseDebugSettings() {
    setStatelessToStatefullBufferOffsetFlag();
}

////////////////////////////////////////////////////////////////////////////////
// ParseBinAsCharArray
////////////////////////////////////////////////////////////////////////////////
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
        << "#include \"runtime/built_ins/registry/built_ins_registry.h\"\n"
        << std::endl;
    out << "namespace NEO {" << std::endl;
    out << "static RegisterEmbeddedResource register" << builtinName << "Bin(" << std::endl;
    out << "    createBuiltinResourceName(" << std::endl;
    out << "        EBuiltInOps::" << builtinName << "," << std::endl;
    out << "        BuiltinCode::getExtension(BuiltinCode::ECodeType::Binary), \"" << familyNameWithType << "\", 0)" << std::endl;
    out << "        .c_str()," << std::endl;
    out << "    (const char *)" << builtinName << "Binary"
        << "_" << familyNameWithType << "," << std::endl;
    out << "    " << builtinName << "BinarySize_" << familyNameWithType << ");" << std::endl;
    out << "}" << std::endl;

    return out.str();
}

////////////////////////////////////////////////////////////////////////////////
// GetFileNameTrunk
////////////////////////////////////////////////////////////////////////////////
std::string OfflineCompiler::getFileNameTrunk(std::string &filePath) {
    size_t slashPos = filePath.find_last_of("\\/", filePath.size()) + 1;
    size_t extPos = filePath.find_last_of(".", filePath.size());
    if (extPos == std::string::npos) {
        extPos = filePath.size();
    }

    std::string fileName;
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
            os << ",";
        os << *it;
    }

    return os.str();
}
////////////////////////////////////////////////////////////////////////////////
// PrintUsage
////////////////////////////////////////////////////////////////////////////////
void OfflineCompiler::printUsage() {
    printf(R"===(Compiles input file to Intel OpenCL GPU device binary (*.bin).
Additionally, outputs intermediate representation (e.g. spirV).
Different input and intermediate file formats are available.

Usage: ocloc [compile] -file <filename> -device <device_type> [-output <filename>] [-out_dir <output_dir>] [-options <options>] [-32|-64] [-internal_options <options>] [-llvm_text|-llvm_input|-spirv_input] [-options_name] [-q] [-cpp_file] [--help]

  -file <filename>              The input file to be compiled
                                (by default input source format is
                                OpenCL C kernel language).
                                
  -device <device_type>         Target device.
                                <device_type> can be: %s

  -output <filename>            Optional output file base name.
                                Default is input file's base name.
                                This base name will be used for all output
                                files. Proper sufixes (describing file formats)
                                will be added automatically.

  -out_dir <output_dir>         Optional output directory.
                                Default is current working directory.

  -options <options>            Optional OpenCL C compilation options
                                as defined by OpenCL specification.

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

  -q                            Will silence most of output messages.

  -cpp_file                     Will generate c++ file with C-array
                                containing Intel OpenCL device binary.

  --help                        Print this usage message.

Examples :
  Compile file to Intel OpenCL GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl
)===",
           NEO::getDevicesTypes().c_str());
}

////////////////////////////////////////////////////////////////////////////////
// StoreBinary
////////////////////////////////////////////////////////////////////////////////
void OfflineCompiler::storeBinary(
    char *&pDst,
    size_t &dstSize,
    const void *pSrc,
    const size_t srcSize) {
    dstSize = 0;

    DEBUG_BREAK_IF(!(pSrc && srcSize > 0));

    delete[] pDst;
    pDst = new char[srcSize];

    dstSize = (cl_uint)srcSize;
    memcpy_s(pDst, dstSize, pSrc, srcSize);
}

////////////////////////////////////////////////////////////////////////////////
// GenerateElfBinary
////////////////////////////////////////////////////////////////////////////////
bool OfflineCompiler::generateElfBinary() {
    bool retVal = true;

    if (!genBinary || !genBinarySize) {
        retVal = false;
    }

    if (retVal) {
        CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

        elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "BuildOptions", options, static_cast<uint32_t>(strlen(options.c_str()) + 1u)));
        std::string irBinaryTemp = irBinary ? std::string(irBinary, irBinarySize) : "";
        elfWriter.addSection(CLElfLib::SSectionNode(isSpirV ? CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV : CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "Intel(R) OpenCL LLVM Object", std::move(irBinaryTemp), static_cast<uint32_t>(irBinarySize)));

        // Add the device binary if it exists
        if (genBinary) {
            std::string genBinaryTemp = genBinary ? std::string(genBinary, genBinarySize) : "";
            elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "Intel(R) OpenCL Device Binary", std::move(genBinaryTemp), static_cast<uint32_t>(genBinarySize)));
        }

        elfBinarySize = elfWriter.getTotalBinarySize();
        elfBinary.resize(elfBinarySize);
        elfWriter.resolveBinary(elfBinary);
    }

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// WriteOutAllFiles
////////////////////////////////////////////////////////////////////////////////
void OfflineCompiler::writeOutAllFiles() {
    std::string fileBase;
    std::string fileTrunk = getFileNameTrunk(inputFile);
    if (outputFile.empty()) {
        fileBase = fileTrunk + "_" + familyNameWithType;
    } else {
        fileBase = outputFile + "_" + familyNameWithType;
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

    if (irBinary) {
        std::string irOutputFileName = generateFilePathForIr(fileBase) + generateOptsSuffix();

        writeDataToFile(
            irOutputFileName.c_str(),
            irBinary,
            irBinarySize);
    }

    if (genBinary) {
        std::string genOutputFile = generateFilePath(outputDirectory, fileBase, ".gen") + generateOptsSuffix();

        writeDataToFile(
            genOutputFile.c_str(),
            genBinary,
            genBinarySize);

        if (useCppFile) {
            std::string cppOutputFile = generateFilePath(outputDirectory, fileBase, ".cpp");
            std::string cpp = parseBinAsCharArray((uint8_t *)genBinary, genBinarySize, fileTrunk);
            writeDataToFile(cppOutputFile.c_str(), cpp.c_str(), cpp.size());
        }
    }

    if (!elfBinary.empty()) {
        std::string elfOutputFile = generateFilePath(outputDirectory, fileBase, ".bin") + generateOptsSuffix();

        writeDataToFile(
            elfOutputFile.c_str(),
            elfBinary.data(),
            elfBinarySize);
    }

    if (debugDataBinary) {
        std::string debugOutputFile = generateFilePath(outputDirectory, fileBase, ".dbg") + generateOptsSuffix();

        writeDataToFile(
            debugOutputFile.c_str(),
            debugDataBinary,
            debugDataBinarySize);
    }
}

bool OfflineCompiler::readOptionsFromFile(std::string &options, const std::string &file) {
    if (!fileExists(file)) {
        return false;
    }
    void *pOptions = nullptr;
    size_t optionsSize = loadDataFromFile(file.c_str(), pOptions);
    if (optionsSize > 0) {
        // Remove comment containing copyright header
        options = (char *)pOptions;
        size_t commentBegin = options.find_first_of("/*");
        size_t commentEnd = options.find_last_of("*/");
        if (commentBegin != std::string::npos && commentEnd != std::string::npos) {
            options = options.replace(commentBegin, commentEnd - commentBegin + 1, "");
            size_t optionsBegin = options.find_first_not_of(" \t\n\r");
            if (optionsBegin != std::string::npos) {
                options = options.substr(optionsBegin, options.length());
            }
        }
        auto trimPos = options.find_last_not_of(" \n\r");
        options = options.substr(0, trimPos + 1);
    }
    deleteDataReadFromFile(pOptions);
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
