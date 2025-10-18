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

extern "C" {
using namespace Ocloc;

int oclocInvoke(unsigned int numArgs, const char *argv[],
                const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    auto argHelper = std::make_unique<OclocArgHelper>(
        numSources, dataSources, lenSources, nameSources,
        numInputHeaders, dataInputHeaders, lenInputHeaders, nameInputHeaders,
        numOutputs, dataOutputs, lenOutputs, nameOutputs);
    std::vector<std::string> args(argv, argv + numArgs);

    try {
        if (numArgs <= 1 || NEO::ConstStringRef("-h") == args[1] || NEO::ConstStringRef("--help") == args[1]) {
            printHelp(*argHelper);
            return OCLOC_SUCCESS;
        }
        auto &command = args[1];
        int retVal = -0;
        if (command == CommandNames::disassemble) {
            retVal = Commands::disassemble(argHelper.get(), args);
        } else if (command == CommandNames::assemble) {
            retVal = Commands::assemble(argHelper.get(), args);
        } else if (command == CommandNames::multi) {
            retVal = Commands::multi(argHelper.get(), args);
        } else if (command == CommandNames::validate) {
            retVal = Commands::validate(argHelper.get(), args);
        } else if (command == CommandNames::query) {
            retVal = Commands::query(argHelper.get(), args);
        } else if (command == CommandNames::ids) {
            retVal = Commands::ids(argHelper.get(), args);
        } else if (command == CommandNames::link) {
            retVal = Commands::link(argHelper.get(), args);
        } else if (command == CommandNames::concat) {
            retVal = Commands::concat(argHelper.get(), args);
        } else {
            retVal = Commands::compile(argHelper.get(), args);
        }

        if (retVal == ocloc_error_t::OCLOC_INVALID_DEVICE && !getOclocFormerLibName().empty()) {
            argHelper->printf("Invalid device error, trying to fallback to former ocloc %s\n", getOclocFormerLibName().c_str());
            auto retValFromFormerOcloc = Commands::invokeFormerOcloc(getOclocFormerLibName(), numArgs, argv, numSources, dataSources, lenSources, nameSources, numInputHeaders, dataInputHeaders, lenInputHeaders, nameInputHeaders, numOutputs, dataOutputs, lenOutputs, nameOutputs);
            if (retValFromFormerOcloc) {
                retVal = retValFromFormerOcloc.value();
                argHelper->dontSetupOutputs();
            } else {
                argHelper->printf("Couldn't load former ocloc %s\n", getOclocFormerLibName().c_str());
            }
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
