/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/numa_library.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>
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
NumaLibrary::NumaGetRunNodeMaskPtr NumaLibrary::numaGetRunNodeMaskFunction(nullptr);
NumaLibrary::NumaBitmaskFreePtr NumaLibrary::numaBitmaskFreeFunction(nullptr);
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
    numaGetRunNodeMaskFunction = nullptr;
    numaBitmaskFreeFunction = nullptr;
    if (osLibrary) {
        DEBUG_BREAK_IF(!osLibrary->isLoaded());
        numaAvailableFunction = reinterpret_cast<NumaAvailablePtr>(osLibrary->getProcAddress(std::string(procNumaAvailableStr)));
        numaMaxNodeFunction = reinterpret_cast<NumaMaxNodePtr>(osLibrary->getProcAddress(std::string(procNumaMaxNodeStr)));
        getMemPolicyFunction = reinterpret_cast<GetMemPolicyPtr>(osLibrary->getProcAddress(std::string(procGetMemPolicyStr)));
        numaDistanceFunction = reinterpret_cast<NumaDistancePtr>(osLibrary->getProcAddress(std::string(procNumaDistanceStr)));
        numaNumPossibleNodesFunction = reinterpret_cast<NumaNumPossibleNodesPtr>(osLibrary->getProcAddress(std::string(procNumaNumPossibleNodesStr)));
        numaGetRunNodeMaskFunction = reinterpret_cast<NumaGetRunNodeMaskPtr>(osLibrary->getProcAddress(std::string(procNumaGetRunNodeMaskStr)));
        numaBitmaskFreeFunction = reinterpret_cast<NumaBitmaskFreePtr>(osLibrary->getProcAddress(std::string(procNumaBitmaskFreeStr)));
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
 * @brief Gets the single NUMA node from which the memory policy of the calling thread takes pages.
 * MPOL_BIND, MPOL_PREFERRED and MPOL_PREFERRED_MANY policies name a node when their node mask contains
 * exactly one node. Policies with MPOL_F_RELATIVE_NODES do not name a node, because their node mask is
 * relative to the nodes allowed by the cpuset of the thread.
 * MPOL_DEFAULT, MPOL_LOCAL and MPOL_PREFERRED with an empty node mask
 * take pages from the node of the CPU on which the thread runs.
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
    const bool isNodeMaskEmpty = std::all_of(nodeMask.begin(), nodeMask.end(), [](unsigned long element) { return element == 0; });
    if (mode == mpolDefault || mode == mpolLocal || (mode == mpolPreferred && isNodeMaskEmpty)) {
        return getSingleRunNode();
    }
    if (mode != mpolPreferred && mode != mpolBind && mode != mpolPreferredMany) {
        return std::nullopt;
    }
    return getSingleNodeInMask(nodeMask);
}

std::optional<uint32_t> NumaLibrary::getSingleNodeInMask(std::span<const unsigned long> nodeMask) {
    uint32_t nodesInMask = 0;
    uint32_t node = 0;
    uint32_t maskIndex = 0;
    for (const auto maskElement : nodeMask) {
        if (maskElement != 0) {
            nodesInMask += std::popcount(maskElement);
            node = maskIndex * bitsPerNodeMaskElement + std::countr_zero(maskElement);
        }
        maskIndex++;
    }
    if (nodesInMask != 1) {
        return std::nullopt;
    }
    return node;
}

std::optional<uint32_t> NumaLibrary::getSingleRunNode() {
    if (!numaGetRunNodeMaskFunction || !numaBitmaskFreeFunction) {
        return std::nullopt;
    }
    auto runNodeMask = (*numaGetRunNodeMaskFunction)();
    if (!runNodeMask) {
        return std::nullopt;
    }
    const auto node = getSingleNodeInMask({runNodeMask->maskp, Math::divideAndRoundUp(static_cast<size_t>(runNodeMask->size), bitsPerNodeMaskElement)});
    (*numaBitmaskFreeFunction)(runNodeMask);
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
