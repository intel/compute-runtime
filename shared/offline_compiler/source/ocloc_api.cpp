/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_api.h"

#include "shared/offline_compiler/source/decoder/binary_decoder.h"
#include "shared/offline_compiler/source/decoder/binary_encoder.h"
#include "shared/offline_compiler/source/multi_command.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/offline_compiler.h"

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
  disasm                Disassembles Intel Compute GPU device binary.
  asm                   Assembles Intel Compute GPU device binary.
  multi                 Compiles multiple files using a config file.

Default command (when none provided) is 'compile'.

Examples:
  Compile file to Intel Compute GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl

  Disassemble Intel Compute GPU device binary
    ocloc disasm -file source_file_Gen9core.bin

  Assemble to Intel Compute GPU device binary (after above disasm)
    ocloc asm -out reassembled.bin
)===";

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
        if (numArgs == 1 || (numArgs > 1 && (ConstStringRef("-h") == allArgs[1] || ConstStringRef("--help") == allArgs[1]))) {
            helper->printf("%s", help);
            return ErrorCode::SUCCESS;
        } else if (numArgs > 1 && ConstStringRef("disasm") == allArgs[1]) {
            BinaryDecoder disasm(helper.get());
            int retVal = disasm.validateInput(allArgs);
            if (retVal == 0) {
                return disasm.decode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && ConstStringRef("asm") == allArgs[1]) {
            BinaryEncoder assembler(helper.get());
            int retVal = assembler.validateInput(allArgs);
            if (retVal == 0) {
                return assembler.encode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && (ConstStringRef("multi") == allArgs[1] || ConstStringRef("-multi") == allArgs[1])) {
            int retValue = ErrorCode::SUCCESS;
            std::unique_ptr<MultiCommand> pMulti{(MultiCommand::create(allArgs, retValue, helper.get()))};
            return retValue;
        } else if (requestedFatBinary(numArgs, argv)) {
            return buildFatbinary(numArgs, argv, helper.get());
        } else {
            int retVal = ErrorCode::SUCCESS;

            std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(numArgs, allArgs, true, retVal, helper.get())};
            if (retVal == ErrorCode::SUCCESS) {
                retVal = buildWithSafetyGuard(pCompiler.get());

                std::string buildLog = pCompiler->getBuildLog();
                if (buildLog.empty() == false) {
                    helper->printf("%s\n", buildLog.c_str());
                }

                if (retVal == ErrorCode::SUCCESS) {
                    if (!pCompiler->isQuiet())
                        helper->printf("Build succeeded.\n");
                } else {
                    helper->printf("Build failed with error code: %d\n", retVal);
                }
            }
            return retVal;
        }
    } catch (const std::exception &e) {
        helper->printf("%s\n", e.what());
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
