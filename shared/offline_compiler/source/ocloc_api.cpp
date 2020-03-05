/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_api.h"

#include <iostream>

void printHelp() {
    printf(R"===(ocloc is a tool for managing Intel OpenCL GPU device binary format.
It can be used for generation (as part of 'compile' command) as well as
manipulation (decoding/modifying - as part of 'disasm'/'asm' commands) of such
binary files.
Intel OpenCL GPU device binary is a format used by Intel OpenCL GPU runtime
(aka NEO). Intel OpenCL GPU runtime will return this binary format when queried
using clGetProgramInfo(..., CL_PROGRAM_BINARIES, ...). It will also honor
this format as input to clCreateProgramWithBinary function call.
ocloc does not require Intel GPU device to be present in the system nor does it
depend on Intel OpenCL GPU runtime driver to be installed. It does however rely
on the same set of compilers (IGC, common_clang) as the runtime driver.

Usage: ocloc [--help] <command> [<command_args>]
Available commands are listed below.
Use 'ocloc <command> --help' to get help about specific command.

Commands:
  compile               Compiles input to Intel OpenCL GPU device binary.
  disasm                Disassembles Intel OpenCL GPU device binary.
  asm                   Assembles Intel OpenCL GPU device binary.
  multi                 Compiles multiple files using a config file.

Default command (when none provided) is 'compile'.

Examples:
  Compile file to Intel OpenCL GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl

  Disassemble Intel OpenCL GPU device binary
    ocloc disasm -file source_file_Gen9core.bin

  Assemble to Intel OpenCL GPU device binary (after above disasm)
    ocloc asm -out reassembled.bin
)===");
}

extern "C" {
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
        if (numArgs == 1 || (numArgs > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
            printHelp();
        } else if (numArgs > 1 && !strcmp(argv[1], "disasm")) {
            BinaryDecoder disasm(std::move(helper));
            int retVal = disasm.validateInput(allArgs);
            if (retVal == 0) {
                return disasm.decode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && !strcmp(argv[1], "asm")) {
            BinaryEncoder assembler(std::move(helper));
            int retVal = assembler.validateInput(allArgs);
            if (retVal == 0) {
                return assembler.encode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && (!strcmp(argv[1], "multi") || !strcmp(argv[1], "-multi"))) {
            int retValue = ErrorCode::SUCCESS;
            auto pMulti = std::unique_ptr<MultiCommand>(MultiCommand::create(allArgs, retValue));
            return retValue;
        } else if (requestedFatBinary(numArgs, argv)) {
            return buildFatbinary(numArgs, argv);
        } else {
            int retVal = ErrorCode::SUCCESS;
            std::vector<std::string> allArgs;
            if (numArgs > 1) {
                allArgs.assign(argv, argv + numArgs);
            }

            OfflineCompiler *pCompiler = OfflineCompiler::create(numArgs, allArgs, true, retVal, std::move(helper));

            if (retVal == ErrorCode::SUCCESS) {
                retVal = buildWithSafetyGuard(pCompiler);

                std::string buildLog = pCompiler->getBuildLog();
                if (buildLog.empty() == false) {
                    printf("%s\n", buildLog.c_str());
                }

                if (retVal == ErrorCode::SUCCESS) {
                    if (!pCompiler->isQuiet())
                        printf("Build succeeded.\n");
                } else {
                    printf("Build failed with error code: %d\n", retVal);
                }
            }
            delete pCompiler;
            return retVal;
        }
    } catch (const std::exception &e) {
        printf("%s\n", e.what());
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
}
