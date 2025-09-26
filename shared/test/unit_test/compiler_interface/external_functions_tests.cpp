/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include "gtest/gtest.h"

#include <vector>

using namespace NEO;

TEST(DependencyResolverTests, GivenEmptyGraphReturnEmptyResolve) {
    std::vector<std::vector<size_t>> graph = {};
    DependencyResolver resolver(graph);
    const auto &resolve = resolver.resolveDependencies();
    EXPECT_TRUE(resolve.empty());
}

TEST(DependencyResolverTests, GivenGraphWithLoopReturnCorrectResolve) {
    /*
        0 -> 1
        ^    |
        |    v
        3 <- 2
    */
    std::vector<std::vector<size_t>> graph = {{1}, {2}, {3}, {0}};
    std::vector<size_t> expectedResolve = {3, 2, 1, 0};
    DependencyResolver resolver(graph);
    const auto &resolve = resolver.resolveDependencies();
    EXPECT_EQ(expectedResolve, resolve);
}

TEST(DependencyResolverTests, GivenGraphWithNodeConnectedToItselfThenReturnCorrectResolve) {
    /*
        0-->
        ^  |
        |<-v
    */
    std::vector<std::vector<size_t>> graph = {{0}};
    std::vector<size_t> expectedResolve = {0};
    DependencyResolver resolver(graph);
    const auto &resolve = resolver.resolveDependencies();
    EXPECT_EQ(expectedResolve, resolve);
}

TEST(DependencyResolverTests, GivenOneConnectedGraphReturnCorrectResolve) {
    /*
        0 -> 1 -> 2
             ^
             |
             3 -> 4
    */
    std::vector<std::vector<size_t>> graph = {{1}, {2}, {}, {1, 4}, {}};
    std::vector<size_t> expectedResolve = {2, 1, 0, 4, 3};
    DependencyResolver resolver(graph);
    const auto &resolve = resolver.resolveDependencies();
    EXPECT_EQ(expectedResolve, resolve);
}

TEST(DependencyResolverTests, GivenMultipleDisconnectedGraphsReturnCorrectResolve) {
    /*
       0
       1 -> 2 -> 5
       4 -> 3
    */
    std::vector<std::vector<size_t>> graph = {{}, {2}, {5}, {}, {3}, {}};
    std::vector<size_t> expectedResolve = {0, 5, 2, 1, 3, 4};
    DependencyResolver resolver(graph);
    const auto &resolve = resolver.resolveDependencies();
    EXPECT_EQ(expectedResolve, resolve);
}

struct ExternalFunctionsTests : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}

    struct FunctionInfo {
        uint8_t barrierCount;
        bool hasRTCalls;
        bool hasPrintfCalls;
        bool hasIndirectCalls;
    };

    void addExternalFunction(const std::string &functionName, FunctionInfo functionInfo) {
        funcNameToId[functionName] = extFuncInfoStorage.size();
        extFuncInfoStorage.push_back(ExternalFunctionInfo{functionName, functionInfo.barrierCount, 128U, 8U, functionInfo.hasRTCalls, functionInfo.hasPrintfCalls, functionInfo.hasIndirectCalls});
    }
    void addKernel(const std::string &kernelName) {
        kernelDescriptorStorage.push_back(std::make_unique<KernelDescriptor>());
        auto &kd = *kernelDescriptorStorage.rbegin();
        kd->kernelMetadata.kernelName = kernelName;
        nameToKernelDescriptor[kernelName] = kd.get();
    }
    void addFuncDependency(const std::string &calleeName, const std::string &callerName) {
        funcDependenciesStorage.push_back({calleeName, callerName, false});
    }
    void addKernelDependency(const std::string &calleeName, const std::string &kernelCallerName) {
        kernelDependenciesStorage.push_back({calleeName, kernelCallerName, false});
    }
    void addOptionalFuncDependency(const std::string &calleeName, const std::string &callerName) {
        funcDependenciesStorage.push_back({calleeName, callerName, true});
    }
    void addOptionalKernelDependency(const std::string &calleeName, const std::string &kernelCallerName) {
        kernelDependenciesStorage.push_back({calleeName, kernelCallerName, true});
    }

    void clear() {
        extFuncInfoStorage.clear();
        kernelDependenciesStorage.clear();
        funcDependenciesStorage.clear();
        kernelDescriptorStorage.clear();
        funcNameToId.clear();
        nameToKernelDescriptor.clear();
    }
    void set() {
        auto toPtrVec = [](auto &inVec, auto &outPtrVec) {
            outPtrVec.resize(inVec.size());
            for (size_t i = 0; i < inVec.size(); i++) {
                outPtrVec[i] = &inVec[i];
            }
        };
        toPtrVec(extFuncInfoStorage, extFuncInfo);
        toPtrVec(funcDependenciesStorage, functionDependencies);
        toPtrVec(kernelDependenciesStorage, kernelDependencies);
    }

    ExternalFunctionInfosT extFuncInfo;
    FunctionDependenciesT functionDependencies;
    KernelDependenciesT kernelDependencies;
    KernelDescriptorMapT nameToKernelDescriptor;
    FuncNameToIdMapT funcNameToId;

  protected:
    std::vector<ExternalFunctionInfo> extFuncInfoStorage;
    std::vector<ExternalFunctionUsageKernel> kernelDependenciesStorage;
    std::vector<ExternalFunctionUsageExtFunc> funcDependenciesStorage;
    std::vector<std::unique_ptr<KernelDescriptor>> kernelDescriptorStorage;
};

TEST_F(ExternalFunctionsTests, GivenMissingExtFuncInLookupMapWhenResolvingExtFuncDependenciesThenReturnError) {
    addFuncDependency("fun1", "fun0");
    set();
    auto error = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(ERROR_EXTERNAL_FUNCTION_INFO_MISSING, error);
    clear();

    addFuncDependency("fun1", "fun0");
    addExternalFunction("fun1", {});
    set();
    error = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(ERROR_EXTERNAL_FUNCTION_INFO_MISSING, error);
    clear();

    addFuncDependency("fun1", "fun0");
    addExternalFunction("fun0", {});
    set();
    error = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(ERROR_EXTERNAL_FUNCTION_INFO_MISSING, error);
}

TEST_F(ExternalFunctionsTests, GivenMissingOptionalExtFuncInLookupMapWhenResolvingExtFuncDependenciesThenReturnSuccess) {
    addOptionalFuncDependency("fun1", "fun0");
    set();
    auto error = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    clear();

    addOptionalFuncDependency("fun1", "fun0");
    addExternalFunction("fun1", {});
    set();
    error = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    clear();

    addOptionalFuncDependency("fun1", "fun0");
    addExternalFunction("fun0", {});
    set();
    error = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
}

TEST_F(ExternalFunctionsTests, GivenMissingExtFuncInLookupMapWhenResolvingKernelDependenciesThenReturnError) {
    addKernel("kernel");
    addKernelDependency("fun0", "kernel");
    set();
    auto error = resolveKernelDependencies(extFuncInfo, funcNameToId, kernelDependencies, nameToKernelDescriptor);
    EXPECT_EQ(ERROR_EXTERNAL_FUNCTION_INFO_MISSING, error);
}
TEST_F(ExternalFunctionsTests, GivenMissingOptionalExtFuncInLookupMapWhenResolvingKernelDependenciesThenReturnSuccess) {
    addKernel("kernel");
    addOptionalKernelDependency("fun0", "kernel");
    set();
    auto error = resolveKernelDependencies(extFuncInfo, funcNameToId, kernelDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
}

TEST_F(ExternalFunctionsTests, GivenMissingKernelInLookupMapWhenResolvingKernelDependenciesThenReturnError) {
    addExternalFunction("fun0", {});
    addKernelDependency("fun0", "kernel");
    set();
    auto error = resolveKernelDependencies(extFuncInfo, funcNameToId, kernelDependencies, nameToKernelDescriptor);
    EXPECT_EQ(ERROR_KERNEL_DESCRIPTOR_MISSING, error);
}

TEST_F(ExternalFunctionsTests, GivenNoDependenciesWhenResolvingBarrierCountThenReturnSuccess) {
    set();
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
}

TEST_F(ExternalFunctionsTests, GivenMissingExtFuncInExtFuncDependenciesWhenResolvingBarrierCountThenReturnError) {
    addFuncDependency("fun0", "fun1");
    set();
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(ERROR_EXTERNAL_FUNCTION_INFO_MISSING, error);
}

TEST_F(ExternalFunctionsTests, GivenMissingExtFuncInKernelDependenciesWhenResolvingBarrierCountThenReturnError) {
    addKernelDependency("fun0", "kernel");
    addKernel("kernel");
    set();
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(ERROR_EXTERNAL_FUNCTION_INFO_MISSING, error);
}

TEST_F(ExternalFunctionsTests, GivenLoopWhenResolvingExtFuncDependenciesThenReturnSuccess) {
    addExternalFunction("fun0", {.barrierCount = 4});
    addExternalFunction("fun1", {.barrierCount = 2});
    addFuncDependency("fun0", "fun1");
    addFuncDependency("fun1", "fun0");
    set();
    auto retVal = resolveExtFuncDependencies(extFuncInfo, funcNameToId, functionDependencies);
    EXPECT_EQ(RESOLVE_SUCCESS, retVal);
    EXPECT_EQ(4U, extFuncInfo[funcNameToId["fun0"]]->barrierCount);
    EXPECT_EQ(4U, extFuncInfo[funcNameToId["fun1"]]->barrierCount);
}

TEST_F(ExternalFunctionsTests, GivenValidFunctionAndKernelDependenciesWhenResolvingDependenciesThenSetAppropriateBarrierCountAndReturnSuccess) {
    addKernel("kernel");
    addExternalFunction("fun0", {.barrierCount = 1});
    addExternalFunction("fun1", {.barrierCount = 2});
    addFuncDependency("fun1", "fun0");
    addKernelDependency("fun0", "kernel");
    set();
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    EXPECT_EQ(2U, extFuncInfo[funcNameToId["fun0"]]->barrierCount);
    EXPECT_EQ(2U, extFuncInfo[funcNameToId["fun1"]]->barrierCount);
    EXPECT_EQ(2U, nameToKernelDescriptor["kernel"]->kernelAttributes.barrierCount);
}

TEST_F(ExternalFunctionsTests, GivenValidFunctionAndKernelDependenciesWhenResolvingDependenciesThenSetAppropriateHasRTCallsAndReturnSuccess) {
    addKernel("kernel0");
    addKernel("kernel1");
    addKernel("kernel2");
    addExternalFunction("fun0", {.hasRTCalls = false});
    addExternalFunction("fun1", {.hasRTCalls = true});
    addExternalFunction("fun2", {.hasRTCalls = false});

    addFuncDependency("fun1", "fun0");
    addKernelDependency("fun0", "kernel0");
    addKernelDependency("fun2", "kernel1");
    addKernelDependency("fun2", "kernel2");
    set();

    nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasRTCalls = true;
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun0"]]->hasRTCalls);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun1"]]->hasRTCalls);
    EXPECT_FALSE(extFuncInfo[funcNameToId["fun2"]]->hasRTCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel0"]->kernelAttributes.flags.hasRTCalls);
    EXPECT_FALSE(nameToKernelDescriptor["kernel1"]->kernelAttributes.flags.hasRTCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasRTCalls);
}

TEST_F(ExternalFunctionsTests, GivenValidFunctionAndKernelDependenciesWhenResolvingDependenciesThenSetAppropriateHasPrintfCallsAndReturnSuccess) {
    addKernel("kernel0");
    addKernel("kernel1");
    addKernel("kernel2");
    addExternalFunction("fun0", {.hasPrintfCalls = false});
    addExternalFunction("fun1", {.hasPrintfCalls = true});
    addExternalFunction("fun2", {.hasPrintfCalls = false});

    addFuncDependency("fun1", "fun0");
    addKernelDependency("fun0", "kernel0");
    addKernelDependency("fun2", "kernel1");
    addKernelDependency("fun2", "kernel2");
    set();

    nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasPrintfCalls = true;
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun0"]]->hasPrintfCalls);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun1"]]->hasPrintfCalls);
    EXPECT_FALSE(extFuncInfo[funcNameToId["fun2"]]->hasPrintfCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel0"]->kernelAttributes.flags.hasPrintfCalls);
    EXPECT_FALSE(nameToKernelDescriptor["kernel1"]->kernelAttributes.flags.hasPrintfCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasPrintfCalls);
}

TEST_F(ExternalFunctionsTests, GivenValidFunctionAndKernelDependenciesWhenResolvingDependenciesThenSetAppropriateHasIndirectfCallsAndReturnSuccess) {
    addKernel("kernel0");
    addKernel("kernel1");
    addKernel("kernel2");
    addExternalFunction("fun0", {.hasIndirectCalls = false});
    addExternalFunction("fun1", {.hasIndirectCalls = true});
    addExternalFunction("fun2", {.hasIndirectCalls = false});

    addFuncDependency("fun1", "fun0");
    addKernelDependency("fun0", "kernel0");
    addKernelDependency("fun2", "kernel1");
    addKernelDependency("fun2", "kernel2");
    set();

    nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasIndirectCalls = true;
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun0"]]->hasIndirectCalls);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun1"]]->hasIndirectCalls);
    EXPECT_FALSE(extFuncInfo[funcNameToId["fun2"]]->hasIndirectCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel0"]->kernelAttributes.flags.hasIndirectCalls);
    EXPECT_FALSE(nameToKernelDescriptor["kernel1"]->kernelAttributes.flags.hasIndirectCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasIndirectCalls);
}

TEST_F(ExternalFunctionsTests, GivenValidFunctionAndKernelOptionalDependenciesWhenResolvingDependenciesThenSetAppropriateHasIndirectfCallsAndReturnSuccess) {
    addKernel("kernel0");
    addKernel("kernel1");
    addKernel("kernel2");
    addExternalFunction("fun0", {.hasIndirectCalls = false});
    addExternalFunction("fun1", {.hasIndirectCalls = true});
    addExternalFunction("fun2", {.hasIndirectCalls = false});

    addOptionalFuncDependency("fun1", "fun0");
    addOptionalKernelDependency("fun0", "kernel0");
    addOptionalKernelDependency("fun2", "kernel1");
    addOptionalKernelDependency("fun2", "kernel2");
    set();

    nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasIndirectCalls = true;
    auto error = resolveExternalDependencies(extFuncInfo, kernelDependencies, functionDependencies, nameToKernelDescriptor);
    EXPECT_EQ(RESOLVE_SUCCESS, error);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun0"]]->hasIndirectCalls);
    EXPECT_TRUE(extFuncInfo[funcNameToId["fun1"]]->hasIndirectCalls);
    EXPECT_FALSE(extFuncInfo[funcNameToId["fun2"]]->hasIndirectCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel0"]->kernelAttributes.flags.hasIndirectCalls);
    EXPECT_FALSE(nameToKernelDescriptor["kernel1"]->kernelAttributes.flags.hasIndirectCalls);
    EXPECT_TRUE(nameToKernelDescriptor["kernel2"]->kernelAttributes.flags.hasIndirectCalls);
}
