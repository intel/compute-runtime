/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include <algorithm>
namespace NEO {

uint32_t resolveBarrierCount(ExternalFunctionInfosT externalFunctionInfos, KernelDependenciesT kernelDependencies,
                             FunctionDependenciesT funcDependencies, KernelDescriptorMapT &nameToKernelDescriptor) {
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

uint32_t getExtFuncDependencies(FuncNameToIdMapT &funcNameToId, FunctionDependenciesT funcDependencies, size_t numExternalFuncs,
                                DependenciesT &outDependencies, CalledByT &outCalledBy) {
    outDependencies.resize(numExternalFuncs);
    outCalledBy.resize(numExternalFuncs);
    for (size_t i = 0; i < funcDependencies.size(); i++) {
        auto funcDep = funcDependencies[i];
        if (funcNameToId.count(funcDep->callerFuncName) == 0 ||
            funcNameToId.count(funcDep->usedFuncName) == 0) {
            return ERROR_EXTERNAL_FUNCTION_INFO_MISSING;
        }
        size_t callerId = funcNameToId[funcDep->callerFuncName];
        size_t calleeId = funcNameToId[funcDep->usedFuncName];

        outDependencies[callerId].push_back(calleeId);
        outCalledBy[calleeId].push_back(callerId);
    }

    return RESOLVE_SUCCESS;
}

uint32_t resolveExtFuncDependencies(ExternalFunctionInfosT externalFunctionInfos, FuncNameToIdMapT &funcNameToId, FunctionDependenciesT funcDependencies) {
    DependenciesT dependencies;
    CalledByT calledBy;
    auto error = getExtFuncDependencies(funcNameToId, funcDependencies, externalFunctionInfos.size(), dependencies, calledBy);
    if (error != RESOLVE_SUCCESS) {
        return error;
    }

    DependencyResolver depResolver(dependencies);
    auto resolved = depResolver.resolveDependencies();
    if (depResolver.hasLoop()) {
        return ERROR_LOOP_DETECTED;
    }
    for (auto calleeId : resolved) {
        const auto callee = externalFunctionInfos[calleeId];
        for (auto callerId : calledBy[calleeId]) {
            auto caller = externalFunctionInfos[callerId];
            caller->barrierCount = std::max(caller->barrierCount, callee->barrierCount);
        }
    }
    return RESOLVE_SUCCESS;
}

uint32_t resolveKernelDependencies(ExternalFunctionInfosT externalFunctionInfos, FuncNameToIdMapT &funcNameToId, KernelDependenciesT kernelDependencies, KernelDescriptorMapT &nameToKernelDescriptor) {
    for (size_t i = 0; i < kernelDependencies.size(); i++) {
        auto kernelDep = kernelDependencies[i];
        if (funcNameToId.count(kernelDep->usedFuncName) == 0) {
            return ERROR_EXTERNAL_FUNCTION_INFO_MISSING;
        } else if (nameToKernelDescriptor.count(kernelDep->kernelName) == 0) {
            return ERROR_KERNEL_DESCRIPTOR_MISSING;
        }
        const auto functionBarrierCount = externalFunctionInfos[funcNameToId[kernelDep->usedFuncName]]->barrierCount;
        auto &kernelBarrierCount = nameToKernelDescriptor[kernelDep->kernelName]->kernelAttributes.barrierCount;
        kernelBarrierCount = std::max(kernelBarrierCount, functionBarrierCount);
    }
    return RESOLVE_SUCCESS;
}

std::vector<size_t> DependencyResolver::resolveDependencies() {
    for (size_t i = 0; i < graph.size(); i++) {
        if (std::find(seen.begin(), seen.end(), i) == seen.end() && graph[i].empty() == false) {
            resolveDependency(i, graph[i]);
        }
    }
    if (loopDeteckted) {
        return {};
    }
    return resolved;
}

void DependencyResolver::resolveDependency(size_t nodeId, const std::vector<size_t> &edges) {
    seen.push_back(nodeId);
    for (auto &edgeId : edges) {
        if (std::find(resolved.begin(), resolved.end(), edgeId) == resolved.end()) {
            if (std::find(seen.begin(), seen.end(), edgeId) != seen.end()) {
                loopDeteckted = true;
            } else {
                resolveDependency(edgeId, graph[edgeId]);
            }
        }
    }
    resolved.push_back(nodeId);
}
} // namespace NEO