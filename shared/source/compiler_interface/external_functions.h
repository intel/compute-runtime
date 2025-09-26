/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
struct KernelDescriptor;

enum ExternalFunctionResolveError : uint32_t {
    RESOLVE_SUCCESS = 0,
    ERROR_EXTERNAL_FUNCTION_INFO_MISSING,
    ERROR_KERNEL_DESCRIPTOR_MISSING,
    ERROR_LOOP_DETECTED
};

struct ExternalFunctionInfo {
    std::string functionName = "";
    uint8_t barrierCount = 0U;
    uint16_t numGrfRequired = 0U;
    uint8_t simdSize = 0U;
    bool hasRTCalls = false;
    bool hasPrintfCalls = false;
    bool hasIndirectCalls = false;
};

struct ExternalFunctionUsageKernel {
    std::string usedFuncName;
    std::string kernelName;
    bool optional = false;
};

struct ExternalFunctionUsageExtFunc {
    std::string usedFuncName;
    std::string callerFuncName;
    bool optional = false;
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

  protected:
    void resolveDependency(size_t nodeId, const std::vector<size_t> &edges);
    std::vector<size_t> seen;
    std::vector<size_t> resolved;
    const std::vector<std::vector<size_t>> &graph;
};

uint32_t resolveExternalDependencies(const ExternalFunctionInfosT &externalFunctionInfos, const KernelDependenciesT &kernelDependencies,
                                     const FunctionDependenciesT &funcDependencies, const KernelDescriptorMapT &nameToKernelDescriptor);

uint32_t getExtFuncDependencies(const FuncNameToIdMapT &funcNameToId, const FunctionDependenciesT &funcDependencies, size_t numExternalFuncs,
                                DependenciesT &outDependencies, CalledByT &outCalledBy);

uint32_t resolveExtFuncDependencies(const ExternalFunctionInfosT &externalFunctionInfos, const FuncNameToIdMapT &funcNameToId, const FunctionDependenciesT &funcDependencies);

uint32_t resolveKernelDependencies(const ExternalFunctionInfosT &externalFunctionInfos, const FuncNameToIdMapT &funcNameToId, const KernelDependenciesT &kernelDependencies, const KernelDescriptorMapT &nameToKernelDescriptor);

} // namespace NEO
