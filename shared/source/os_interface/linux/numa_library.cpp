/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/numa_library.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cerrno>
#include <iostream>

namespace NEO {
namespace Linux {

std::unique_ptr<NEO::OsLibrary> NumaLibrary::osLibrary(nullptr);
NumaLibrary::GetMemPolicyPtr NumaLibrary::getMemPolicyFunction(nullptr);
NumaLibrary::NumaAvailablePtr NumaLibrary::numaAvailableFunction(nullptr);
NumaLibrary::NumaMaxNodePtr NumaLibrary::numaMaxNodeFunction(nullptr);
int NumaLibrary::maxNode(-1);
bool NumaLibrary::numaLoaded(false);

bool NumaLibrary::init() {
    osLibrary.reset(NEO::OsLibrary::loadFunc(std::string(numaLibNameStr)));
    numaLoaded = false;
    numaAvailableFunction = nullptr;
    numaMaxNodeFunction = nullptr;
    getMemPolicyFunction = nullptr;
    if (osLibrary) {
        DEBUG_BREAK_IF(!osLibrary->isLoaded());
        numaAvailableFunction = reinterpret_cast<NumaAvailablePtr>(osLibrary->getProcAddress(std::string(procNumaAvailableStr)));
        numaMaxNodeFunction = reinterpret_cast<NumaMaxNodePtr>(osLibrary->getProcAddress(std::string(procNumaMaxNodeStr)));
        getMemPolicyFunction = reinterpret_cast<GetMemPolicyPtr>(osLibrary->getProcAddress(std::string(procGetMemPolicyStr)));
        if (numaAvailableFunction && numaMaxNodeFunction && getMemPolicyFunction) {
            if ((*numaAvailableFunction)() == 0) {
                maxNode = (*numaMaxNodeFunction)();
                numaLoaded = maxNode > 0;
            }
        }
    }
    return numaLoaded;
}

bool NumaLibrary::getMemPolicy(int *mode, std::vector<unsigned long> &nodeMask) {
    if (numaLoaded) {
        // re-initialize vector with size maxNode;
        std::vector<unsigned long>(maxNode + 1, 0).swap(nodeMask);
        return (*getMemPolicyFunction)(mode, nodeMask.data(), maxNode + 1, nullptr, 0) != -1;
    }
    return false;
}

} // namespace Linux
} // namespace NEO
