/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/numa_library.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include <bit>
#include <cerrno>
#include <iostream>

namespace NEO {
namespace Linux {

std::unique_ptr<NEO::OsLibrary> NumaLibrary::osLibrary(nullptr);
NumaLibrary::GetMemPolicyPtr NumaLibrary::getMemPolicyFunction(nullptr);
NumaLibrary::NumaAvailablePtr NumaLibrary::numaAvailableFunction(nullptr);
NumaLibrary::NumaMaxNodePtr NumaLibrary::numaMaxNodeFunction(nullptr);
NumaLibrary::NumaDistancePtr NumaLibrary::numaDistanceFunction(nullptr);
NumaLibrary::NumaNumPossibleNodesPtr NumaLibrary::numaNumPossibleNodesFunction(nullptr);
int NumaLibrary::maxNode(-1);
int NumaLibrary::possibleNodes(0);
bool NumaLibrary::numaLoaded(false);

bool NumaLibrary::init() {
    osLibrary.reset(NEO::OsLibrary::loadFunc(std::string(numaLibNameStr)));
    numaLoaded = false;
    numaAvailableFunction = nullptr;
    numaMaxNodeFunction = nullptr;
    getMemPolicyFunction = nullptr;
    numaDistanceFunction = nullptr;
    numaNumPossibleNodesFunction = nullptr;
    if (osLibrary) {
        DEBUG_BREAK_IF(!osLibrary->isLoaded());
        numaAvailableFunction = reinterpret_cast<NumaAvailablePtr>(osLibrary->getProcAddress(std::string(procNumaAvailableStr)));
        numaMaxNodeFunction = reinterpret_cast<NumaMaxNodePtr>(osLibrary->getProcAddress(std::string(procNumaMaxNodeStr)));
        getMemPolicyFunction = reinterpret_cast<GetMemPolicyPtr>(osLibrary->getProcAddress(std::string(procGetMemPolicyStr)));
        numaDistanceFunction = reinterpret_cast<NumaDistancePtr>(osLibrary->getProcAddress(std::string(procNumaDistanceStr)));
        numaNumPossibleNodesFunction = reinterpret_cast<NumaNumPossibleNodesPtr>(osLibrary->getProcAddress(std::string(procNumaNumPossibleNodesStr)));
        if (numaAvailableFunction && numaMaxNodeFunction && getMemPolicyFunction && numaNumPossibleNodesFunction) {
            if ((*numaAvailableFunction)() == 0) {
                maxNode = (*numaMaxNodeFunction)();
                possibleNodes = (*numaNumPossibleNodesFunction)();
                numaLoaded = maxNode > 0;
            }
        }
    }
    return numaLoaded;
}

bool NumaLibrary::getMemPolicy(int *mode, std::vector<unsigned long> &nodeMask) {
    if (numaLoaded) {
        nodeMask.assign(Math::divideAndRoundUp(static_cast<size_t>(possibleNodes), bitsPerNodeMaskElement), 0);
        return (*getMemPolicyFunction)(mode, nodeMask.data(), nodeMask.size() * bitsPerNodeMaskElement, nullptr, 0) != -1;
    }
    return false;
}

/**
 * @brief Gets the single NUMA node named by the memory policy of the calling thread.
 * Only MPOL_BIND, MPOL_PREFERRED and MPOL_PREFERRED_MANY policies whose node mask contains
 * exactly one node name a node. Policies with MPOL_F_RELATIVE_NODES do not name a node, because
 * their node mask is relative to the nodes allowed by the cpuset of the thread.
 *
 * @return NUMA node of the policy, or std::nullopt if the policy does not name exactly one node or cannot be read.
 */
std::optional<uint32_t> NumaLibrary::getSinglePreferredNode() {
    int mode = -1;
    std::vector<unsigned long> nodeMask;
    if (!getMemPolicy(&mode, nodeMask)) {
        return std::nullopt;
    }
    if (mode & mpolFRelativeNodes) {
        return std::nullopt;
    }
    mode &= ~mpolModeFlags;
    if (mode != mpolPreferred && mode != mpolBind && mode != mpolPreferredMany) {
        return std::nullopt;
    }
    uint32_t nodesInMask = 0;
    uint32_t node = 0;
    for (uint32_t maskIndex = 0; maskIndex < nodeMask.size(); maskIndex++) {
        if (nodeMask[maskIndex] != 0) {
            nodesInMask += std::popcount(nodeMask[maskIndex]);
            node = maskIndex * bitsPerNodeMaskElement + std::countr_zero(nodeMask[maskIndex]);
        }
    }
    if (nodesInMask != 1) {
        return std::nullopt;
    }
    return node;
}

int NumaLibrary::getDistance(uint32_t fromNode, uint32_t toNode) {
    if (!numaLoaded || !numaDistanceFunction) {
        return -1;
    }
    auto distance = (*numaDistanceFunction)(static_cast<int>(fromNode), static_cast<int>(toNode));
    return distance > 0 ? distance : -1;
}

} // namespace Linux
} // namespace NEO
