/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"

#include "shared/source/kernel/kernel_descriptor.h"

#include <algorithm>
namespace NEO {

uint32_t resolveExternalDependencies(const ExternalFunctionInfosT &externalFunctionInfos, const KernelDependenciesT &kernelDependencies,
                                     const FunctionDependenciesT &funcDependencies, const KernelDescriptorMapT &nameToKernelDescriptor) {
    FuncNameToIdMapT funcNameToId;
    for (size_t i = 0U; i < externalFunctionInfos.size(); i++) {
        auto &extFuncInfo = externalFunctionInfos[i];
        funcNameToId[extFuncInfo->functionName] = i;
    }

    auto error = resolveExtFuncDependencies(externalFunctionInfos, funcNameToId, funcDependencies);
    if (error != RESOLVE_SUCCESS) {
        return error;
    }

    error = resolveKernelDependencies(externalFunctionInfos, funcNameToId, kernelDependencies, nameToKernelDescriptor);
    return error;
}

uint32_t getExtFuncDependencies(const FuncNameToIdMapT &funcNameToId, const FunctionDependenciesT &funcDependencies, size_t numExternalFuncs,
                                DependenciesT &outDependencies, CalledByT &outCalledBy) {
    outDependencies.resize(numExternalFuncs);
    outCalledBy.resize(numExternalFuncs);
    for (size_t i = 0; i < funcDependencies.size(); i++) {
        auto funcDep = funcDependencies[i];
        if (funcNameToId.count(funcDep->callerFuncName) == 0 ||
            funcNameToId.count(funcDep->usedFuncName) == 0) {
            if (funcDep->optional) {
                continue;
            }
            return ERROR_EXTERNAL_FUNCTION_INFO_MISSING;
        }
        size_t callerId = funcNameToId.at(funcDep->callerFuncName);
        size_t calleeId = funcNameToId.at(funcDep->usedFuncName);

        outDependencies[callerId].push_back(calleeId);
        outCalledBy[calleeId].push_back(callerId);
    }

    return RESOLVE_SUCCESS;
}

uint32_t resolveExtFuncDependencies(const ExternalFunctionInfosT &externalFunctionInfos, const FuncNameToIdMapT &funcNameToId, const FunctionDependenciesT &funcDependencies) {
    DependenciesT dependencies;
    CalledByT calledBy;
    auto error = getExtFuncDependencies(funcNameToId, funcDependencies, externalFunctionInfos.size(), dependencies, calledBy);
    if (error != RESOLVE_SUCCESS) {
        return error;
    }

    DependencyResolver depResolver(dependencies);
    auto resolved = depResolver.resolveDependencies();
    for (auto calleeId : resolved) {
        const auto callee = externalFunctionInfos[calleeId];
        for (auto callerId : calledBy[calleeId]) {
            auto caller = externalFunctionInfos[callerId];
            caller->barrierCount = std::max(caller->barrierCount, callee->barrierCount);
            caller->hasRTCalls |= callee->hasRTCalls;
            caller->hasPrintfCalls |= callee->hasPrintfCalls;
            caller->hasIndirectCalls |= callee->hasIndirectCalls;
        }
    }
    return RESOLVE_SUCCESS;
}

uint32_t resolveKernelDependencies(const ExternalFunctionInfosT &externalFunctionInfos, const FuncNameToIdMapT &funcNameToId, const KernelDependenciesT &kernelDependencies, const KernelDescriptorMapT &nameToKernelDescriptor) {
    for (auto &kernelDep : kernelDependencies) {
        if (funcNameToId.count(kernelDep->usedFuncName) == 0) {
            if (kernelDep->optional) {
                continue;
            }
            return ERROR_EXTERNAL_FUNCTION_INFO_MISSING;
        } else if (nameToKernelDescriptor.count(kernelDep->kernelName) == 0) {
            return ERROR_KERNEL_DESCRIPTOR_MISSING;
        }
        auto &kernelAttributes = nameToKernelDescriptor.at(kernelDep->kernelName)->kernelAttributes;
        const auto &externalFunctionInfo = *externalFunctionInfos.at(funcNameToId.at(kernelDep->usedFuncName));
        kernelAttributes.barrierCount = std::max(externalFunctionInfo.barrierCount, kernelAttributes.barrierCount);
        kernelAttributes.flags.hasRTCalls |= externalFunctionInfo.hasRTCalls;
        kernelAttributes.flags.hasPrintfCalls |= externalFunctionInfo.hasPrintfCalls;
        kernelAttributes.flags.hasIndirectCalls |= externalFunctionInfo.hasIndirectCalls;
    }
    return RESOLVE_SUCCESS;
}

std::vector<size_t> DependencyResolver::resolveDependencies() {
    for (size_t i = 0; i < graph.size(); i++) {
        if (std::find(seen.begin(), seen.end(), i) == seen.end()) {
            resolveDependency(i, graph[i]);
        }
    }
    return resolved;
}

void DependencyResolver::resolveDependency(size_t nodeId, const std::vector<size_t> &edges) {
    seen.push_back(nodeId);
    for (auto &edgeId : edges) {
        if (std::find(resolved.begin(), resolved.end(), edgeId) == resolved.end() &&
            std::find(seen.begin(), seen.end(), edgeId) == seen.end()) {
            resolveDependency(edgeId, graph[edgeId]);
        }
    }
    resolved.push_back(nodeId);
}
} // namespace NEO