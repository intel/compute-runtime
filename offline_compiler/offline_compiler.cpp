/*
 * Copyright (c) 2018, Intel Corporation
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

#include "cif/common/cif_main.h"
#include "cif/helpers/error.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/platform_helper.h"
#include "offline_compiler.h"
#include "igfxfmid.h"
#include "runtime/helpers/file_io.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_inc_base.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/validators.h"
#include "runtime/platform/extensions.h"
#include "elf/writer.h"
#include <iomanip>
#include <list>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#define MakeDirectory _mkdir
#define GetCurrentWorkingDirectory _getcwd
#else
#include <sys/stat.h>
#define MakeDirectory(dir) mkdir(dir, 0777)
#define GetCurrentWorkingDirectory getcwd
#endif

namespace OCLRT {

CIF::CIFMain *createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc);

////////////////////////////////////////////////////////////////////////////////
// StringsAreEqual
////////////////////////////////////////////////////////////////////////////////
bool stringsAreEqual(const char *string1, const char *string2) {
    if (string2 == nullptr)
        return false;
    return (strcmp(string1, string2) == 0);
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
OfflineCompiler *OfflineCompiler::create(size_t numArgs, const char *const *argv, int &retVal) {
    retVal = CL_SUCCESS;
    auto pOffCompiler = new OfflineCompiler();

    if (pOffCompiler) {
        retVal = pOffCompiler->initialize(numArgs, argv);
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
        UNRECOVERABLE_IF(fclDeviceCtx == nullptr);
        UNRECOVERABLE_IF(igcDeviceCtx == nullptr);

        CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> igcOutput;
        bool inputIsIntermediateRepresentation = inputFileLlvm || inputFileSpirV;
        if (false == inputIsIntermediateRepresentation) {
            IGC::CodeType::CodeType_t intermediateRepresentation = useLlvmText ? IGC::CodeType::llvmLl : preferredIntermediateRepresentation;
            // sourceCode.size() returns the number of characters without null terminated char
            auto fclSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), sourceCode.c_str(), sourceCode.size() + 1);
            auto fclOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), options.c_str(), options.size());
            auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), internalOptions.c_str(), internalOptions.size());

            auto fclTranslationCtx = fclDeviceCtx->CreateTranslationCtx(IGC::CodeType::oclC, intermediateRepresentation);
            auto igcTranslationCtx = igcDeviceCtx->CreateTranslationCtx(intermediateRepresentation, IGC::CodeType::oclGenBin);

            if (false == OCLRT::areNotNullptr(fclSrc.get(), fclOptions.get(), fclInternalOptions.get(),
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
            auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), nullptr, 0);
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
                familyNameWithType.append(familyName[hwInfo->pPlatform->eRenderCoreFamily]);
                familyNameWithType.append(getPlatformType(*hwInfo));
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
int OfflineCompiler::initialize(size_t numArgs, const char *const *argv) {
    int retVal = CL_SUCCESS;
    const char *pSource = nullptr;
    void *pSourceFromFile = nullptr;
    size_t sourceFromFileSize = 0;

    retVal = parseCommandLine(numArgs, argv);
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

            bool internalOptionsRead = readOptionsFromFile(internalOptions, internalOptionsFileName);
            if (internalOptionsRead && !isQuiet()) {
                printf("Building with internal options:\n%s\n", internalOptions.c_str());
            }
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

    this->fclLib.reset(OsLibrary::load(Os::frontEndDllName));
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
        // given FCL is not compatible
        DEBUG_BREAK_IF(true);
        return CL_OUT_OF_HOST_MEMORY;
    }

    this->fclDeviceCtx = this->fclMain->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
    if (this->fclDeviceCtx == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    fclDeviceCtx->SetOclApiVersion(hwInfo->capabilityTable.clVersionSupport * 10);
    preferredIntermediateRepresentation = fclDeviceCtx->GetPreferredIntermediateRepresentation();

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

    if (false == this->igcMain->IsCompatible<IGC::IgcOclDeviceCtx>()) {
        // given IGC is not compatible
        DEBUG_BREAK_IF(true);
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
    IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform.get(), *hwInfo->pPlatform);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo.get(), *hwInfo->pSysInfo);
    // populate with features
    igcFeWa.get()->SetFtrDesktop(hwInfo->pSkuTable->ftrDesktop);
    igcFeWa.get()->SetFtrChannelSwizzlingXOREnabled(hwInfo->pSkuTable->ftrChannelSwizzlingXOREnabled);

    igcFeWa.get()->SetFtrGtBigDie(hwInfo->pSkuTable->ftrGtBigDie);
    igcFeWa.get()->SetFtrGtMediumDie(hwInfo->pSkuTable->ftrGtMediumDie);
    igcFeWa.get()->SetFtrGtSmallDie(hwInfo->pSkuTable->ftrGtSmallDie);

    igcFeWa.get()->SetFtrGT1(hwInfo->pSkuTable->ftrGT1);
    igcFeWa.get()->SetFtrGT1_5(hwInfo->pSkuTable->ftrGT1_5);
    igcFeWa.get()->SetFtrGT2(hwInfo->pSkuTable->ftrGT2);
    igcFeWa.get()->SetFtrGT3(hwInfo->pSkuTable->ftrGT3);
    igcFeWa.get()->SetFtrGT4(hwInfo->pSkuTable->ftrGT4);

    igcFeWa.get()->SetFtrIVBM0M1Platform(hwInfo->pSkuTable->ftrIVBM0M1Platform);
    igcFeWa.get()->SetFtrGTL(hwInfo->pSkuTable->ftrGT1);
    igcFeWa.get()->SetFtrGTM(hwInfo->pSkuTable->ftrGT2);
    igcFeWa.get()->SetFtrGTH(hwInfo->pSkuTable->ftrGT3);

    igcFeWa.get()->SetFtrSGTPVSKUStrapPresent(hwInfo->pSkuTable->ftrSGTPVSKUStrapPresent);
    igcFeWa.get()->SetFtrGTA(hwInfo->pSkuTable->ftrGTA);
    igcFeWa.get()->SetFtrGTC(hwInfo->pSkuTable->ftrGTC);
    igcFeWa.get()->SetFtrGTX(hwInfo->pSkuTable->ftrGTX);
    igcFeWa.get()->SetFtr5Slice(hwInfo->pSkuTable->ftr5Slice);

    igcFeWa.get()->SetFtrGpGpuMidThreadLevelPreempt(hwInfo->pSkuTable->ftrGpGpuMidThreadLevelPreempt);
    igcFeWa.get()->SetFtrIoMmuPageFaulting(hwInfo->pSkuTable->ftrIoMmuPageFaulting);
    igcFeWa.get()->SetFtrWddm2Svm(hwInfo->pSkuTable->ftrWddm2Svm);
    igcFeWa.get()->SetFtrPooledEuEnabled(hwInfo->pSkuTable->ftrPooledEuEnabled);

    igcFeWa.get()->SetFtrResourceStreamer(hwInfo->pSkuTable->ftrResourceStreamer);

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// ParseCommandLine
////////////////////////////////////////////////////////////////////////////////
int OfflineCompiler::parseCommandLine(size_t numArgs, const char *const *argv) {
    int retVal = CL_SUCCESS;
    bool compile32 = false;
    bool compile64 = false;

    if (numArgs < 2) {
        printUsage();
        retVal = PRINT_USAGE;
    }

    for (uint32_t argIndex = 1; argIndex < numArgs; argIndex++) {
        if ((stringsAreEqual(argv[argIndex], "-file")) &&
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
        } else if (stringsAreEqual(argv[argIndex], "-?")) {
            printUsage();
            retVal = PRINT_USAGE;
        } else {
            printf("Invalid option (arg %d): %s\n", argIndex, argv[argIndex]);
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
            }
            std::string extensionsList = getExtensionsList(*hwInfo);
            internalOptions.append(convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str()));
        }
    }

    return retVal;
}

////////////////////////////////////////////////////////////////////////////////
// ParseCommandLine
////////////////////////////////////////////////////////////////////////////////
void OfflineCompiler::parseDebugSettings() {
    if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get()) {
        internalOptions += "-cl-intel-has-buffer-offset-arg ";
    }
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
    out << "namespace OCLRT {" << std::endl;
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

    printf("Compiles CL files into llvm (.bc or .ll), gen isa (.gen), and binary files (.bin)\n\n");
    printf("cloc -file <filename> -device <device_type> [OPTIONS]\n\n");
    printf("  -file <filename>             Indicates the CL kernel file to be compiled.\n");
    printf("  -device <device_type>        Indicates which device for which we will compile.\n");
    printf("                               <device_type> can be: %s\n", getDevicesTypes().c_str());
    printf("\n");
    printf("  -output <filename>           Indicates output files core name.\n");
    printf("  -out_dir <output_dir>        Indicates the directory into which the compiled files\n");
    printf("                               will be placed.\n");
    printf("  -cpp_file                    Cpp file with scheduler program binary will be generated.\n");
    printf("\n");
    printf("  -32                          Force compile to 32-bit binary.\n");
    printf("  -64                          Force compile to 64-bit binary.\n");
    printf("  -internal_options <options>  Compiler internal options.\n");
    printf("  -llvm_text                   Readable LLVM text will be output in a .ll file instead of\n");
    printf("                               through the default lllvm binary (.bc) file.\n");
    printf("  -llvm_input                  Indicates input file is llvm source\n");
    printf("  -spirv_input                  Indicates input file is a SpirV binary\n");
    printf("  -options <options>           Compiler options.\n");
    printf("  -options_name                Add suffix with compile options to filename\n");
    printf("  -q                           Be more quiet. print only warnings and errors.\n");
    printf("  -?                           Print this usage message.\n");
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

} // namespace OCLRT
