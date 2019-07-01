/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/multi_command.h"
#include "offline_compiler/offline_compiler.h"
#include "runtime/os_interface/os_library.h"

#include "decoder/binary_decoder.h"
#include "decoder/binary_encoder.h"
#include <CL/cl.h>

#include <fstream>
#include <iostream>

using namespace NEO;

void printHelp() {
    printf(R"===(ocloc is a tool for offline compilation to Intel OCL GPU device binary files.
It can be used for generation as well as manipulation (decoding/modifying)
of such binary files.

Usage: ocloc [--help] <command> [<command_args>]
Available commands are listed below.
Use 'ocloc <command> --help' to get help about specific command.

Commands:
  compile               Compiles input to Intel OCL GPU device binary. 
  disasm                Disassembles Intel OCL GPU device binary. 
  asm                   Assembles Intel OCL GPU device binary.
  multi                 Compiles multiple files using a config file.

Default command (when none provided) is 'compile'.

Examples:
  Compile file to Intel OCL GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl
  
  Disassemble Intel OCL GPU device binary
    ocloc disasm -file source_file_Gen9core.bin
  
  Assemble to Intel OCL GPU device binary (after above disasm)
    ocloc asm -out reassembled.bin
)===");
}

int main(int numArgs, const char *argv[]) {
    try {
        if (numArgs == 1 || (numArgs > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
            printHelp();
        } else if (numArgs > 1 && !strcmp(argv[1], "disasm")) {
            BinaryDecoder disasm;
            int retVal = disasm.validateInput(numArgs, argv);
            if (retVal == 0) {
                return disasm.decode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && !strcmp(argv[1], "asm")) {
            BinaryEncoder assembler;
            int retVal = assembler.validateInput(numArgs, argv);
            if (retVal == 0) {
                return assembler.encode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && (!strcmp(argv[1], "multi") || !strcmp(argv[1], "-multi"))) {
            int retValue = CL_SUCCESS;
            auto pMulti = std::unique_ptr<MultiCommand>(MultiCommand::create(numArgs, argv, retValue));
            return retValue;
        } else {
            int retVal = CL_SUCCESS;
            std::vector<std::string> allArgs;
            if (numArgs > 1) {
                allArgs.assign(argv, argv + numArgs);
            }

            OfflineCompiler *pCompiler = OfflineCompiler::create(numArgs, allArgs, retVal);

            if (retVal == CL_SUCCESS) {
                retVal = buildWithSafetyGuard(pCompiler);

                std::string buildLog = pCompiler->getBuildLog();
                if (buildLog.empty() == false) {
                    printf("%s\n", buildLog.c_str());
                }

                if (retVal == CL_SUCCESS) {
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
}
