/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/include/level_zero/ze_stypes.h"

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct VisitUserData {
    ze_command_list_handle_t targetCmdList = nullptr;
    // Profiling support for concrete visitor callbacks
    std::vector<ze_event_handle_t> profilingEvents;
    std::vector<std::string> apiNames;
    uint32_t memoryCopyCount = 0;
    uint32_t barrierCount = 0;
    uint32_t launchKernelCount = 0;
    uint32_t eventIndex = 0;
    bool enableProfiling = false;
};

struct VisitProfilingData {
    std::vector<ze_event_handle_t> events;
    std::vector<std::string> apiNames;
    uint32_t nextEventIndex = 0;
    bool overflow = false;
};

static ze_result_t VISITOR_CCONV appendMemoryCopyVisitor(ze_command_list_handle_t,
                                                         void *dstptr,
                                                         const void *srcptr,
                                                         size_t size,
                                                         ze_event_handle_t hSignalEvent,
                                                         uint32_t numWaitEvents,
                                                         ze_event_handle_t *phWaitEvents,
                                                         void *userData) {
    auto *visitUserData = static_cast<VisitUserData *>(userData);
    ++visitUserData->memoryCopyCount;

    // Inject profiling event if enabled
    ze_event_handle_t effectiveSignalEvent = hSignalEvent;
    if (visitUserData->enableProfiling && visitUserData->eventIndex < visitUserData->profilingEvents.size()) {
        effectiveSignalEvent = visitUserData->profilingEvents[visitUserData->eventIndex];
        visitUserData->apiNames.emplace_back("zeCommandListAppendMemoryCopy");
        ++visitUserData->eventIndex;
    }

    return zeCommandListAppendMemoryCopy(visitUserData->targetCmdList, dstptr, srcptr, size, effectiveSignalEvent, numWaitEvents, phWaitEvents);
}

static ze_result_t VISITOR_CCONV appendBarrierVisitor(ze_command_list_handle_t,
                                                      ze_event_handle_t hSignalEvent,
                                                      uint32_t numWaitEvents,
                                                      ze_event_handle_t *phWaitEvents,
                                                      void *userData) {
    auto *visitUserData = static_cast<VisitUserData *>(userData);
    ++visitUserData->barrierCount;

    // Inject profiling event if enabled
    ze_event_handle_t effectiveSignalEvent = hSignalEvent;
    if (visitUserData->enableProfiling && visitUserData->eventIndex < visitUserData->profilingEvents.size()) {
        effectiveSignalEvent = visitUserData->profilingEvents[visitUserData->eventIndex];
        visitUserData->apiNames.emplace_back("zeCommandListAppendBarrier");
        ++visitUserData->eventIndex;
    }

    return zeCommandListAppendBarrier(visitUserData->targetCmdList, effectiveSignalEvent, numWaitEvents, phWaitEvents);
}

static ze_result_t VISITOR_CCONV appendLaunchKernelVisitor(ze_command_list_handle_t,
                                                           ze_kernel_handle_t hKernel,
                                                           const ze_group_count_t *pLaunchFuncArgs,
                                                           ze_event_handle_t hSignalEvent,
                                                           uint32_t numWaitEvents,
                                                           ze_event_handle_t *phWaitEvents,
                                                           void *userData) {
    auto *visitUserData = static_cast<VisitUserData *>(userData);
    ++visitUserData->launchKernelCount;

    // Inject profiling event if enabled
    ze_event_handle_t effectiveSignalEvent = hSignalEvent;
    if (visitUserData->enableProfiling && visitUserData->eventIndex < visitUserData->profilingEvents.size()) {
        effectiveSignalEvent = visitUserData->profilingEvents[visitUserData->eventIndex];
        visitUserData->apiNames.emplace_back("zeCommandListAppendLaunchKernel");
        ++visitUserData->eventIndex;
    }

    return zeCommandListAppendLaunchKernel(visitUserData->targetCmdList, hKernel, pLaunchFuncArgs, effectiveSignalEvent, numWaitEvents, phWaitEvents);
}

static void VISITOR_CCONV beforeDefaultOpProfiling(const char *,
                                                   void *userData,
                                                   uint32_t *,
                                                   ze_event_handle_t **,
                                                   ze_event_handle_t *hSignalEvent) {
    auto *profilingData = static_cast<VisitProfilingData *>(userData);
    if (profilingData->nextEventIndex >= profilingData->events.size()) {
        profilingData->overflow = true;
        return;
    }

    *hSignalEvent = profilingData->events[profilingData->nextEventIndex];
}

static void VISITOR_CCONV afterDefaultOpProfiling(const char *fname,
                                                  void *userData,
                                                  uint32_t *,
                                                  ze_event_handle_t **,
                                                  ze_event_handle_t *) {
    auto *profilingData = static_cast<VisitProfilingData *>(userData);
    if (profilingData->nextEventIndex >= profilingData->events.size()) {
        profilingData->overflow = true;
        return;
    }

    profilingData->apiNames.emplace_back(fname ? fname : "unknown");
    ++profilingData->nextEventIndex;
}

bool testVisitReappend(LevelZeroBlackBoxTests::VisitExtension::VisitApi &visitApi,
                       ze_context_handle_t context,
                       ze_device_handle_t device,
                       ze_module_handle_t module,
                       bool useConcreteVisitors,
                       bool useProfilingCallbacks,
                       bool aubMode) {
    bool validRet = true;

    // ---------- allocations ----------
    constexpr size_t allocSize = 4096;

    ze_host_mem_alloc_desc_t hostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t devDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};

    void *srcBuffer = nullptr;    // host-visible, filled with pattern
    void *outputBuffer = nullptr; // host-visible, receives result
    void *interimBuffer = nullptr;
    void *dstDevBuffer = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &outputBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devDesc, allocSize, allocSize, device, &interimBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devDesc, allocSize, allocSize, device, &dstDevBuffer));

    // Fill source with a recognizable pattern
    for (size_t i = 0; i < allocSize; ++i) {
        static_cast<uint8_t *>(srcBuffer)[i] = static_cast<uint8_t>(i & 0xFF);
    }
    std::memset(outputBuffer, 0, allocSize);

    // ---------- kernel setup ----------
    ze_kernel_handle_t kernel = nullptr;
    LevelZeroBlackBoxTests::createKernelWithName(module, "memcpy_bytes", kernel);

    constexpr size_t numThreads = allocSize; // one byte per thread
    uint32_t groupSizeX = 32u, groupSizeY = 1u, groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, static_cast<uint32_t>(numThreads), 1u, 1u, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstDevBuffer), &dstDevBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(interimBuffer), &interimBuffer));

    ze_group_count_t groupCount{};
    groupCount.groupCountX = static_cast<uint32_t>(numThreads) / groupSizeX;
    groupCount.groupCountY = 1u;
    groupCount.groupCountZ = 1u;

    // ---------- build the non-immediate command list ----------
    ze_command_list_handle_t recordCmdList = nullptr;
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, recordCmdList, false, ZE_COMMAND_LIST_FLAG_ENABLE_CMD_VISITING));

    // host src -> device interim
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(recordCmdList, interimBuffer, srcBuffer, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(recordCmdList, nullptr, 0, nullptr));

    // memcpy_bytes kernel: interim -> dstDev
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(recordCmdList, kernel, &groupCount, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(recordCmdList, nullptr, 0, nullptr));

    // device dst -> host output
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(recordCmdList, outputBuffer, dstDevBuffer, allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(recordCmdList));

    // ---------- immediate command list that will receive the reappended commands ----------
    ze_command_list_handle_t immCmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device, immCmdList);

    ze_event_pool_handle_t profilingEventPool = nullptr;
    VisitProfilingData profilingData{};
    constexpr uint32_t profilingEventCount = 8;
    if (useProfilingCallbacks) {
        profilingData.events.resize(profilingEventCount, nullptr);
        LevelZeroBlackBoxTests::createEventPoolAndEvents(context,
                                                         device,
                                                         profilingEventPool,
                                                         ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
                                                         false,
                                                         nullptr,
                                                         profilingEventCount,
                                                         profilingData.events.data(),
                                                         ZE_EVENT_SCOPE_FLAG_HOST,
                                                         ZE_EVENT_SCOPE_FLAG_HOST);
    }

    // ---------- visit with REAPPEND ----------

    ze_visit_ext_desc_t visitDesc{ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC};
    VisitUserData visitUserData{};
    visitUserData.targetCmdList = immCmdList;

    // Set up profiling in concrete visitor callbacks if enabled
    if (useConcreteVisitors && useProfilingCallbacks) {
        visitUserData.enableProfiling = true;
        visitUserData.profilingEvents = profilingData.events;
    }

    ze_concrete_visitor_ext_desc_t launchKernelVisitorDesc{ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC};
    ze_concrete_visitor_ext_desc_t barrierVisitorDesc{ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC};
    ze_concrete_visitor_ext_desc_t memoryCopyVisitorDesc{ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC};

    if (useConcreteVisitors) {
        launchKernelVisitorDesc.pNext = nullptr;
        launchKernelVisitorDesc.fname = "zeCommandListAppendLaunchKernel";
        launchKernelVisitorDesc.callback = reinterpret_cast<void *>(appendLaunchKernelVisitor);

        barrierVisitorDesc.pNext = &launchKernelVisitorDesc;
        barrierVisitorDesc.fname = "zeCommandListAppendBarrier";
        barrierVisitorDesc.callback = reinterpret_cast<void *>(appendBarrierVisitor);

        memoryCopyVisitorDesc.pNext = &barrierVisitorDesc;
        memoryCopyVisitorDesc.fname = "zeCommandListAppendMemoryCopy";
        memoryCopyVisitorDesc.callback = reinterpret_cast<void *>(appendMemoryCopyVisitor);
    }

    visitDesc.pNext = useConcreteVisitors ? &memoryCopyVisitorDesc : nullptr;
    visitDesc.userData = useConcreteVisitors ? static_cast<void *>(&visitUserData) : (useProfilingCallbacks ? static_cast<void *>(&profilingData) : nullptr);
    visitDesc.hReappendTargetCmdList = useConcreteVisitors ? nullptr : immCmdList;
    visitDesc.beforeDefaultOpClb = (useProfilingCallbacks && !useConcreteVisitors) ? beforeDefaultOpProfiling : nullptr;
    visitDesc.defaultOp = useConcreteVisitors ? ZE_VISIT_EXT_DEFAULT_OP_IGNORE : ZE_VISIT_EXT_DEFAULT_OP_REAPPEND;
    visitDesc.afterDefaultOpClb = (useProfilingCallbacks && !useConcreteVisitors) ? afterDefaultOpProfiling : nullptr;

    SUCCESS_OR_TERMINATE(visitApi.commandListVisit(recordCmdList, &visitDesc));

    // ---------- synchronize and validate ----------
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(immCmdList, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        validRet = LevelZeroBlackBoxTests::validate(srcBuffer, outputBuffer, allocSize);
        if (!validRet) {
            std::cerr << "testVisitReappend: data mismatch after visit+reappend!\n";
        }
        if (useConcreteVisitors) {
            bool expectedCallbacksInvoked = visitUserData.memoryCopyCount == 2 &&
                                            visitUserData.barrierCount == 2 &&
                                            visitUserData.launchKernelCount == 1;
            if (!expectedCallbacksInvoked) {
                std::cerr << "Unexpected visitor callback counts: memoryCopy=" << visitUserData.memoryCopyCount
                          << ", barrier=" << visitUserData.barrierCount
                          << ", launchKernel=" << visitUserData.launchKernelCount << "\n";
            }
            validRet &= expectedCallbacksInvoked;

            // Report profiling results from concrete visitor callbacks if enabled
            if (useProfilingCallbacks) {
                ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2};
                SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
                const double nsPerCycle = 1000000000.0 / static_cast<double>(deviceProperties.timerResolution);

                std::cout << "\nVisit concrete visitor profiling results (timerResolution=" << std::dec
                          << deviceProperties.timerResolution << " cycles/s):\n";

                for (uint32_t i = 0; i < visitUserData.eventIndex && i < visitUserData.apiNames.size(); ++i) {
                    ze_kernel_timestamp_result_t ts = {};
                    auto queryResult = zeEventQueryKernelTimestamp(visitUserData.profilingEvents[i], &ts);
                    if (queryResult == ZE_RESULT_SUCCESS) {
                        uint64_t durationCycles = ts.context.kernelEnd - ts.context.kernelStart;
                        double durationNs = static_cast<double>(durationCycles) * nsPerCycle;
                        std::cout << "  [" << i << "] " << visitUserData.apiNames[i]
                                  << " duration: " << std::dec << durationCycles << " cycles, "
                                  << durationNs << " ns\n";
                    } else {
                        std::cout << "  [" << i << "] " << visitUserData.apiNames[i]
                                  << " timestamp not available, query result=" << std::hex << queryResult << "\n";
                    }
                }
            }
        }

        if (useProfilingCallbacks) {
            bool profilingStateValid = !profilingData.overflow && profilingData.nextEventIndex == profilingData.apiNames.size();
            if (!profilingStateValid) {
                std::cerr << "Profiling callback bookkeeping overflow or mismatch. eventsUsed=" << profilingData.nextEventIndex
                          << ", namesTracked=" << profilingData.apiNames.size() << "\n";
                validRet = false;
            }

            ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2};
            SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
            const double nsPerCycle = 1000000000.0 / static_cast<double>(deviceProperties.timerResolution);

            std::cout << "Visit default-op profiling results (timerResolution=" << std::dec
                      << deviceProperties.timerResolution << " cycles/s):\n";

            for (uint32_t i = 0; i < profilingData.nextEventIndex; ++i) {
                ze_kernel_timestamp_result_t ts = {};
                auto queryResult = zeEventQueryKernelTimestamp(profilingData.events[i], &ts);
                if (queryResult == ZE_RESULT_SUCCESS) {
                    uint64_t durationCycles = ts.context.kernelEnd - ts.context.kernelStart;
                    double durationNs = static_cast<double>(durationCycles) * nsPerCycle;
                    std::cout << "  [" << i << "] " << profilingData.apiNames[i]
                              << " duration: " << std::dec << durationCycles << " cycles, "
                              << durationNs << " ns\n";
                } else {
                    std::cout << "  [" << i << "] " << profilingData.apiNames[i]
                              << " timestamp not available, query result=" << std::hex << queryResult << "\n";
                }
            }
        }
    }

    // ---------- cleanup ----------
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(immCmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(recordCmdList));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstDevBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, interimBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, outputBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    if (useProfilingCallbacks) {
        for (auto event : profilingData.events) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        }
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(profilingEventPool));
    }
    return validRet;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestVisitReappendDefaultOp = 0u;
    constexpr uint32_t bitNumberTestVisitReappendConcreteVisitors = 1u;
    constexpr uint32_t bitNumberTestVisitReappendDefaultOpProfiling = 2u;
    constexpr uint32_t bitNumberTestReappendConcreteVisitorsWithProfiling = 3u;

    constexpr uint32_t defaultTestMask = std::numeric_limits<uint32_t>::max();
    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, defaultTestMask);

    const std::string blackBoxName("Zello Visit");

    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    auto &visitApi = LevelZeroBlackBoxTests::VisitExtension::loadVisitApi(driverHandle);
    if (!visitApi.valid()) {
        std::cerr << "Visit extension API is not available!" << std::endl;
        return 0;
    }

    // Build the test module (provides "memcpy_bytes" and other kernels)
    ze_module_handle_t module = nullptr;
    LevelZeroBlackBoxTests::createModuleFromSpirV(context, device, LevelZeroBlackBoxTests::openCLKernelsSource, module);

    bool boxPass = true;
    std::string currentTest;

    if (testMask.test(bitNumberTestVisitReappendDefaultOp)) {
        currentTest = "Visit Reappend Default Op (MemoryCopy + LaunchKernel)";
        std::cout << "Starting test: " << currentTest << std::endl;
        bool casePass = testVisitReappend(visitApi, context, device, module, false, false, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestVisitReappendConcreteVisitors)) {
        currentTest = "Visit Reappend Concrete Visitors (MemoryCopy + LaunchKernel)";
        std::cout << "Starting test: " << currentTest << std::endl;
        bool casePass = testVisitReappend(visitApi, context, device, module, true, false, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestVisitReappendDefaultOpProfiling)) {
        currentTest = "Visit Reappend Default Op Profiling (before/after callbacks)";
        std::cout << "Starting test: " << currentTest << std::endl;
        bool casePass = testVisitReappend(visitApi, context, device, module, false, true, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestReappendConcreteVisitorsWithProfiling)) {
        currentTest = "Visit Reappend Concrete Visitors with Profiling (MemoryCopy + LaunchKernel)";
        std::cout << "Starting test: " << currentTest << std::endl;
        bool casePass = testVisitReappend(visitApi, context, device, module, true, true, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    int mainRetCode = aubMode ? 0 : (boxPass ? 0 : 1);
    std::string finalStatus = (mainRetCode != 0) ? " FAILED" : " SUCCESS";
    std::cerr << blackBoxName << finalStatus << std::endl;
    return mainRetCode;
}
