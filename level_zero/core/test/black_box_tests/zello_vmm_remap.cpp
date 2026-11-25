/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstdio>
#include <iostream>
#include <limits>
#include <vector>

enum class SubmissionPath {
    regularCommandListOnQueue,
    regularCommandListImmediateAppend,
    graph,
    mutableCommandList,
};

struct VirtualMemorySegments {
    ze_context_handle_t context = nullptr;
    ze_device_handle_t device = nullptr;
    size_t segmentSize = 0u;
    std::vector<void *> virtualAddresses;
    std::vector<ze_physical_mem_handle_t> mappedPhysicalMemory;
    std::vector<ze_physical_mem_handle_t> retiredPhysicalMemory;

    void map(size_t index) {
        ze_physical_mem_desc_t physicalDesc = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC};
        physicalDesc.flags = ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_DEVICE;
        physicalDesc.size = segmentSize;
        SUCCESS_OR_TERMINATE(zePhysicalMemCreate(context, device, &physicalDesc, &mappedPhysicalMemory[index]));
        SUCCESS_OR_TERMINATE(zeVirtualMemMap(context, virtualAddresses[index], segmentSize, mappedPhysicalMemory[index], 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE));
    }

    void create(size_t count) {
        virtualAddresses.resize(count, nullptr);
        mappedPhysicalMemory.resize(count, nullptr);
        for (size_t i = 0; i < count; i++) {
            SUCCESS_OR_TERMINATE(zeVirtualMemReserve(context, nullptr, segmentSize, &virtualAddresses[i]));
            map(i);
        }
    }

    void remapToNewPhysicalMemory() {
        for (size_t i = 0; i < virtualAddresses.size(); i++) {
            SUCCESS_OR_TERMINATE(zeVirtualMemUnmap(context, virtualAddresses[i], segmentSize));
            retiredPhysicalMemory.push_back(mappedPhysicalMemory[i]);
            map(i);
        }
    }

    void destroyRetiredPhysicalMemory() {
        for (auto physicalMemory : retiredPhysicalMemory) {
            SUCCESS_OR_TERMINATE(zePhysicalMemDestroy(context, physicalMemory));
        }
        retiredPhysicalMemory.clear();
    }

    void release() {
        for (size_t i = 0; i < virtualAddresses.size(); i++) {
            SUCCESS_OR_TERMINATE(zeVirtualMemUnmap(context, virtualAddresses[i], segmentSize));
            SUCCESS_OR_TERMINATE(zePhysicalMemDestroy(context, mappedPhysicalMemory[i]));
            SUCCESS_OR_TERMINATE(zeVirtualMemFree(context, virtualAddresses[i], segmentSize));
        }
        destroyRetiredPhysicalMemory();
    }
};

std::vector<std::vector<uint8_t>> scribbleOverFreedHeapMemory() {
    std::vector<std::vector<uint8_t>> chunks;
    for (size_t size = 16u; size <= 4096u; size += 16u) {
        for (int i = 0; i < 64; i++) {
            chunks.emplace_back(size, uint8_t{0xa5});
        }
    }
    return chunks;
}

bool isMutableKernelArgumentsSupported(ze_driver_handle_t driverHandle, ze_device_handle_t device) {
    std::vector<ze_driver_extension_properties_t> extensions(1);
    std::snprintf(extensions[0].name, sizeof(extensions[0].name), "%s", ZE_MUTABLE_COMMAND_LIST_EXT_NAME);
    extensions[0].version = ZE_MUTABLE_COMMAND_LIST_EXT_VERSION_1_0;
    if (!LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensions)) {
        return false;
    }
    ze_mutable_command_list_ext_properties_t mclProperties = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXT_PROPERTIES};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.pNext = &mclProperties;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    return !!(mclProperties.mutableCommandFlags & ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS);
}

bool isGraphSupported(ze_driver_handle_t driverHandle, ze_device_handle_t device) {
    std::vector<ze_driver_extension_properties_t> extensions(1);
    std::snprintf(extensions[0].name, sizeof(extensions[0].name), "%s", ZE_RECORD_REPLAY_GRAPH_EXT_NAME);
    extensions[0].version = ZE_RECORD_REPLAY_GRAPH_EXT_VERSION_1_0;
    if (!LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensions)) {
        return false;
    }
    ze_record_replay_graph_ext_properties_t graphProperties = {ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXT_PROPERTIES};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.pNext = &graphProperties;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    return !!(graphProperties.graphFlags & ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_IMMUTABLE_GRAPH);
}

bool testReexecutionAfterVirtualMemoryRemap(ze_context_handle_t context, ze_device_handle_t device, ze_kernel_handle_t copyKernel,
                                            SubmissionPath submissionPath, bool aubMode) {
    constexpr size_t segmentCount = 2u;
    constexpr size_t copySize = 4096u;
    constexpr uint64_t timeout = std::numeric_limits<uint64_t>::max();

    size_t segmentSize = 0u;
    SUCCESS_OR_TERMINATE(zeVirtualMemQueryPageSize(context, device, copySize, &segmentSize));
    VirtualMemorySegments segments{context, device, segmentSize};
    segments.create(segmentCount);

    std::vector<void *> outputs(segmentCount, nullptr);
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    for (auto &output : outputs) {
        SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, copySize, 1, &output));
    }

    ze_command_list_handle_t immediateCmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, immediateCmdList);

    auto fillSegments = [&](uint8_t value) {
        for (auto virtualAddress : segments.virtualAddresses) {
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(immediateCmdList, virtualAddress, &value, sizeof(value), copySize, nullptr, 0, nullptr));
        }
        SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(immediateCmdList, timeout));
    };

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, static_cast<uint32_t>(copySize), 1u, 1u, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));
    const ze_group_count_t groupCount = {static_cast<uint32_t>(copySize / groupSizeX), 1u, 1u};

    std::vector<uint64_t> commandIds(segmentCount, 0u);
    auto appendCopies = [&](ze_command_list_handle_t cmdList) {
        for (size_t i = 0; i < segmentCount; i++) {
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &outputs[i]));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &segments.virtualAddresses[i]));
            if (submissionPath == SubmissionPath::mutableCommandList) {
                ze_mutable_command_id_ext_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXT_DESC};
                commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXT_FLAG_KERNEL_ARGUMENTS;
                SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExt(cmdList, &commandIdDesc, &commandIds[i]));
            }
            SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &groupCount, nullptr, 0, nullptr));
        }
    };

    ze_command_queue_handle_t queue = nullptr;
    ze_command_list_handle_t recordedCmdList = nullptr;
    ze_graph_handle_t graph = nullptr;
    ze_executable_graph_handle_t executableGraph = nullptr;

    if (submissionPath == SubmissionPath::graph) {
        SUCCESS_OR_TERMINATE(zeGraphCreateExt(context, nullptr, &graph));
        SUCCESS_OR_TERMINATE(zeCommandListBeginCaptureIntoGraphExt(immediateCmdList, graph, nullptr));
        appendCopies(immediateCmdList);
        SUCCESS_OR_TERMINATE(zeCommandListEndGraphCaptureExt(immediateCmdList, nullptr, nullptr));
        SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(graph, nullptr, &executableGraph));
    } else {
        uint32_t ordinal = 0u;
        queue = LevelZeroBlackBoxTests::createCommandQueue(context, device, &ordinal, false);
        ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
        ze_mutable_command_list_ext_desc_t mutableDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXT_DESC};
        if (submissionPath == SubmissionPath::mutableCommandList) {
            cmdListDesc.pNext = &mutableDesc;
        }
        cmdListDesc.commandQueueGroupOrdinal = ordinal;
        SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &recordedCmdList));
        appendCopies(recordedCmdList);
        SUCCESS_OR_TERMINATE(zeCommandListClose(recordedCmdList));
    }

    bool validRet = true;
    auto executeAndValidate = [&](uint8_t expectedValue, const char *stage) {
        switch (submissionPath) {
        case SubmissionPath::regularCommandListOnQueue:
        case SubmissionPath::mutableCommandList:
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(queue, 1, &recordedCmdList, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(queue, timeout));
            break;
        case SubmissionPath::regularCommandListImmediateAppend:
            SUCCESS_OR_TERMINATE(zeCommandListImmediateAppendCommandListsWithParameters(immediateCmdList, 1, &recordedCmdList, nullptr, nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(immediateCmdList, timeout));
            break;
        case SubmissionPath::graph:
            SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(immediateCmdList, executableGraph, nullptr, nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(immediateCmdList, timeout));
            break;
        }
        if (aubMode) {
            return;
        }
        for (size_t i = 0; i < segmentCount; i++) {
            if (!LevelZeroBlackBoxTests::validateToValue<uint8_t>(expectedValue, outputs[i], copySize)) {
                std::cerr << stage << ": segment " << i << " has unexpected content" << std::endl;
                validRet = false;
            }
        }
    };

    fillSegments(1u);
    executeAndValidate(1u, "before remap");

    if (submissionPath == SubmissionPath::mutableCommandList) {
        for (size_t i = 0; i < segmentCount; i++) {
            ze_mutable_kernel_argument_ext_desc_t mutateSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXT_DESC};
            mutateSrcArg.commandId = commandIds[i];
            mutateSrcArg.argIndex = 1;
            mutateSrcArg.argSize = sizeof(void *);
            mutateSrcArg.pArgValue = &segments.virtualAddresses[(i + 1) % segmentCount];
            ze_mutable_commands_ext_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXT_DESC};
            mutateDesc.pNext = &mutateSrcArg;
            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExt(recordedCmdList, &mutateDesc));
        }
        SUCCESS_OR_TERMINATE(zeCommandListClose(recordedCmdList));
        executeAndValidate(1u, "after kernel argument mutation");
    }

    segments.remapToNewPhysicalMemory();
    fillSegments(2u);
    executeAndValidate(2u, "after remap");

    segments.destroyRetiredPhysicalMemory();
    // the driver must not touch the destroyed physical memory; overwriting freed heap makes a stale reference fault
    auto scribbledHeapMemory = scribbleOverFreedHeapMemory();
    fillSegments(3u);
    executeAndValidate(3u, "after destroying old physical memory");

    if (executableGraph) {
        SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(executableGraph));
        SUCCESS_OR_TERMINATE(zeGraphDestroyExt(graph));
    }
    if (recordedCmdList) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(recordedCmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(queue));
    }
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(immediateCmdList));
    for (auto output : outputs) {
        SUCCESS_OR_TERMINATE(zeMemFree(context, output));
    }
    segments.release();
    return validRet;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestRegularCommandListOnQueue = 0u;
    constexpr uint32_t bitNumberTestRegularCommandListImmediateAppend = 1u;
    constexpr uint32_t bitNumberTestGraph = 2u;
    constexpr uint32_t bitNumberTestMutableCommandList = 3u;

    const std::string blackBoxName("Zello VMM Remap");
    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, std::numeric_limits<uint32_t>::max());
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t copyKernel = nullptr;
    LevelZeroBlackBoxTests::createModuleFromSpirV(context, device, LevelZeroBlackBoxTests::openCLKernelsSource, module);
    LevelZeroBlackBoxTests::createKernelWithName(module, "memcpy_bytes", copyKernel);

    struct TestCase {
        uint32_t bitNumber;
        SubmissionPath submissionPath;
        const char *name;
        bool supported;
    };
    const TestCase testCases[] = {
        {bitNumberTestRegularCommandListOnQueue, SubmissionPath::regularCommandListOnQueue, "Regular command list on queue", true},
        {bitNumberTestRegularCommandListImmediateAppend, SubmissionPath::regularCommandListImmediateAppend, "Regular command list appended to immediate command list", true},
        {bitNumberTestGraph, SubmissionPath::graph, "Graph", isGraphSupported(driverHandle, device)},
        {bitNumberTestMutableCommandList, SubmissionPath::mutableCommandList, "Mutable command list with kernel argument mutation", isMutableKernelArgumentsSupported(driverHandle, device)},
    };

    bool boxPass = true;
    for (const auto &testCase : testCases) {
        if (!testMask.test(testCase.bitNumber)) {
            continue;
        }
        LevelZeroBlackBoxTests::printTestHeader(testCase.name);
        if (!testCase.supported) {
            std::cout << "Skipped. " << testCase.name << " not supported" << std::endl;
            continue;
        }
        bool casePass = testReexecutionAfterVirtualMemoryRemap(context, device, copyKernel, testCase.submissionPath, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, testCase.name);
        boxPass &= casePass;
        LevelZeroBlackBoxTests::addFailedCase(casePass, testCase.name, testCase.bitNumber, LevelZeroBlackBoxTests::undefinedTestNumber);
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    int mainRetCode = LevelZeroBlackBoxTests::printTestResult(boxPass, aubMode, blackBoxName);
    LevelZeroBlackBoxTests::printFailedCases();
    return mainRetCode;
}
