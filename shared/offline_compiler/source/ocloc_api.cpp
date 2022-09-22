/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_api.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/ocloc_interface.h"

#include <iostream>

extern "C" {
using namespace Ocloc;

int oclocInvoke(unsigned int numArgs, const char *argv[],
                const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    auto helper = std::make_unique<OclocArgHelper>(
        numSources, dataSources, lenSources, nameSources,
        numInputHeaders, dataInputHeaders, lenInputHeaders, nameInputHeaders,
        numOutputs, dataOutputs, lenOutputs, nameOutputs);
    std::vector<std::string> args(argv, argv + numArgs);

    try {
        if (numArgs <= 1 || NEO::ConstStringRef("-h") == args[1] || NEO::ConstStringRef("--help") == args[1]) {
            printHelp(helper.get());
            return NEO::OclocErrorCode::SUCCESS;
        }
        auto &command = args[1];
        if (command == CommandNames::disassemble) {
            return Commands::disassemble(helper.get(), args);
        } else if (command == CommandNames::assemble) {
            return Commands::assemble(helper.get(), args);
        } else if (command == CommandNames::multi) {
            return Commands::multi(helper.get(), args);
        } else if (command == CommandNames::validate) {
            return Commands::validate(helper.get(), args);
        } else if (command == CommandNames::query) {
            return Commands::query(helper.get(), args);
        } else if (command == CommandNames::ids) {
            return Commands::ids(helper.get(), args);
        } else if (command == CommandNames::link) {
            return Commands::link(helper.get(), args);
        } else if (command == CommandNames::concat) {
            return Commands::concat(helper.get(), args);
        } else {
            return Commands::compile(helper.get(), args);
        }
    } catch (const std::exception &e) {
        printf("%s\n", e.what());
        printOclocCmdLine(args);
        return -1;
    }
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
