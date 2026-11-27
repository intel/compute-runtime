/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"
#include "zello_graph_test_helper.h"
#include "zello_test_duration.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ZeGraphGetNodesExpFuncPtr = decltype(&zeGraphGetNodesExp);
using ZeGraphNodesUpdateExpFuncPtr = decltype(&zeGraphNodesUpdateExp);
using ZeExecutableGraphNodesUpdateExpFuncPtr = decltype(&zeExecutableGraphNodesUpdateExp);
using ZeExecutableGraphUpdateExpFuncPtr = decltype(&zeExecutableGraphUpdateExp);

struct MutableGraphApi {
    ZeGraphGetNodesExpFuncPtr graphGetNodes = nullptr;
    ZeGraphNodesUpdateExpFuncPtr graphNodesUpdate = nullptr;
    ZeExecutableGraphNodesUpdateExpFuncPtr executableGraphNodesUpdate = nullptr;
    ZeExecutableGraphUpdateExpFuncPtr executableGraphUpdate = nullptr;
};

struct MutableGraphKernels {
    ze_kernel_handle_t add = nullptr;
    ze_kernel_handle_t mul = nullptr;
};

enum class UpdateDescriptorKind {
    fineGrain,
    aggregate
};

enum class MutationMode {
    mutateGraphInstantiateNewExecutableGraph,
    mutateGraphUpdateExecutableGraph,
    mutateExecutableGraph
};

struct KernelArguments {
    void *source = nullptr;
    void *destination = nullptr;
    int32_t value = 0;
};

constexpr uint32_t elementCount = 256U;
constexpr uint32_t halfElementCount = elementCount / 2U;
constexpr uint32_t quarterElementCount = elementCount / 4U;
constexpr size_t bufferSize = elementCount * sizeof(int32_t);
constexpr uint32_t fullGroupSize = 64U;
constexpr uint32_t halfGroupSize = fullGroupSize / 2U;
constexpr uint32_t fullGroupCount = elementCount / fullGroupSize;
constexpr uint32_t halfGroupCount = fullGroupCount / 2U;
constexpr ze_group_count_t fullGroupCountShape = {fullGroupCount, 1U, 1U};
constexpr ze_group_count_t halfGroupCountShape = {halfGroupCount, 1U, 1U};
constexpr ze_group_size_t fullGroupSizeShape = {fullGroupSize, 1U, 1U};
constexpr ze_group_size_t halfGroupSizeShape = {halfGroupSize, 1U, 1U};

constexpr uint32_t sourceArgumentIndex = 0U;
constexpr uint32_t destinationArgumentIndex = 1U;
constexpr uint32_t valueArgumentIndex = 2U;
constexpr uint32_t kernelArgumentsCount = 3U;
constexpr std::array<size_t, kernelArgumentsCount> kernelArgumentSizes = {sizeof(void *), sizeof(void *), sizeof(int32_t)};

constexpr uint32_t addNodeIndex = 0U;
constexpr uint32_t mulNodeIndex = 1U;
constexpr uint32_t capturedNodesCount = 2U;

constexpr ze_mutable_graph_node_exp_flags_t kernelArgumentsAndDispatchShapesFlags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT |
                                                                                    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT |
                                                                                    ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE;

using NodeHandles = std::array<ze_graph_node_handle_t, capturedNodesCount>;
using NodeUpdateDescriptors = std::array<const void *, capturedNodesCount>;

bool loadMutableGraphApi(ze_driver_handle_t driverHandle, MutableGraphApi &api) {
    bool loaded = (ZE_RESULT_SUCCESS == zeDriverGetExtensionFunctionAddress(driverHandle, "zeGraphGetNodesExp", reinterpret_cast<void **>(&api.graphGetNodes)));
    loaded &= (ZE_RESULT_SUCCESS == zeDriverGetExtensionFunctionAddress(driverHandle, "zeGraphNodesUpdateExp", reinterpret_cast<void **>(&api.graphNodesUpdate)));
    loaded &= (ZE_RESULT_SUCCESS == zeDriverGetExtensionFunctionAddress(driverHandle, "zeExecutableGraphNodesUpdateExp", reinterpret_cast<void **>(&api.executableGraphNodesUpdate)));
    loaded &= (ZE_RESULT_SUCCESS == zeDriverGetExtensionFunctionAddress(driverHandle, "zeExecutableGraphUpdateExp", reinterpret_cast<void **>(&api.executableGraphUpdate)));
    return loaded;
}

const char *getUpdateDescriptorKindName(UpdateDescriptorKind descriptorKind) {
    switch (descriptorKind) {
    case UpdateDescriptorKind::fineGrain:
        return "fine grain update descriptors";
    case UpdateDescriptorKind::aggregate:
        return "aggregate launch kernel update descriptor";
    }
    return "unknown update descriptor";
}

const char *getMutationModeName(MutationMode mutationMode) {
    switch (mutationMode) {
    case MutationMode::mutateGraphInstantiateNewExecutableGraph:
        return "mutate graph, instantiate new executable graph";
    case MutationMode::mutateGraphUpdateExecutableGraph:
        return "mutate graph, update executable graph from it";
    case MutationMode::mutateExecutableGraph:
        return "mutate executable graph";
    }
    return "unknown mutation mode";
}

void *allocateHostBuffer(ze_context_handle_t context, int32_t initialValue) {
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, bufferSize, bufferSize, &buffer));
    std::fill_n(static_cast<int32_t *>(buffer), elementCount, initialValue);
    return buffer;
}

void freeHostBuffers(ze_context_handle_t context, const std::vector<void *> &buffers) {
    for (auto buffer : buffers) {
        SUCCESS_OR_TERMINATE(zeMemFree(context, buffer));
    }
}

bool verifyGraphNodes(const MutableGraphApi &api, ze_graph_handle_t graph, const NodeHandles &nodes) {
    uint32_t graphNodesCount = 0U;
    SUCCESS_OR_TERMINATE(api.graphGetNodes(graph, &graphNodesCount, nullptr));
    if (capturedNodesCount != graphNodesCount) {
        std::cerr << "Graph nodes count mismatch: expected " << capturedNodesCount << ", got " << graphNodesCount << std::endl;
        return false;
    }
    NodeHandles graphNodes = {};
    SUCCESS_OR_TERMINATE(api.graphGetNodes(graph, &graphNodesCount, graphNodes.data()));
    if (nodes != graphNodes) {
        std::cerr << "Graph nodes differ from the nodes returned by the graph node descriptors" << std::endl;
        return false;
    }
    return true;
}

bool logValidation(const char *elementName, bool valid) {
    std::cout << "Validation of " << elementName << ": " << (valid ? "success" : "failure") << std::endl;
    return valid;
}

bool validateBufferSplit(const void *buffer, uint32_t splitElementCount, int32_t expectedBeforeSplit, int32_t expectedFromSplit) {
    const auto values = static_cast<const int32_t *>(buffer);
    bool valid = LevelZeroBlackBoxTests::validateToValue(expectedBeforeSplit, values, splitElementCount);
    valid &= LevelZeroBlackBoxTests::validateToValue(expectedFromSplit, values + splitElementCount, elementCount - splitElementCount);
    return valid;
}

bool testMutateSingleElementKernelArguments(const MutableGraphApi &api,
                                            ze_context_handle_t context,
                                            ze_device_handle_t device,
                                            const MutableGraphKernels &kernels,
                                            UpdateDescriptorKind descriptorKind,
                                            MutationMode mutationMode,
                                            bool aubMode,
                                            const GraphTests::GraphDumpSettings &dumpSettings,
                                            const std::string &testName) {
    constexpr int32_t initialSourceValue = 3;
    constexpr int32_t mutatedSourceValue = 7;
    constexpr int32_t initialAddValue = 5;
    constexpr int32_t mutatedAddValue = 11;
    constexpr int32_t initialMulValue = 2;
    constexpr int32_t mutatedMulValue = 4;
    constexpr int32_t clearedValue = 0;

    ze_command_list_handle_t cmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, false, false, cmdList);
    auto initialSource = allocateHostBuffer(context, initialSourceValue);
    auto mutatedSource = allocateHostBuffer(context, mutatedSourceValue);
    auto initialAddDestination = allocateHostBuffer(context, clearedValue);
    auto mutatedAddDestination = allocateHostBuffer(context, clearedValue);
    auto initialMulDestination = allocateHostBuffer(context, clearedValue);
    auto mutatedMulDestination = allocateHostBuffer(context, clearedValue);

    const KernelArguments initialAddArguments = {initialSource, initialAddDestination, initialAddValue};
    KernelArguments initialMulArguments = {initialSource, initialMulDestination, initialMulValue};
    const KernelArguments mutatedAddArguments = {mutatedSource, mutatedAddDestination, mutatedAddValue};
    const KernelArguments mutatedMulArguments = {mutatedSource, mutatedMulDestination, mutatedMulValue};

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ze_graph_handle_t graph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphCreateExt(context, &mutableGraphDesc, &graph));

    NodeHandles nodes = {};
    ze_graph_node_exp_desc_t addNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    addNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT;
    addNodeDesc.phNodeHandle = &nodes[addNodeIndex];
    ze_graph_node_exp_desc_t mulNodeDesc = addNodeDesc;
    mulNodeDesc.phNodeHandle = &nodes[mulNodeIndex];

    SUCCESS_OR_TERMINATE(zeCommandListBeginCaptureIntoGraphExt(cmdList, graph, nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernels.add, fullGroupSizeShape.groupSizeX, fullGroupSizeShape.groupSizeY, fullGroupSizeShape.groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, sourceArgumentIndex, sizeof(initialAddArguments.source), &initialAddArguments.source));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, destinationArgumentIndex, sizeof(initialAddArguments.destination), &initialAddArguments.destination));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, valueArgumentIndex, sizeof(initialAddArguments.value), &initialAddArguments.value));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithParameters(cmdList, kernels.add, &fullGroupCountShape, &addNodeDesc, nullptr, 0U, nullptr));
    std::array<void *, kernelArgumentsCount> initialMulArgumentValues = {&initialMulArguments.source, &initialMulArguments.destination, &initialMulArguments.value};
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithArguments(cmdList, kernels.mul, fullGroupCountShape, fullGroupSizeShape, initialMulArgumentValues.data(), &mulNodeDesc, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListEndGraphCaptureExt(cmdList, nullptr, nullptr));

    bool validRet = logValidation("graph nodes", verifyGraphNodes(api, graph, nodes));

    std::array<const void *, kernelArgumentsCount> mutatedAddArgumentValues = {&mutatedAddArguments.source, &mutatedAddArguments.destination, &mutatedAddArguments.value};
    std::array<const void *, kernelArgumentsCount> mutatedMulArgumentValues = {&mutatedMulArguments.source, &mutatedMulArguments.destination, &mutatedMulArguments.value};

    std::array<ze_graph_node_update_kernel_argument_exp_desc_t, kernelArgumentsCount> addArgumentDescs = {};
    std::array<ze_graph_node_update_kernel_argument_exp_desc_t, kernelArgumentsCount> mulArgumentDescs = {};
    for (uint32_t argIndex = 0U; argIndex < kernelArgumentsCount; ++argIndex) {
        addArgumentDescs[argIndex].stype = ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC;
        addArgumentDescs[argIndex].argIndex = argIndex;
        addArgumentDescs[argIndex].argSize = kernelArgumentSizes[argIndex];
        addArgumentDescs[argIndex].pArgValue = mutatedAddArgumentValues[argIndex];
        mulArgumentDescs[argIndex].stype = ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC;
        mulArgumentDescs[argIndex].argIndex = argIndex;
        mulArgumentDescs[argIndex].argSize = kernelArgumentSizes[argIndex];
        mulArgumentDescs[argIndex].pArgValue = mutatedMulArgumentValues[argIndex];
    }
    addArgumentDescs[sourceArgumentIndex].pNext = &addArgumentDescs[destinationArgumentIndex];
    addArgumentDescs[destinationArgumentIndex].pNext = &addArgumentDescs[valueArgumentIndex];
    mulArgumentDescs[sourceArgumentIndex].pNext = &mulArgumentDescs[destinationArgumentIndex];
    mulArgumentDescs[destinationArgumentIndex].pNext = &mulArgumentDescs[valueArgumentIndex];

    ze_graph_node_update_launch_kernel_exp_desc_t addLaunchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    addLaunchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    addLaunchKernelDesc.pArgValues = mutatedAddArgumentValues.data();
    ze_graph_node_update_launch_kernel_exp_desc_t mulLaunchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    mulLaunchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    mulLaunchKernelDesc.pArgValues = mutatedMulArgumentValues.data();

    NodeUpdateDescriptors updateDescs = {addArgumentDescs.data(), mulArgumentDescs.data()};
    if (UpdateDescriptorKind::aggregate == descriptorKind) {
        updateDescs = {&addLaunchKernelDesc, &mulLaunchKernelDesc};
    }

    ze_executable_graph_handle_t executableGraph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &executableGraph));
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, executableGraph, nullptr, nullptr, 0U, nullptr));

    ze_executable_graph_handle_t mutatedExecutableGraph = executableGraph;
    switch (mutationMode) {
    case MutationMode::mutateGraphInstantiateNewExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &mutatedExecutableGraph));
        break;
    case MutationMode::mutateGraphUpdateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(api.executableGraphUpdate(executableGraph, graph, nullptr));
        break;
    case MutationMode::mutateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.executableGraphNodesUpdate(executableGraph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        break;
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, mutatedExecutableGraph, nullptr, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    if (mutatedExecutableGraph != executableGraph) {
        SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(mutatedExecutableGraph));
    }
    SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(executableGraph));

    if (false == aubMode) {
        validRet &= logValidation("add node initial destination", LevelZeroBlackBoxTests::validateToValue(initialSourceValue + initialAddValue, initialAddDestination, elementCount));
        validRet &= logValidation("mul node initial destination", LevelZeroBlackBoxTests::validateToValue(initialSourceValue * initialMulValue, initialMulDestination, elementCount));
        validRet &= logValidation("add node mutated destination", LevelZeroBlackBoxTests::validateToValue(mutatedSourceValue + mutatedAddValue, mutatedAddDestination, elementCount));
        validRet &= logValidation("mul node mutated destination", LevelZeroBlackBoxTests::validateToValue(mutatedSourceValue * mutatedMulValue, mutatedMulDestination, elementCount));
    }
    GraphTests::dumpGraphToDotIfEnabled(graph, testName, dumpSettings);

    SUCCESS_OR_TERMINATE(zeGraphDestroyExt(graph));
    freeHostBuffers(context, {initialSource, mutatedSource, initialAddDestination, mutatedAddDestination, initialMulDestination, mutatedMulDestination});
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    return validRet;
}

bool testMutateSingleElementGroupCount(const MutableGraphApi &api,
                                       ze_context_handle_t context,
                                       ze_device_handle_t device,
                                       const MutableGraphKernels &kernels,
                                       UpdateDescriptorKind descriptorKind,
                                       MutationMode mutationMode,
                                       bool aubMode,
                                       const GraphTests::GraphDumpSettings &dumpSettings,
                                       const std::string &testName) {
    constexpr int32_t initialValue = 3;
    constexpr int32_t addValue = 5;
    constexpr int32_t mulValue = 2;
    const ze_group_count_t initialGroupCount = halfGroupCountShape;
    const ze_group_count_t mutatedGroupCount = fullGroupCountShape;

    ze_command_list_handle_t cmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, false, false, cmdList);
    auto addBuffer = allocateHostBuffer(context, initialValue);
    auto mulBuffer = allocateHostBuffer(context, initialValue);

    const KernelArguments addArguments = {addBuffer, addBuffer, addValue};
    KernelArguments mulArguments = {mulBuffer, mulBuffer, mulValue};

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ze_graph_handle_t graph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphCreateExt(context, &mutableGraphDesc, &graph));

    NodeHandles nodes = {};
    ze_graph_node_exp_desc_t addNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    addNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT;
    addNodeDesc.phNodeHandle = &nodes[addNodeIndex];
    ze_graph_node_exp_desc_t mulNodeDesc = addNodeDesc;
    mulNodeDesc.phNodeHandle = &nodes[mulNodeIndex];

    SUCCESS_OR_TERMINATE(zeCommandListBeginCaptureIntoGraphExt(cmdList, graph, nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernels.add, fullGroupSizeShape.groupSizeX, fullGroupSizeShape.groupSizeY, fullGroupSizeShape.groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, sourceArgumentIndex, sizeof(addArguments.source), &addArguments.source));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, destinationArgumentIndex, sizeof(addArguments.destination), &addArguments.destination));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, valueArgumentIndex, sizeof(addArguments.value), &addArguments.value));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithParameters(cmdList, kernels.add, &initialGroupCount, &addNodeDesc, nullptr, 0U, nullptr));
    std::array<void *, kernelArgumentsCount> mulArgumentValues = {&mulArguments.source, &mulArguments.destination, &mulArguments.value};
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithArguments(cmdList, kernels.mul, initialGroupCount, fullGroupSizeShape, mulArgumentValues.data(), &mulNodeDesc, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListEndGraphCaptureExt(cmdList, nullptr, nullptr));

    bool validRet = logValidation("graph nodes", verifyGraphNodes(api, graph, nodes));

    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = mutatedGroupCount;
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupCount = mutatedGroupCount;

    NodeUpdateDescriptors updateDescs = {&groupCountDesc, &groupCountDesc};
    if (UpdateDescriptorKind::aggregate == descriptorKind) {
        updateDescs = {&launchKernelDesc, &launchKernelDesc};
    }

    ze_executable_graph_handle_t executableGraph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &executableGraph));
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, executableGraph, nullptr, nullptr, 0U, nullptr));

    ze_executable_graph_handle_t mutatedExecutableGraph = executableGraph;
    switch (mutationMode) {
    case MutationMode::mutateGraphInstantiateNewExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &mutatedExecutableGraph));
        break;
    case MutationMode::mutateGraphUpdateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(api.executableGraphUpdate(executableGraph, graph, nullptr));
        break;
    case MutationMode::mutateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.executableGraphNodesUpdate(executableGraph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        break;
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, mutatedExecutableGraph, nullptr, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    if (mutatedExecutableGraph != executableGraph) {
        SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(mutatedExecutableGraph));
    }
    SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(executableGraph));

    if (false == aubMode) {
        validRet &= logValidation("add node buffer", validateBufferSplit(addBuffer, halfElementCount, initialValue + addValue + addValue, initialValue + addValue));
        validRet &= logValidation("mul node buffer", validateBufferSplit(mulBuffer, halfElementCount, initialValue * mulValue * mulValue, initialValue * mulValue));
    }
    GraphTests::dumpGraphToDotIfEnabled(graph, testName, dumpSettings);

    SUCCESS_OR_TERMINATE(zeGraphDestroyExt(graph));
    freeHostBuffers(context, {addBuffer, mulBuffer});
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    return validRet;
}

bool testMutateSingleElementGroupSize(const MutableGraphApi &api,
                                      ze_context_handle_t context,
                                      ze_device_handle_t device,
                                      const MutableGraphKernels &kernels,
                                      UpdateDescriptorKind descriptorKind,
                                      MutationMode mutationMode,
                                      bool aubMode,
                                      const GraphTests::GraphDumpSettings &dumpSettings,
                                      const std::string &testName) {
    constexpr int32_t initialValue = 3;
    constexpr int32_t addValue = 5;
    constexpr int32_t mulValue = 2;
    const ze_group_size_t initialGroupSize = halfGroupSizeShape;
    const ze_group_size_t mutatedGroupSize = fullGroupSizeShape;

    ze_command_list_handle_t cmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, false, false, cmdList);
    auto addBuffer = allocateHostBuffer(context, initialValue);
    auto mulBuffer = allocateHostBuffer(context, initialValue);

    const KernelArguments addArguments = {addBuffer, addBuffer, addValue};
    KernelArguments mulArguments = {mulBuffer, mulBuffer, mulValue};

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ze_graph_handle_t graph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphCreateExt(context, &mutableGraphDesc, &graph));

    NodeHandles nodes = {};
    ze_graph_node_exp_desc_t addNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    addNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE;
    addNodeDesc.phNodeHandle = &nodes[addNodeIndex];
    ze_graph_node_exp_desc_t mulNodeDesc = addNodeDesc;
    mulNodeDesc.phNodeHandle = &nodes[mulNodeIndex];

    SUCCESS_OR_TERMINATE(zeCommandListBeginCaptureIntoGraphExt(cmdList, graph, nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernels.add, initialGroupSize.groupSizeX, initialGroupSize.groupSizeY, initialGroupSize.groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, sourceArgumentIndex, sizeof(addArguments.source), &addArguments.source));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, destinationArgumentIndex, sizeof(addArguments.destination), &addArguments.destination));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, valueArgumentIndex, sizeof(addArguments.value), &addArguments.value));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithParameters(cmdList, kernels.add, &fullGroupCountShape, &addNodeDesc, nullptr, 0U, nullptr));
    std::array<void *, kernelArgumentsCount> mulArgumentValues = {&mulArguments.source, &mulArguments.destination, &mulArguments.value};
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithArguments(cmdList, kernels.mul, fullGroupCountShape, initialGroupSize, mulArgumentValues.data(), &mulNodeDesc, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListEndGraphCaptureExt(cmdList, nullptr, nullptr));

    bool validRet = logValidation("graph nodes", verifyGraphNodes(api, graph, nodes));

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = mutatedGroupSize;
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupSize = mutatedGroupSize;

    NodeUpdateDescriptors updateDescs = {&groupSizeDesc, &groupSizeDesc};
    if (UpdateDescriptorKind::aggregate == descriptorKind) {
        updateDescs = {&launchKernelDesc, &launchKernelDesc};
    }

    ze_executable_graph_handle_t executableGraph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &executableGraph));
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, executableGraph, nullptr, nullptr, 0U, nullptr));

    ze_executable_graph_handle_t mutatedExecutableGraph = executableGraph;
    switch (mutationMode) {
    case MutationMode::mutateGraphInstantiateNewExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &mutatedExecutableGraph));
        break;
    case MutationMode::mutateGraphUpdateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(api.executableGraphUpdate(executableGraph, graph, nullptr));
        break;
    case MutationMode::mutateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.executableGraphNodesUpdate(executableGraph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        break;
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, mutatedExecutableGraph, nullptr, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    if (mutatedExecutableGraph != executableGraph) {
        SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(mutatedExecutableGraph));
    }
    SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(executableGraph));

    if (false == aubMode) {
        validRet &= logValidation("add node buffer", validateBufferSplit(addBuffer, halfElementCount, initialValue + addValue + addValue, initialValue + addValue));
        validRet &= logValidation("mul node buffer", validateBufferSplit(mulBuffer, halfElementCount, initialValue * mulValue * mulValue, initialValue * mulValue));
    }
    GraphTests::dumpGraphToDotIfEnabled(graph, testName, dumpSettings);

    SUCCESS_OR_TERMINATE(zeGraphDestroyExt(graph));
    freeHostBuffers(context, {addBuffer, mulBuffer});
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    return validRet;
}

bool testMutateKernelArgumentsAndDispatchShapes(const MutableGraphApi &api,
                                                ze_context_handle_t context,
                                                ze_device_handle_t device,
                                                const MutableGraphKernels &kernels,
                                                UpdateDescriptorKind descriptorKind,
                                                MutationMode mutationMode,
                                                bool aubMode,
                                                const GraphTests::GraphDumpSettings &dumpSettings,
                                                const std::string &testName) {
    constexpr int32_t initialSourceValue = 3;
    constexpr int32_t mutatedSourceValue = 7;
    constexpr int32_t initialAddValue = 5;
    constexpr int32_t mutatedAddValue = 11;
    constexpr int32_t initialMulValue = 2;
    constexpr int32_t mutatedMulValue = 4;
    constexpr int32_t clearedValue = 0;
    const ze_group_count_t mutatedGroupCount = fullGroupCountShape;
    const ze_group_size_t mutatedGroupSize = fullGroupSizeShape;

    ze_command_list_handle_t cmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, false, false, cmdList);
    auto initialSource = allocateHostBuffer(context, initialSourceValue);
    auto mutatedSource = allocateHostBuffer(context, mutatedSourceValue);
    auto initialAddDestination = allocateHostBuffer(context, clearedValue);
    auto mutatedAddDestination = allocateHostBuffer(context, clearedValue);
    auto initialMulDestination = allocateHostBuffer(context, clearedValue);
    auto mutatedMulDestination = allocateHostBuffer(context, clearedValue);

    const KernelArguments initialAddArguments = {initialSource, initialAddDestination, initialAddValue};
    KernelArguments initialMulArguments = {initialSource, initialMulDestination, initialMulValue};
    const KernelArguments mutatedAddArguments = {mutatedSource, mutatedAddDestination, mutatedAddValue};
    const KernelArguments mutatedMulArguments = {mutatedSource, mutatedMulDestination, mutatedMulValue};

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ze_graph_handle_t graph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphCreateExt(context, &mutableGraphDesc, &graph));

    NodeHandles nodes = {};
    ze_graph_node_exp_desc_t addNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    addNodeDesc.flags = kernelArgumentsAndDispatchShapesFlags;
    addNodeDesc.phNodeHandle = &nodes[addNodeIndex];
    ze_graph_node_exp_desc_t mulNodeDesc = addNodeDesc;
    mulNodeDesc.phNodeHandle = &nodes[mulNodeIndex];

    SUCCESS_OR_TERMINATE(zeCommandListBeginCaptureIntoGraphExt(cmdList, graph, nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernels.add, halfGroupSizeShape.groupSizeX, halfGroupSizeShape.groupSizeY, halfGroupSizeShape.groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, sourceArgumentIndex, sizeof(initialAddArguments.source), &initialAddArguments.source));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, destinationArgumentIndex, sizeof(initialAddArguments.destination), &initialAddArguments.destination));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels.add, valueArgumentIndex, sizeof(initialAddArguments.value), &initialAddArguments.value));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithParameters(cmdList, kernels.add, &halfGroupCountShape, &addNodeDesc, nullptr, 0U, nullptr));
    std::array<void *, kernelArgumentsCount> initialMulArgumentValues = {&initialMulArguments.source, &initialMulArguments.destination, &initialMulArguments.value};
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithArguments(cmdList, kernels.mul, halfGroupCountShape, halfGroupSizeShape, initialMulArgumentValues.data(), &mulNodeDesc, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListEndGraphCaptureExt(cmdList, nullptr, nullptr));

    bool validRet = logValidation("graph nodes", verifyGraphNodes(api, graph, nodes));

    std::array<const void *, kernelArgumentsCount> mutatedAddArgumentValues = {&mutatedAddArguments.source, &mutatedAddArguments.destination, &mutatedAddArguments.value};
    std::array<const void *, kernelArgumentsCount> mutatedMulArgumentValues = {&mutatedMulArguments.source, &mutatedMulArguments.destination, &mutatedMulArguments.value};

    std::array<ze_graph_node_update_kernel_argument_exp_desc_t, kernelArgumentsCount> addArgumentDescs = {};
    std::array<ze_graph_node_update_kernel_argument_exp_desc_t, kernelArgumentsCount> mulArgumentDescs = {};
    for (uint32_t argIndex = 0U; argIndex < kernelArgumentsCount; ++argIndex) {
        addArgumentDescs[argIndex].stype = ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC;
        addArgumentDescs[argIndex].argIndex = argIndex;
        addArgumentDescs[argIndex].argSize = kernelArgumentSizes[argIndex];
        addArgumentDescs[argIndex].pArgValue = mutatedAddArgumentValues[argIndex];
        mulArgumentDescs[argIndex].stype = ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC;
        mulArgumentDescs[argIndex].argIndex = argIndex;
        mulArgumentDescs[argIndex].argSize = kernelArgumentSizes[argIndex];
        mulArgumentDescs[argIndex].pArgValue = mutatedMulArgumentValues[argIndex];
    }
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.groupCount = mutatedGroupCount;
    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = mutatedGroupSize;

    addArgumentDescs[sourceArgumentIndex].pNext = &addArgumentDescs[destinationArgumentIndex];
    addArgumentDescs[destinationArgumentIndex].pNext = &addArgumentDescs[valueArgumentIndex];
    addArgumentDescs[valueArgumentIndex].pNext = &groupCountDesc;
    mulArgumentDescs[sourceArgumentIndex].pNext = &mulArgumentDescs[destinationArgumentIndex];
    mulArgumentDescs[destinationArgumentIndex].pNext = &mulArgumentDescs[valueArgumentIndex];
    mulArgumentDescs[valueArgumentIndex].pNext = &groupCountDesc;
    groupCountDesc.pNext = &groupSizeDesc;

    ze_graph_node_update_launch_kernel_exp_desc_t addLaunchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    addLaunchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    addLaunchKernelDesc.pArgValues = mutatedAddArgumentValues.data();
    addLaunchKernelDesc.groupCount = mutatedGroupCount;
    addLaunchKernelDesc.groupSize = mutatedGroupSize;
    ze_graph_node_update_launch_kernel_exp_desc_t mulLaunchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    mulLaunchKernelDesc.pArgSizes = kernelArgumentSizes.data();
    mulLaunchKernelDesc.pArgValues = mutatedMulArgumentValues.data();
    mulLaunchKernelDesc.groupCount = mutatedGroupCount;
    mulLaunchKernelDesc.groupSize = mutatedGroupSize;

    NodeUpdateDescriptors updateDescs = {addArgumentDescs.data(), mulArgumentDescs.data()};
    if (UpdateDescriptorKind::aggregate == descriptorKind) {
        updateDescs = {&addLaunchKernelDesc, &mulLaunchKernelDesc};
    }

    ze_executable_graph_handle_t executableGraph = nullptr;
    SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &executableGraph));
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, executableGraph, nullptr, nullptr, 0U, nullptr));

    ze_executable_graph_handle_t mutatedExecutableGraph = executableGraph;
    switch (mutationMode) {
    case MutationMode::mutateGraphInstantiateNewExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &mutatedExecutableGraph));
        break;
    case MutationMode::mutateGraphUpdateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.graphNodesUpdate(graph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        SUCCESS_OR_TERMINATE(api.executableGraphUpdate(executableGraph, graph, nullptr));
        break;
    case MutationMode::mutateExecutableGraph:
        SUCCESS_OR_TERMINATE(api.executableGraphNodesUpdate(executableGraph, capturedNodesCount, nodes.data(), updateDescs.data(), nullptr));
        break;
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(cmdList, mutatedExecutableGraph, nullptr, nullptr, 0U, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    if (mutatedExecutableGraph != executableGraph) {
        SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(mutatedExecutableGraph));
    }
    SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(executableGraph));

    if (false == aubMode) {
        validRet &= logValidation("add node initial destination", validateBufferSplit(initialAddDestination, quarterElementCount, initialSourceValue + initialAddValue, clearedValue));
        validRet &= logValidation("mul node initial destination", validateBufferSplit(initialMulDestination, quarterElementCount, initialSourceValue * initialMulValue, clearedValue));
        validRet &= logValidation("add node mutated destination", LevelZeroBlackBoxTests::validateToValue(mutatedSourceValue + mutatedAddValue, mutatedAddDestination, elementCount));
        validRet &= logValidation("mul node mutated destination", LevelZeroBlackBoxTests::validateToValue(mutatedSourceValue * mutatedMulValue, mutatedMulDestination, elementCount));
    }
    GraphTests::dumpGraphToDotIfEnabled(graph, testName, dumpSettings);

    SUCCESS_OR_TERMINATE(zeGraphDestroyExt(graph));
    freeHostBuffers(context, {initialSource, mutatedSource, initialAddDestination, mutatedAddDestination, initialMulDestination, mutatedMulDestination});
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    return validRet;
}

} // namespace

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestMutateKernelArguments = 0U;
    constexpr uint32_t bitNumberTestMutateGroupCount = 1U;
    constexpr uint32_t bitNumberTestMutateGroupSize = 2U;
    constexpr uint32_t bitNumberTestMutateKernelArgumentsAndDispatchShapes = 3U;

    LevelZeroBlackBoxTests::TestDuration testDuration = LevelZeroBlackBoxTests::TestDuration(argc, argv);

    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, std::numeric_limits<uint32_t>::max());
    LevelZeroBlackBoxTests::TestBitMask testSubMask = LevelZeroBlackBoxTests::getTestSubmask(argc, argv, std::numeric_limits<uint32_t>::max());
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    const GraphTests::GraphDumpSettings dumpSettings = GraphTests::getGraphDumpSettings(argc, argv);

    const std::string blackBoxName("Zello Mutable Graph");

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device0 = devices[0];

    ze_mutable_graph_exp_properties_t mutableGraphProperties = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_PROPERTIES};
    ze_record_replay_graph_ext_properties_t graphProperties = {ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXT_PROPERTIES};
    graphProperties.pNext = &mutableGraphProperties;
    ze_device_properties_t device0Properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device0Properties.pNext = &graphProperties;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &device0Properties));
    LevelZeroBlackBoxTests::printDeviceProperties(device0Properties);

    if (false == GraphTests::isGraphExtensionPresent(driverHandle)) {
        std::cerr << "Graph extension not present" << std::endl;
        return 1;
    }
    if (0U == (graphProperties.graphFlags & ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_MUTABLE_GRAPH)) {
        std::cerr << "Device not supporting mutable graph" << std::endl;
        return 1;
    }
    MutableGraphApi api;
    if (false == loadMutableGraphApi(driverHandle, api)) {
        std::cerr << "Mutable graph functions not available" << std::endl;
        return 1;
    }

    ze_module_handle_t moduleTestKernels = nullptr;
    LevelZeroBlackBoxTests::createModuleFromSpirV(context, device0, LevelZeroBlackBoxTests::openCLKernelsSource, moduleTestKernels);
    MutableGraphKernels kernels;
    LevelZeroBlackBoxTests::createKernelWithName(moduleTestKernels, "add_constant_output", kernels.add);
    LevelZeroBlackBoxTests::createKernelWithName(moduleTestKernels, "mul_constant_output", kernels.mul);

    bool boxPass = true;
    auto runCase = [&](const char *testTitle, auto testFunction, UpdateDescriptorKind descriptorKind, MutationMode mutationMode, uint32_t testNumber, uint32_t subTestNumber) {
        std::ostringstream caseName;
        caseName << testTitle << " - " << getUpdateDescriptorKindName(descriptorKind) << " - " << getMutationModeName(mutationMode);
        const std::string currentTest = caseName.str();
        const std::string dumpName = "zello_mutable_graph_test_" + std::to_string(testNumber) + "_subtest_" + std::to_string(subTestNumber);

        LevelZeroBlackBoxTests::printTestHeader(currentTest);
        const bool casePass = testFunction(api, context, device0, kernels, descriptorKind, mutationMode, aubMode, dumpSettings, dumpName);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
        LevelZeroBlackBoxTests::addFailedCase(casePass, currentTest, testNumber, subTestNumber);
    };

    if (testMask.test(bitNumberTestMutateKernelArguments)) {
        const char *testTitle = "Mutate kernel arguments";
        if (0U == (mutableGraphProperties.flags & ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT)) {
            std::cout << testTitle << ": mutable graph node kernel arguments not supported, skipping test" << std::endl;
        } else {
            if (testSubMask.test(0)) {
                runCase(testTitle, testMutateSingleElementKernelArguments, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateKernelArguments, 0);
            }
            if (testSubMask.test(1)) {
                runCase(testTitle, testMutateSingleElementKernelArguments, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateKernelArguments, 1);
            }
            if (testSubMask.test(2)) {
                runCase(testTitle, testMutateSingleElementKernelArguments, UpdateDescriptorKind::fineGrain, MutationMode::mutateExecutableGraph, bitNumberTestMutateKernelArguments, 2);
            }
            if (testSubMask.test(3)) {
                runCase(testTitle, testMutateSingleElementKernelArguments, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateKernelArguments, 3);
            }
            if (testSubMask.test(4)) {
                runCase(testTitle, testMutateSingleElementKernelArguments, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateKernelArguments, 4);
            }
            if (testSubMask.test(5)) {
                runCase(testTitle, testMutateSingleElementKernelArguments, UpdateDescriptorKind::aggregate, MutationMode::mutateExecutableGraph, bitNumberTestMutateKernelArguments, 5);
            }
        }
    }

    if (testMask.test(bitNumberTestMutateGroupCount)) {
        const char *testTitle = "Mutate group count";
        if (0U == (mutableGraphProperties.flags & ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT)) {
            std::cout << testTitle << ": mutable graph node group count not supported, skipping test" << std::endl;
        } else {
            if (testSubMask.test(0)) {
                runCase(testTitle, testMutateSingleElementGroupCount, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateGroupCount, 0);
            }
            if (testSubMask.test(1)) {
                runCase(testTitle, testMutateSingleElementGroupCount, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateGroupCount, 1);
            }
            if (testSubMask.test(2)) {
                runCase(testTitle, testMutateSingleElementGroupCount, UpdateDescriptorKind::fineGrain, MutationMode::mutateExecutableGraph, bitNumberTestMutateGroupCount, 2);
            }
            if (testSubMask.test(3)) {
                runCase(testTitle, testMutateSingleElementGroupCount, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateGroupCount, 3);
            }
            if (testSubMask.test(4)) {
                runCase(testTitle, testMutateSingleElementGroupCount, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateGroupCount, 4);
            }
            if (testSubMask.test(5)) {
                runCase(testTitle, testMutateSingleElementGroupCount, UpdateDescriptorKind::aggregate, MutationMode::mutateExecutableGraph, bitNumberTestMutateGroupCount, 5);
            }
        }
    }

    if (testMask.test(bitNumberTestMutateGroupSize)) {
        const char *testTitle = "Mutate group size";
        if (0U == (mutableGraphProperties.flags & ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE)) {
            std::cout << testTitle << ": mutable graph node group size not supported, skipping test" << std::endl;
        } else {
            if (testSubMask.test(0)) {
                runCase(testTitle, testMutateSingleElementGroupSize, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateGroupSize, 0);
            }
            if (testSubMask.test(1)) {
                runCase(testTitle, testMutateSingleElementGroupSize, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateGroupSize, 1);
            }
            if (testSubMask.test(2)) {
                runCase(testTitle, testMutateSingleElementGroupSize, UpdateDescriptorKind::fineGrain, MutationMode::mutateExecutableGraph, bitNumberTestMutateGroupSize, 2);
            }
            if (testSubMask.test(3)) {
                runCase(testTitle, testMutateSingleElementGroupSize, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateGroupSize, 3);
            }
            if (testSubMask.test(4)) {
                runCase(testTitle, testMutateSingleElementGroupSize, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateGroupSize, 4);
            }
            if (testSubMask.test(5)) {
                runCase(testTitle, testMutateSingleElementGroupSize, UpdateDescriptorKind::aggregate, MutationMode::mutateExecutableGraph, bitNumberTestMutateGroupSize, 5);
            }
        }
    }

    if (testMask.test(bitNumberTestMutateKernelArgumentsAndDispatchShapes)) {
        const char *testTitle = "Mutate kernel arguments, group count and group size";
        if (kernelArgumentsAndDispatchShapesFlags != (mutableGraphProperties.flags & kernelArgumentsAndDispatchShapesFlags)) {
            std::cout << testTitle << ": mutable graph node kernel arguments, group count or group size not supported, skipping test" << std::endl;
        } else {
            if (testSubMask.test(0)) {
                runCase(testTitle, testMutateKernelArgumentsAndDispatchShapes, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateKernelArgumentsAndDispatchShapes, 0);
            }
            if (testSubMask.test(1)) {
                runCase(testTitle, testMutateKernelArgumentsAndDispatchShapes, UpdateDescriptorKind::fineGrain, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateKernelArgumentsAndDispatchShapes, 1);
            }
            if (testSubMask.test(2)) {
                runCase(testTitle, testMutateKernelArgumentsAndDispatchShapes, UpdateDescriptorKind::fineGrain, MutationMode::mutateExecutableGraph, bitNumberTestMutateKernelArgumentsAndDispatchShapes, 2);
            }
            if (testSubMask.test(3)) {
                runCase(testTitle, testMutateKernelArgumentsAndDispatchShapes, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphInstantiateNewExecutableGraph, bitNumberTestMutateKernelArgumentsAndDispatchShapes, 3);
            }
            if (testSubMask.test(4)) {
                runCase(testTitle, testMutateKernelArgumentsAndDispatchShapes, UpdateDescriptorKind::aggregate, MutationMode::mutateGraphUpdateExecutableGraph, bitNumberTestMutateKernelArgumentsAndDispatchShapes, 4);
            }
            if (testSubMask.test(5)) {
                runCase(testTitle, testMutateKernelArgumentsAndDispatchShapes, UpdateDescriptorKind::aggregate, MutationMode::mutateExecutableGraph, bitNumberTestMutateKernelArgumentsAndDispatchShapes, 5);
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernels.add));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernels.mul));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(moduleTestKernels));

    int mainRetCode = LevelZeroBlackBoxTests::printTestResult(boxPass, aubMode, blackBoxName);
    LevelZeroBlackBoxTests::printFailedCases();
    return mainRetCode;
}
