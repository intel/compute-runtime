/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
struct KernelDescriptor;

enum ExternalFunctionResolveError : uint32_t {
    RESOLVE_SUCCESS = 0,
    ERROR_EXTERNAL_FUNCTION_INFO_MISSING,
    ERROR_KERNEL_DESCRIPTOR_MISSING,
    ERROR_LOOP_DETECKTED
};

struct ExternalFunctionInfo {
    std::string functionName = "";
    uint8_t barrierCount = 0U;
    uint16_t numGrfRequired = 0U;
    uint8_t simdSize = 0U;
};

struct ExternalFunctionUsageKernel {
    std::string usedFuncName;
    std::string kernelName;
};

struct ExternalFunctionUsageExtFunc {
    std::string usedFuncName;
    std::string callerFuncName;
};

using ExternalFunctionInfosT = std::vector<ExternalFunctionInfo *>;
using KernelDependenciesT = std::vector<const ExternalFunctionUsageKernel *>;
using FunctionDependenciesT = std::vector<const ExternalFunctionUsageExtFunc *>;
using KernelDescriptorMapT = std::unordered_map<std::string, KernelDescriptor *>;
using FuncNameToIdMapT = std::unordered_map<std::string, size_t>;
using DependenciesT = std::vector<std::vector<size_t>>;
using CalledByT = std::vector<std::vector<size_t>>;

class DependencyResolver {
  public:
    DependencyResolver(const std::vector<std::vector<size_t>> &graph) : graph(graph) {}
    std::vector<size_t> resolveDependencies();
    inline bool hasLoop() { return loopDeteckted; }

  protected:
    void resolveDependency(size_t nodeId, const std::vector<size_t> &edges);
    std::vector<size_t> seen;
    std::vector<size_t> resolved;
    const std::vector<std::vector<size_t>> &graph;
    bool loopDeteckted = false;
};

uint32_t resolveBarrierCount(ExternalFunctionInfosT externalFunctionInfos, KernelDependenciesT kernelDependencies,
                             FunctionDependenciesT funcDependencies, KernelDescriptorMapT &nameToKernelDescriptor);

uint32_t getExtFuncDependencies(FuncNameToIdMapT &funcNameToId, FunctionDependenciesT funcDependencies, size_t numExternalFuncs,
                                DependenciesT &outDependencies, CalledByT &outCalledBy);

uint32_t resolveExtFuncDependencies(ExternalFunctionInfosT externalFunctionInfos, FuncNameToIdMapT &funcNameToId, FunctionDependenciesT funcDependencies);

uint32_t resolveKernelDependencies(ExternalFunctionInfosT externalFunctionInfos, FuncNameToIdMapT &funcNameToId, KernelDependenciesT kernelDependencies, KernelDescriptorMapT &nameToKernelDescriptor);

} // namespace NEO
