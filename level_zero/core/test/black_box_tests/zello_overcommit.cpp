/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Verifies the failure point when the device USM memory limit is reached.

uint64_t queryDeviceLocalMemoryBytes(ze_device_handle_t device) {
    uint32_t count = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGetMemoryProperties(device, &count, nullptr));
    if (count == 0) {
        return 0;
    }
    std::vector<ze_device_memory_properties_t> memProps(count);
    for (auto &prop : memProps) {
        prop.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
        prop.pNext = nullptr;
    }
    SUCCESS_OR_TERMINATE(zeDeviceGetMemoryProperties(device, &count, memProps.data()));
    uint64_t maxSize = 0;
    for (const auto &prop : memProps) {
        if (prop.totalSize > maxSize) {
            maxSize = prop.totalSize;
        }
    }
    return maxSize;
}

} // namespace

int main(int argc, char *argv[]) {
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);

    const uint32_t chunkMb = std::max(1u, LevelZeroBlackBoxTests::getParamValue(argc, argv, "-s", "--chunk-mb", 1024u));
    const uint32_t maxGb = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-g", "--max-gb", 0u);
    const bool touch = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-t", "--touch");

    const std::string blackBoxName("Zello Overcommit");

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device0 = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    const uint64_t totalBytes = queryDeviceLocalMemoryBytes(device0);
    const uint64_t chunkBytes = static_cast<uint64_t>(chunkMb) * (1ull << 20);
    const uint64_t maxBytes = (maxGb != 0u) ? (static_cast<uint64_t>(maxGb) * (1ull << 30))
                                            : (totalBytes + totalBytes / 2ull);
    const uint32_t maxChunks = static_cast<uint32_t>(std::max<uint64_t>(1ull, maxBytes / chunkBytes));

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Overcommit reproducer: device local memory=" << (totalBytes >> 20) << "MB"
                  << " chunk=" << chunkMb << "MB"
                  << " target=" << (maxBytes >> 30) << "GB (" << maxChunks << " chunks)"
                  << " touch=" << (touch ? "yes" : "no") << std::endl;
    }

    ze_command_list_handle_t immCmdList = nullptr;
    if (touch) {
        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device0, &queueDesc, &immCmdList));
    }

    std::vector<void *> allocations;
    allocations.reserve(maxChunks);
    uint64_t allocatedBytes = 0;
    bool allocFailed = false;
    bool touchFailed = false;
    uint32_t crossedPhysicalAtChunk = 0;

    for (uint32_t i = 0; i < maxChunks; i++) {
        void *ptr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        const auto allocResult = zeMemAllocDevice(context, &deviceDesc, chunkBytes, 4096, device0, &ptr);
        if (allocResult != ZE_RESULT_SUCCESS || ptr == nullptr) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "  alloc #" << (i + 1) << " FAILED at requested cumulative "
                          << ((allocatedBytes + chunkBytes) >> 30) << "GB, result=0x" << std::hex << allocResult << std::dec << std::endl;
            }
            allocFailed = true;
            break;
        }
        allocations.push_back(ptr);
        allocatedBytes += chunkBytes;

        if (touch) {
            const uint8_t pattern = static_cast<uint8_t>(i & 0xff);
            const auto fillResult = zeCommandListAppendMemoryFill(immCmdList, ptr, &pattern, sizeof(pattern), chunkBytes, nullptr, 0, nullptr);
            if (fillResult != ZE_RESULT_SUCCESS) {
                if (LevelZeroBlackBoxTests::verbose) {
                    std::cout << "  touch #" << (i + 1) << " FAILED at backed cumulative "
                              << (allocatedBytes >> 30) << "GB, result=0x" << std::hex << fillResult << std::dec << std::endl;
                }
                touchFailed = true;
                break;
            }
        }

        const uint64_t percent = (totalBytes != 0) ? (allocatedBytes * 100ull / totalBytes) : 0;
        if (crossedPhysicalAtChunk == 0 && allocatedBytes > totalBytes) {
            crossedPhysicalAtChunk = i + 1;
        }
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "  alloc #" << (i + 1) << " ok cumulative=" << (allocatedBytes >> 30) << "GB ("
                      << percent << "% of VRAM)" << (allocatedBytes > totalBytes ? " [OVERCOMMIT]" : "") << std::endl;
        }
    }

    const bool overcommitReached = allocatedBytes > totalBytes;
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "==== Summary ====" << std::endl;
        std::cout << "  buffers allocated : " << allocations.size() << std::endl;
        std::cout << "  total allocated   : " << (allocatedBytes >> 30) << "GB of " << (totalBytes >> 20) << "MB VRAM" << std::endl;
        std::cout << "  overcommit reached: " << (overcommitReached ? "yes" : "no");
        if (crossedPhysicalAtChunk != 0) {
            std::cout << " (crossed 100% VRAM at alloc #" << crossedPhysicalAtChunk << ")";
        }
        std::cout << std::endl;
        std::cout << "  alloc failure     : " << (allocFailed ? "yes" : "no") << std::endl;
        if (touch) {
            std::cout << "  touch failure     : " << (touchFailed ? "yes" : "no") << std::endl;
        }
    }

    for (auto ptr : allocations) {
        SUCCESS_OR_TERMINATE(zeMemFree(context, ptr));
    }
    if (immCmdList != nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(immCmdList));
    }

    const bool boxPass = overcommitReached && !allocFailed && !touchFailed;
    LevelZeroBlackBoxTests::printResult(false, boxPass, blackBoxName);
    return boxPass ? 0 : 1;
}
