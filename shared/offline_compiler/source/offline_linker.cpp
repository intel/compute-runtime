/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_linker.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_library.h"

#include "cif/common/cif_main.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/platform_helper.h"

#include <algorithm>
#include <array>

namespace NEO {

CIF::CIFMain *createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc);

std::unique_ptr<OfflineLinker> OfflineLinker::create(size_t argsCount, const std::vector<std::string> &args, int &errorCode, OclocArgHelper *argHelper) {
    std::unique_ptr<OfflineLinker> linker{new OfflineLinker{argHelper, std::make_unique<OclocIgcFacade>(argHelper)}};
    errorCode = linker->initialize(argsCount, args);

    return linker;
}

OfflineLinker::OfflineLinker(OclocArgHelper *argHelper, std::unique_ptr<OclocIgcFacade> igcFacade)
    : argHelper{argHelper}, operationMode{OperationMode::SKIP_EXECUTION}, outputFilename{"linker_output"}, outputFormat{IGC::CodeType::llvmBc}, igcFacade{std::move(igcFacade)} {}

OfflineLinker::~OfflineLinker() = default;

int OfflineLinker::initialize(size_t argsCount, const std::vector<std::string> &args) {
    const auto parsingResult{parseCommand(argsCount, args)};
    if (parsingResult != OclocErrorCode::SUCCESS) {
        return parsingResult;
    }

    // If a user requested help, then stop here.
    if (operationMode == OperationMode::SHOW_HELP) {
        return OclocErrorCode::SUCCESS;
    }

    const auto verificationResult{verifyLinkerCommand()};
    if (verificationResult != OclocErrorCode::SUCCESS) {
        return verificationResult;
    }

    const auto loadingResult{loadInputFilesContent()};
    if (loadingResult != OclocErrorCode::SUCCESS) {
        return loadingResult;
    }

    const auto hwInfoInitializationResult{initHardwareInfo()};
    if (hwInfoInitializationResult != OclocErrorCode::SUCCESS) {
        return hwInfoInitializationResult;
    }

    const auto igcPreparationResult{igcFacade->initialize(hwInfo)};
    if (igcPreparationResult != OclocErrorCode::SUCCESS) {
        return igcPreparationResult;
    }

    operationMode = OperationMode::LINK_FILES;
    return OclocErrorCode::SUCCESS;
}

int OfflineLinker::parseCommand(size_t argsCount, const std::vector<std::string> &args) {
    if (argsCount < 2u) {
        operationMode = OperationMode::SHOW_HELP;
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }

    for (size_t argIndex = 1u; argIndex < argsCount; ++argIndex) {
        const auto &currentArg{args[argIndex]};
        const auto hasMoreArgs{argIndex + 1 < argsCount};

        if (currentArg == "link") {
            continue;
        } else if ((currentArg == "-file") && hasMoreArgs) {
            inputFilenames.push_back(args[argIndex + 1]);
            ++argIndex;
        } else if (currentArg == "-out" && hasMoreArgs) {
            outputFilename = args[argIndex + 1];
            ++argIndex;
        } else if ((currentArg == "-out_format") && hasMoreArgs) {
            outputFormat = parseOutputFormat(args[argIndex + 1]);
            ++argIndex;
        } else if ((currentArg == "-options") && hasMoreArgs) {
            options = args[argIndex + 1];
            ++argIndex;
        } else if ((currentArg == "-internal_options") && hasMoreArgs) {
            internalOptions = args[argIndex + 1];
            ++argIndex;
        } else if (currentArg == "--help") {
            operationMode = OperationMode::SHOW_HELP;
            return OclocErrorCode::SUCCESS;
        } else {
            argHelper->printf("Invalid option (arg %zd): %s\n", argIndex, currentArg.c_str());
            return OclocErrorCode::INVALID_COMMAND_LINE;
        }
    }

    return OclocErrorCode::SUCCESS;
}

IGC::CodeType::CodeType_t OfflineLinker::parseOutputFormat(const std::string &outputFormatName) {
    constexpr static std::array supportedFormatNames = {
        std::pair{"ELF", IGC::CodeType::elf},
        std::pair{"LLVM_BC", IGC::CodeType::llvmBc}};

    for (const auto &[name, format] : supportedFormatNames) {
        if (name == outputFormatName) {
            return format;
        }
    }

    return IGC::CodeType::invalid;
}

int OfflineLinker::verifyLinkerCommand() {
    if (inputFilenames.empty()) {
        argHelper->printf("Error: Input name is missing! At least one input file is required!\n");
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }

    for (const auto &filename : inputFilenames) {
        if (filename.empty()) {
            argHelper->printf("Error: Empty filename cannot be used!\n");
            return OclocErrorCode::INVALID_COMMAND_LINE;
        }

        if (!argHelper->fileExists(filename)) {
            argHelper->printf("Error: Input file %s missing.\n", filename.c_str());
            return OclocErrorCode::INVALID_FILE;
        }
    }

    if (outputFormat == IGC::CodeType::invalid) {
        argHelper->printf("Error: Invalid output type!\n");
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }

    return OclocErrorCode::SUCCESS;
}

int OfflineLinker::loadInputFilesContent() {
    std::unique_ptr<char[]> bytes{};
    size_t size{};
    IGC::CodeType::CodeType_t codeType{};

    inputFilesContent.reserve(inputFilenames.size());

    for (const auto &filename : inputFilenames) {
        size = 0;
        bytes = argHelper->loadDataFromFile(filename, size);
        if (size == 0) {
            argHelper->printf("Error: Cannot read input file: %s\n", filename.c_str());
            return OclocErrorCode::INVALID_FILE;
        }

        codeType = detectCodeType(bytes.get(), size);
        if (codeType == IGC::CodeType::invalid) {
            argHelper->printf("Error: Unsupported format of input file: %s\n", filename.c_str());
            return OclocErrorCode::INVALID_PROGRAM;
        }

        inputFilesContent.emplace_back(std::move(bytes), size, codeType);
    }

    return OclocErrorCode::SUCCESS;
}

IGC::CodeType::CodeType_t OfflineLinker::detectCodeType(char *bytes, size_t size) const {
    const auto bytesArray = ArrayRef<const uint8_t>::fromAny(bytes, size);
    if (isSpirVBitcode(bytesArray)) {
        return IGC::CodeType::spirV;
    }

    if (isLlvmBitcode(bytesArray)) {
        return IGC::CodeType::llvmBc;
    }

    return IGC::CodeType::invalid;
}

int OfflineLinker::initHardwareInfo() {
    // In spite of linking input files to intermediate representation instead of native binaries,
    // we have to initialize hardware info. Without that, initialization of IGC fails.
    // Therefore, we select the first valid hardware info entry and use it.
    const auto hwInfoTable{getHardwareInfoTable()};
    for (auto productId = 0u; productId < hwInfoTable.size(); ++productId) {
        if (hwInfoTable[productId]) {
            hwInfo = *hwInfoTable[productId];

            const auto hwInfoConfig = defaultHardwareInfoConfigTable[hwInfo.platform.eProductFamily];
            setHwInfoValuesFromConfig(hwInfoConfig, hwInfo);
            hardwareInfoSetup[hwInfo.platform.eProductFamily](&hwInfo, true, hwInfoConfig);

            return OclocErrorCode::SUCCESS;
        }
    }

    argHelper->printf("Error! Cannot retrieve any valid hardware information!\n");
    return OclocErrorCode::INVALID_DEVICE;
}

ArrayRef<const HardwareInfo *> OfflineLinker::getHardwareInfoTable() const {
    return {hardwareInfoTable};
}

int OfflineLinker::execute() {
    switch (operationMode) {
    case OperationMode::SHOW_HELP:
        return showHelp();
    case OperationMode::LINK_FILES:
        return link();
    case OperationMode::SKIP_EXECUTION:
        [[fallthrough]];
    default:
        argHelper->printf("Error: Linker cannot be executed due to unsuccessful initialization!\n");
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }
}

int OfflineLinker::showHelp() {
    constexpr auto help{R"===(Links several IR files to selected output format (LLVM BC, ELF).
Input files can be given in SPIR-V or LLVM BC.

Usage: ocloc link [-file <filename>]... -out <filename> [-out_format <format>] [-options <options>] [-internal_options <options>] [--help]

  -file <filename>              The input file to be linked.
                                Multiple files can be passed using repetition of this arguments.
                                Please see examples below.

  -out <filename>               Output filename.

  -out_format <format>          Output file format. Supported ones are ELF and LLVM_BC.
                                When not specified, LLVM_BC is used.

  -options <options>            Optional OpenCL C compilation options
                                as defined by OpenCL specification.

  -internal_options <options>   Optional compiler internal options
                                as defined by compilers used underneath.
                                Check intel-graphics-compiler (IGC) project
                                for details on available internal options.
                                You also may provide explicit --help to inquire
                                information about option, mentioned in -options.

  --help                        Print this usage message.

Examples:
  Link two SPIR-V files to LLVM BC output
    ocloc link -file first_file.spv -file second_file.spv -out linker_output.llvmbc

  Link two LLVM BC files to ELF output
    ocloc link -file first_file.llvmbc -file second_file.llvmbc -out_format ELF -out translated.elf
)==="};

    argHelper->printf(help);

    return OclocErrorCode::SUCCESS;
}

int OfflineLinker::link() {
    const auto encodedElfFile{createSingleInputFile()};
    if (outputFormat == IGC::CodeType::elf) {
        argHelper->saveOutput(outputFilename, encodedElfFile.data(), encodedElfFile.size());
        return OclocErrorCode::SUCCESS;
    }

    const auto [translationResult, translatedBitcode] = translateToOutputFormat(encodedElfFile);
    if (translationResult == OclocErrorCode::SUCCESS) {
        argHelper->saveOutput(outputFilename, translatedBitcode.data(), translatedBitcode.size());
    }

    return translationResult;
}

std::vector<uint8_t> OfflineLinker::createSingleInputFile() const {
    NEO::Elf::ElfEncoder<> elfEncoder{true, false, 1U};
    elfEncoder.getElfFileHeader().type = Elf::ET_OPENCL_OBJECTS;

    for (const auto &[bytes, size, codeType] : inputFilesContent) {
        const auto isSpirv = codeType == IGC::CodeType::spirV;
        const auto sectionType = isSpirv ? Elf::SHT_OPENCL_SPIRV : Elf::SHT_OPENCL_LLVM_BINARY;
        const auto sectionName = isSpirv ? Elf::SectionNamesOpenCl::spirvObject : Elf::SectionNamesOpenCl::llvmObject;
        const auto bytesArray = ArrayRef<const uint8_t>::fromAny(bytes.get(), size);

        elfEncoder.appendSection(sectionType, sectionName, bytesArray);
    }

    return elfEncoder.encode();
}

std::pair<int, std::vector<uint8_t>> OfflineLinker::translateToOutputFormat(const std::vector<uint8_t> &elfInput) {
    auto igcSrc = igcFacade->createConstBuffer(elfInput.data(), elfInput.size());
    auto igcOptions = igcFacade->createConstBuffer(options.c_str(), options.size());
    auto igcInternalOptions = igcFacade->createConstBuffer(internalOptions.c_str(), internalOptions.size());
    auto igcTranslationCtx = igcFacade->createTranslationContext(IGC::CodeType::elf, outputFormat);

    const auto tracingOptions{nullptr};
    const auto tracingOptionsSize{0};
    const auto igcOutput = igcTranslationCtx->Translate(igcSrc.get(), igcOptions.get(), igcInternalOptions.get(), tracingOptions, tracingOptionsSize);

    std::vector<uint8_t> outputFileContent{};
    if (!igcOutput) {
        argHelper->printf("Error: Translation has failed! IGC output is nullptr!\n");
        return {OclocErrorCode::OUT_OF_HOST_MEMORY, std::move(outputFileContent)};
    }

    if (igcOutput->GetOutput()->GetSizeRaw() != 0) {
        outputFileContent.resize(igcOutput->GetOutput()->GetSizeRaw());
        memcpy_s(outputFileContent.data(), outputFileContent.size(), igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetSizeRaw());
    }

    tryToStoreBuildLog(igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());

    const auto errorCode{igcOutput->Successful() ? OclocErrorCode::SUCCESS : OclocErrorCode::BUILD_PROGRAM_FAILURE};
    if (errorCode != OclocErrorCode::SUCCESS) {
        argHelper->printf("Error: Translation has failed! IGC returned empty output.\n");
    }

    return {errorCode, std::move(outputFileContent)};
}

std::string OfflineLinker::getBuildLog() const {
    return buildLog;
}

void OfflineLinker::tryToStoreBuildLog(const char *buildLogRaw, size_t size) {
    if (buildLogRaw && size != 0) {
        buildLog = std::string{buildLogRaw, buildLogRaw + size};
    }
}

} // namespace NEO