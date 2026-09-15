/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Measures the cold vs steady-state graph submit cost, exercising defer backing on the first submit.
const char *gemmKernelSrc = R"CLC(
__kernel void gemm(__global const float *a, __global const float *b, __global float *c, int n) {
    int col = get_global_id(0);
    int row = get_global_id(1);
    float acc = 0.0f;
    for (int k = 0; k < n; k++) {
        acc += a[row * n + k] * b[k * n + col];
    }
    c[row * n + col] = acc;
}
)CLC";

uint32_t chooseGroupSize(uint32_t matrixDimension) {
    for (uint32_t groupSize = 16; groupSize > 1; groupSize >>= 1) {
        if (matrixDimension % groupSize == 0) {
            return groupSize;
        }
    }
    return 1;
}

double toMicroseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000.0;
}

double vectorMin(const std::vector<double> &v) { return *std::min_element(v.begin(), v.end()); }
double vectorMax(const std::vector<double> &v) { return *std::max_element(v.begin(), v.end()); }

double vectorMedian(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    if (n == 0) {
        return 0.0;
    }
    if (n % 2 == 1) {
        return v[n / 2];
    }
    return (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

struct GraphResources {
    ze_command_list_handle_t cmdList = nullptr;
    ze_graph_handle_t virtualGraph = nullptr;
    ze_executable_graph_handle_t physicalGraph = nullptr;
    std::vector<void *> a;
    std::vector<void *> b;
    std::vector<void *> c;
};

void allocateNodeBuffers(ze_context_handle_t context, ze_device_handle_t device, uint32_t nodeCount, size_t bufBytes, GraphResources &res) {
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    res.a.resize(nodeCount, nullptr);
    res.b.resize(nodeCount, nullptr);
    res.c.resize(nodeCount, nullptr);
    for (uint32_t i = 0; i < nodeCount; i++) {
        SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, bufBytes, 4096, device, &res.a[i]));
        SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, bufBytes, 4096, device, &res.b[i]));
        SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, bufBytes, 4096, device, &res.c[i]));
    }
}

void captureGemmGraph(ze_context_handle_t context, ze_device_handle_t device, std::vector<ze_kernel_handle_t> &kernels,
                      uint32_t matrixSize, uint32_t groupSize, GraphResources &res) {
    const int matrixDim = static_cast<int>(matrixSize);
    const ze_group_count_t groupCount = {matrixSize / groupSize, matrixSize / groupSize, 1};

    for (uint32_t i = 0; i < kernels.size(); i++) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernels[i], groupSize, groupSize, 1));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels[i], 0, sizeof(void *), &res.a[i]));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels[i], 1, sizeof(void *), &res.b[i]));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels[i], 2, sizeof(void *), &res.c[i]));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernels[i], 3, sizeof(int), &matrixDim));
    }

    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, 0u, false, res.cmdList);

    SUCCESS_OR_TERMINATE(zeGraphCreateExt(context, nullptr, &res.virtualGraph));
    SUCCESS_OR_TERMINATE(zeCommandListBeginCaptureIntoGraphExt(res.cmdList, res.virtualGraph, nullptr));
    for (uint32_t i = 0; i < kernels.size(); i++) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(res.cmdList, kernels[i], &groupCount, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListEndGraphCaptureExt(res.cmdList, nullptr, nullptr));
    SUCCESS_OR_TERMINATE(zeGraphInstantiateExt(res.virtualGraph, nullptr, &res.physicalGraph));
}

void destroyGraph(ze_context_handle_t context, GraphResources &res) {
    SUCCESS_OR_TERMINATE(zeExecutableGraphDestroyExt(res.physicalGraph));
    SUCCESS_OR_TERMINATE(zeGraphDestroyExt(res.virtualGraph));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(res.cmdList));
    for (void *ptr : res.a) {
        SUCCESS_OR_TERMINATE(zeMemFree(context, ptr));
    }
    for (void *ptr : res.b) {
        SUCCESS_OR_TERMINATE(zeMemFree(context, ptr));
    }
    for (void *ptr : res.c) {
        SUCCESS_OR_TERMINATE(zeMemFree(context, ptr));
    }
}

void measurePerf(ze_context_handle_t context, ze_device_handle_t device, std::vector<ze_kernel_handle_t> &kernels,
                 uint32_t matrixSize, uint32_t nodeCount, uint32_t iterations, uint32_t warmup, uint32_t repeat) {
    const size_t bufBytes = static_cast<size_t>(matrixSize) * matrixSize * sizeof(float);
    const uint32_t groupSize = chooseGroupSize(matrixSize);
    const uint32_t steadyStart = std::max(warmup, 1u);

    std::vector<double> coldSubmit;
    std::vector<double> coldTotal;
    std::vector<double> steadySubmitPooled;
    std::vector<double> steadyTotalPooled;
    coldSubmit.reserve(repeat);
    coldTotal.reserve(repeat);

    std::cout << "GEMM graph submit: matrix=" << matrixSize << "x" << matrixSize
              << " nodes=" << nodeCount << " groupSize=" << groupSize
              << " iterations=" << iterations << " warmup=" << warmup << " repeat=" << repeat << std::endl;

    for (uint32_t r = 0; r < repeat; r++) {
        GraphResources res;
        allocateNodeBuffers(context, device, nodeCount, bufBytes, res);
        captureGemmGraph(context, device, kernels, matrixSize, groupSize, res);

        std::vector<double> submitTimings(iterations, 0.0);
        std::vector<double> totalTimings(iterations, 0.0);
        for (uint32_t it = 0; it < iterations; it++) {
            auto start = std::chrono::steady_clock::now();
            SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(res.cmdList, res.physicalGraph, nullptr, nullptr, 0, nullptr));
            auto submitEnd = std::chrono::steady_clock::now();
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(res.cmdList, std::numeric_limits<uint64_t>::max()));
            auto syncEnd = std::chrono::steady_clock::now();
            submitTimings[it] = toMicroseconds(start, submitEnd);
            totalTimings[it] = toMicroseconds(start, syncEnd);
        }

        coldSubmit.push_back(submitTimings[0]);
        coldTotal.push_back(totalTimings[0]);
        std::vector<double> steadySubmit(submitTimings.begin() + std::min<size_t>(steadyStart, submitTimings.size()), submitTimings.end());
        std::vector<double> steadyTotal(totalTimings.begin() + std::min<size_t>(steadyStart, totalTimings.size()), totalTimings.end());
        for (double x : steadySubmit) {
            steadySubmitPooled.push_back(x);
        }
        for (double x : steadyTotal) {
            steadyTotalPooled.push_back(x);
        }

        std::cout << "  repeat " << r
                  << ": cold-submit=" << submitTimings[0]
                  << " cold-total=" << totalTimings[0]
                  << " steady-submit-median=" << (steadySubmit.empty() ? 0.0 : vectorMedian(steadySubmit))
                  << " steady-total-median=" << (steadyTotal.empty() ? 0.0 : vectorMedian(steadyTotal)) << " us" << std::endl;

        destroyGraph(context, res);
    }

    const double steadySubmitMedian = steadySubmitPooled.empty() ? 0.0 : vectorMedian(steadySubmitPooled);
    const double steadyTotalMedian = steadyTotalPooled.empty() ? 0.0 : vectorMedian(steadyTotalPooled);

    std::cout << "==== Summary (all times in us) ====" << std::endl;
    std::cout << "cold submit-call  : min=" << vectorMin(coldSubmit) << " median=" << vectorMedian(coldSubmit) << " max=" << vectorMax(coldSubmit) << std::endl;
    std::cout << "cold total        : min=" << vectorMin(coldTotal) << " median=" << vectorMedian(coldTotal) << " max=" << vectorMax(coldTotal) << std::endl;
    std::cout << "steady submit-call: median=" << steadySubmitMedian << std::endl;
    std::cout << "steady total      : median=" << steadyTotalMedian << std::endl;
    std::cout << "gpu-exec portion  : steady_total - steady_submit = " << (steadyTotalMedian - steadySubmitMedian) << std::endl;
}

bool verifyCorrectness(ze_context_handle_t context, ze_device_handle_t device, std::vector<ze_kernel_handle_t> &kernels,
                       uint32_t matrixSize, uint32_t nodeCount) {
    const size_t elemCount = static_cast<size_t>(matrixSize) * matrixSize;
    const size_t bufBytes = elemCount * sizeof(float);
    const uint32_t groupSize = chooseGroupSize(matrixSize);

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *hostA = nullptr;
    void *hostC = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, bufBytes, 4096, &hostA));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, bufBytes, 4096, &hostC));
    for (size_t i = 0; i < elemCount; i++) {
        static_cast<float *>(hostA)[i] = 1.0f;
    }

    GraphResources res;
    allocateNodeBuffers(context, device, nodeCount, bufBytes, res);

    ze_command_list_handle_t uploadList;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, 0u, true, uploadList);
    for (uint32_t i = 0; i < nodeCount; i++) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(uploadList, res.a[i], hostA, bufBytes, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(uploadList, res.b[i], hostA, bufBytes, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(uploadList, std::numeric_limits<uint64_t>::max()));

    captureGemmGraph(context, device, kernels, matrixSize, groupSize, res);
    SUCCESS_OR_TERMINATE(zeCommandListAppendGraphExt(res.cmdList, res.physicalGraph, nullptr, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(res.cmdList, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    const float expected = static_cast<float>(matrixSize);
    for (uint32_t i = 0; i < nodeCount; i++) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(uploadList, hostC, res.c[i], bufBytes, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(uploadList, std::numeric_limits<uint64_t>::max()));
        valid &= LevelZeroBlackBoxTests::validateToValue<float>(expected, hostC, elemCount);
    }

    destroyGraph(context, res);
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(uploadList));
    SUCCESS_OR_TERMINATE(zeMemFree(context, hostA));
    SUCCESS_OR_TERMINATE(zeMemFree(context, hostC));
    return valid;
}

} // namespace

int main(int argc, char *argv[]) {
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    const bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    const bool check = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-c", "--check");

    const uint32_t matrixSize = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-m", "--matrix-size", 128u);
    const uint32_t nodeCount = std::max(1u, LevelZeroBlackBoxTests::getParamValue(argc, argv, "-n", "--nodes", 10u));
    const uint32_t iterations = std::max(1u, LevelZeroBlackBoxTests::getParamValue(argc, argv, "-i", "--iterations", 100u));
    const uint32_t warmup = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-w", "--warmup", 0u);
    const uint32_t repeat = std::max(1u, LevelZeroBlackBoxTests::getParamValue(argc, argv, "-r", "--repeat", 20u));

    const std::string blackBoxName("Zello Graph GEMM Submit");

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device0 = devices[0];

    ze_record_replay_graph_ext_properties_t graphProperties = {ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXT_PROPERTIES};
    ze_device_properties_t device0Properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device0Properties.pNext = &graphProperties;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &device0Properties));
    LevelZeroBlackBoxTests::printDeviceProperties(device0Properties);

    std::vector<ze_driver_extension_properties_t> extensionVector;
    ze_driver_extension_properties_t graphExtension{};
    std::snprintf(graphExtension.name, sizeof(graphExtension.name), "%s", ZE_RECORD_REPLAY_GRAPH_EXT_NAME);
    graphExtension.version = ZE_RECORD_REPLAY_GRAPH_EXT_VERSION_1_0;
    extensionVector.push_back(graphExtension);
    if (!LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionVector)) {
        std::cerr << "Graph extension not present" << std::endl;
        return 1;
    }
    if (0 == (graphProperties.graphFlags & ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_IMMUTABLE_GRAPH)) {
        std::cerr << "Device not supporting graph" << std::endl;
        return 1;
    }

    ze_module_handle_t module;
    LevelZeroBlackBoxTests::createModuleFromSpirV(context, device0, gemmKernelSrc, module);
    std::vector<ze_kernel_handle_t> kernels(nodeCount, nullptr);
    for (uint32_t i = 0; i < nodeCount; i++) {
        LevelZeroBlackBoxTests::createKernelWithName(module, "gemm", kernels[i]);
    }

    bool boxPass = true;
    if (check) {
        const bool casePass = verifyCorrectness(context, device0, kernels, matrixSize, nodeCount);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, "GEMM correctness");
        boxPass &= casePass;
    }

    if (!aubMode) {
        measurePerf(context, device0, kernels, matrixSize, nodeCount, iterations, warmup, repeat);
    }

    for (auto kernel : kernels) {
        SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    }
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    LevelZeroBlackBoxTests::printResult(aubMode, boxPass, blackBoxName);
    return boxPass ? 0 : 1;
}
