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

int main(int numArgs, const char *argv[]) {
    try {
        if (numArgs > 1 && !strcmp(argv[1], "disasm")) { // -file binary.bin -patch workspace/igc/inc -dump dump/folder
            BinaryDecoder disasm;
            int retVal = disasm.validateInput(numArgs, argv);
            if (retVal == 0) {
                return disasm.decode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && !strcmp(argv[1], "asm")) { // -dump dump/folder -out new_elf.bin
            BinaryEncoder assembler;
            int retVal = assembler.validateInput(numArgs, argv);
            if (retVal == 0) {
                return assembler.encode();
            } else {
                return retVal;
            }
        } else if (numArgs > 1 && !strcmp(argv[1], "-multi")) {
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
