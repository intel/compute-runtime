/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_api.h"

#include "shared/offline_compiler/source/decoder/binary_decoder.h"
#include "shared/offline_compiler/source/decoder/binary_encoder.h"
#include "shared/offline_compiler/source/multi_command.h"
#include "shared/offline_compiler/source/ocloc_concat.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/ocloc_validator.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/offline_linker.h"

#include <iostream>

using namespace NEO;

const char *help = R"===(ocloc is a tool for managing Intel Compute GPU device binary format.
It can be used for generation (as part of 'compile' command) as well as
manipulation (decoding/modifying - as part of 'disasm'/'asm' commands) of such
binary files.
Intel Compute GPU device binary is a format used by Intel Compute GPU runtime
(aka NEO). Intel Compute GPU runtime will return this binary format when queried
using clGetProgramInfo(..., CL_PROGRAM_BINARIES, ...). It will also honor
this format as input to clCreateProgramWithBinary function call.
ocloc does not require Intel GPU device to be present in the system nor does it
depend on Intel Compute GPU runtime driver to be installed. It does however rely
on the same set of compilers (IGC, common_clang) as the runtime driver.

Usage: ocloc [--help] <command> [<command_args>]
Available commands are listed below.
Use 'ocloc <command> --help' to get help about specific command.

Commands:
  compile               Compiles input to Intel Compute GPU device binary.
  link                  Links several IR files.
  disasm                Disassembles Intel Compute GPU device binary.
  asm                   Assembles Intel Compute GPU device binary.
  multi                 Compiles multiple files using a config file.
  validate              Validates Intel Compute GPU device binary.
  query                 Extracts versioning info.
  ids                   Return matching versions <major>.<minor>.<revision>.

Default command (when none provided) is 'compile'.

Examples:
  Compile file to Intel Compute GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl

  Link two SPIR-V files.
    ocloc link -file sample1.spv -file sample2.spv -out_format LLVM_BC -out samples_merged.llvm_bc

  Disassemble Intel Compute GPU device binary
    ocloc disasm -file source_file_Gen9core.bin

  Assemble to Intel Compute GPU device binary (after above disasm)
    ocloc asm -out reassembled.bin

  Validate Intel Compute GPU device binary
    ocloc validate -file source_file_Gen9core.bin

  Extract driver version
    ocloc query OCL_DRIVER_VERSION

  Concatenate fat binaries
    ocloc concat <fat binary> <fat binary> ... [-out <concatenated fat binary name>]

  Return matching version for an acronym
    ocloc ids dg1
)===";

extern "C" {
void printOclocCmdLine(unsigned int numArgs, const char *argv[], std::unique_ptr<OclocArgHelper> &helper) {
    printf("Command was:");
    bool useQuotes = false;
    for (auto i = 0u; i < numArgs; ++i) {
        const char *currArg = argv[i];
        if (useQuotes) {
            printf(" \"%s\"", currArg);
            useQuotes = false;
        } else {
            printf(" %s", currArg);
            useQuotes = helper->areQuotesRequired(currArg);
        }
    }
    printf("\n");
}

void printOclocOptionsReadFromFile(OfflineCompiler *pCompiler) {
    if (pCompiler) {
        std::string options = pCompiler->getOptionsReadFromFile();
        if (options != "") {
            printf("Compiling options read from file were:\n%s\n", options.c_str());
        }

        std::string internalOptions = pCompiler->getInternalOptionsReadFromFile();
        if (internalOptions != "") {
            printf("Internal options read from file were:\n%s\n", internalOptions.c_str());
        }
    }
}

int oclocInvoke(unsigned int numArgs, const char *argv[],
                const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    auto helper = std::make_unique<OclocArgHelper>(
        numSources, dataSources, lenSources, nameSources,
        numInputHeaders, dataInputHeaders, lenInputHeaders, nameInputHeaders,
        numOutputs, dataOutputs, lenOutputs, nameOutputs);
    std::vector<std::string> allArgs;
    if (numArgs > 1) {
        allArgs.assign(argv, argv + numArgs);
    }

    try {
        if (numArgs == 1 || (numArgs > 1 && (ConstStringRef("-h") == allArgs[1] || ConstStringRef("--help") == allArgs[1]))) {
            helper->printf("%s", help);
            return OclocErrorCode::SUCCESS;
        } else if (numArgs > 1 && ConstStringRef("disasm") == allArgs[1]) {
            BinaryDecoder disasm(helper.get());
            int retVal = disasm.validateInput(allArgs);

            if (disasm.showHelp) {
                disasm.printHelp();
                return retVal;
            }

            if (retVal == 0) {
                return disasm.decode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && ConstStringRef("asm") == allArgs[1]) {
            BinaryEncoder assembler(helper.get());
            int retVal = assembler.validateInput(allArgs);

            if (assembler.showHelp) {
                assembler.printHelp();
                return retVal;
            }

            if (retVal == 0) {
                return assembler.encode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && ConstStringRef("multi") == allArgs[1]) {
            int retValue = OclocErrorCode::SUCCESS;
            std::unique_ptr<MultiCommand> pMulti{(MultiCommand::create(allArgs, retValue, helper.get()))};
            return retValue;
        } else if (requestedFatBinary(allArgs, helper.get())) {
            return buildFatBinary(allArgs, helper.get());
        } else if (numArgs > 1 && ConstStringRef("validate") == allArgs[1]) {
            return NEO::Ocloc::validate(allArgs, helper.get());
        } else if (numArgs > 1 && ConstStringRef("query") == allArgs[1]) {
            return OfflineCompiler::query(numArgs, allArgs, helper.get());
        } else if (numArgs > 1 && ConstStringRef("ids") == allArgs[1]) {
            return OfflineCompiler::queryAcronymIds(numArgs, allArgs, helper.get());
        } else if (numArgs > 1 && ConstStringRef("link") == allArgs[1]) {
            int createResult{OclocErrorCode::SUCCESS};
            const auto linker{OfflineLinker::create(numArgs, allArgs, createResult, helper.get())};
            const auto linkingResult{linkWithSafetyGuard(linker.get())};

            const auto buildLog = linker->getBuildLog();
            if (!buildLog.empty()) {
                helper->printf("%s\n", buildLog.c_str());
            }

            if (createResult == OclocErrorCode::SUCCESS && linkingResult == OclocErrorCode::SUCCESS) {
                helper->printf("Linker execution has succeeded!\n");
            }

            return createResult | linkingResult;
        } else if (numArgs > 1 && NEO::OclocConcat::commandStr == allArgs[1]) {
            auto arConcat = NEO::OclocConcat(helper.get());
            auto error = arConcat.initialize(allArgs);
            if (OclocErrorCode::SUCCESS != error) {
                arConcat.printHelp();
                return error;
            }

            error = arConcat.concatenate();
            return error;
        } else {
            int retVal = OclocErrorCode::SUCCESS;

            std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(numArgs, allArgs, true, retVal, helper.get())};
            if (retVal == OclocErrorCode::SUCCESS) {
                retVal = buildWithSafetyGuard(pCompiler.get());

                std::string buildLog = pCompiler->getBuildLog();
                if (buildLog.empty() == false) {
                    helper->printf("%s\n", buildLog.c_str());
                }

                if (retVal == OclocErrorCode::SUCCESS) {
                    if (!pCompiler->isQuiet())
                        helper->printf("Build succeeded.\n");
                } else {
                    helper->printf("Build failed with error code: %d\n", retVal);
                }
            }

            if (retVal != OclocErrorCode::SUCCESS) {
                printOclocOptionsReadFromFile(pCompiler.get());
                printOclocCmdLine(numArgs, argv, helper);
            }
            return retVal;
        }
    } catch (const std::exception &e) {
        helper->printf("%s\n", e.what());
        printOclocCmdLine(numArgs, argv, helper);
        return -1;
    }
    return -1;
}

int oclocFreeOutput(uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    for (uint32_t i = 0; i < *numOutputs; i++) {
        delete[](*dataOutputs)[i];
        delete[](*nameOutputs)[i];
    }
    delete[](*dataOutputs);
    delete[](*lenOutputs);
    delete[](*nameOutputs);
    return 0;
}

int oclocVersion() {
    return static_cast<int>(ocloc_version_t::OCLOC_VERSION_CURRENT);
}
}
