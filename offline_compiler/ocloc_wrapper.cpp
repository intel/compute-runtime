/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_wrapper.h"

#include "shared/source/os_interface/os_library.h"

#include "utilities/get_path.h"

#include <iostream>
#include <string>

typedef int (*pOclocInvoke)(
    unsigned int numArgs, const char *argv[],
    const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
    const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
    uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);

typedef int (*pOclocFreeOutput)(
    uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs);

struct OclocLibrary {
    pOclocInvoke invoke = nullptr;
    pOclocFreeOutput freeOutput = nullptr;

    std::unique_ptr<NEO::OsLibrary> library;
    bool isLoaded() {
        return library != nullptr;
    }
};

OclocWrapper::OclocWrapper() : pImpl(std::make_unique<Impl>()){};
OclocWrapper::~OclocWrapper() = default;

struct OclocWrapper::Impl {
    OclocLibrary oclocLib;

    void loadOcloc() {
        OclocLibrary ocloc;
        std::string oclocLibName = "";
        ocloc.library.reset(NEO::OsLibrary::load(oclocLibName));
        if (nullptr == (ocloc.invoke = reinterpret_cast<pOclocInvoke>(ocloc.library->getProcAddress("oclocInvoke")))) {
            std::cout << "Error! Couldn't find OclocInvoke function.\n";
            return;
        }
        if (nullptr == (ocloc.freeOutput = reinterpret_cast<pOclocFreeOutput>(ocloc.library->getProcAddress("oclocFreeOutput")))) {
            std::cout << "Error! Couldn't find OclocFreeOutput function.\n";
            return;
        }
        this->oclocLib = std::move(ocloc);
    }
};

int OclocWrapper::invokeOcloc(unsigned int numArgs, const char *argv[]) {
    return invokeOcloc(numArgs, argv, 0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr);
}

int OclocWrapper::invokeOcloc(unsigned int numArgs, const char *argv[],
                              const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                              const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                              uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    if (false == tryLoadOcloc()) {
        std::cout << "Error! Ocloc Library couldn't be loaded.\n";
        return -1;
    }
    return pImpl->oclocLib.invoke(numArgs, argv,
                                  numSources, dataSources, lenSources, nameSources,
                                  numInputHeaders, dataInputHeaders, lenInputHeaders, nameInputHeaders,
                                  numOutputs, dataOutputs, lenOutputs, nameOutputs);
}

int OclocWrapper::freeOutput(uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {
    if (false == tryLoadOcloc()) {
        std::cout << "Error! Ocloc Library couldn't be loaded.\n";
        return -1;
    }
    return pImpl->oclocLib.freeOutput(numOutputs, dataOutputs, lenOutputs, nameOutputs);
}

bool OclocWrapper::tryLoadOcloc() {
    if (false == pImpl->oclocLib.isLoaded()) {
        pImpl->loadOcloc();
    }
    return pImpl->oclocLib.isLoaded();
}
