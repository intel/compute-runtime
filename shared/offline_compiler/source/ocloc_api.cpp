/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_api.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_interface.h"

#include <iostream>

using namespace Ocloc;

int oclocInvokeWithHelper(
    OclocArgHelper *argHelper,
    unsigned int numArgs, const char *argv[]) {

    std::vector<std::string> args(argv, argv + numArgs);

    try {
        if (numArgs <= 1 || NEO::ConstStringRef("-h") == args[1] || NEO::ConstStringRef("--help") == args[1]) {
            printHelp(*argHelper);
            return OCLOC_SUCCESS;
        }
        auto &command = args[1];
        int retVal = -0;
        if (command == CommandNames::disassemble) {
            retVal = Commands::disassemble(argHelper, args);
        } else if (command == CommandNames::assemble) {
            retVal = Commands::assemble(argHelper, args);
        } else if (command == CommandNames::multi) {
            retVal = Commands::multi(argHelper, args);
        } else if (command == CommandNames::validate) {
            retVal = Commands::validate(argHelper, args);
        } else if (command == CommandNames::query) {
            retVal = Commands::query(argHelper, args);
        } else if (command == CommandNames::ids) {
            retVal = Commands::ids(argHelper, args);
        } else if (command == CommandNames::link) {
            retVal = Commands::link(argHelper, args);
        } else if (command == CommandNames::concat) {
            retVal = Commands::concat(argHelper, args);
        } else {
            retVal = Commands::compile(argHelper, args);
        }

        if (retVal != OCLOC_SUCCESS) {
            printOclocCmdLine(*argHelper, args);
        } else if (argHelper->isVerbose()) {
            printOclocCmdLine(*argHelper, args);
        }
        return retVal;
    } catch (const std::exception &e) {
        argHelper->printf("%s\n", e.what());
        printOclocCmdLine(*argHelper, args);
        return -1;
    }
}

extern "C" {
int oclocInvoke(unsigned int numArgs, const char *argv[],
                const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    auto argHelper = std::make_unique<OclocArgHelper>(
        numSources, dataSources, lenSources, nameSources,
        numInputHeaders, dataInputHeaders, lenInputHeaders, nameInputHeaders,
        numOutputs, dataOutputs, lenOutputs, nameOutputs);
    return oclocInvokeWithHelper(argHelper.get(), numArgs, argv);
}

int oclocFreeOutput(uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    for (uint32_t i = 0; i < *numOutputs; i++) {
        delete[] (*dataOutputs)[i];
        delete[] (*nameOutputs)[i];
    }
    delete[] (*dataOutputs);
    delete[] (*lenOutputs);
    delete[] (*nameOutputs);
    return 0;
}

int oclocVersion() {
    return static_cast<int>(ocloc_version_t::OCLOC_VERSION_CURRENT);
}
}
