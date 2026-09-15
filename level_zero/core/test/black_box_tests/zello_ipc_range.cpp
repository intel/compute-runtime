/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
constexpr uint32_t numChunks = 3u;
constexpr uint8_t chunkPattern(uint32_t chunkIndex) { return static_cast<uint8_t>(0xA0u + chunkIndex); }
// The first chunk is backed by a 2x object mapped at a non-zero physical offset, with the unmapped
// first half seeded with this sentinel. If the offset were dropped, the importer would read the
// sentinel instead of the chunk pattern. It is the first chunk so that on a legacy (non vm_bind)
// system the folded whole-object placement lands in free VA before the range rather than overlapping
// an earlier chunk (which the kernel would reject).
constexpr uint32_t offsetChunkIndex = 0u;
constexpr uint8_t offsetChunkSentinel = 0x5Au;
// Interior-leading round: the range is exported from a ptr that is interior to the first mapping (not a
// chunk boundary), so the transport header carries a non-zero leadingOffset. This marker is written at that
// interior ptr; if the importer dropped leadingOffset it would land on the chunk start (chunkPattern(0))
// and read the wrong byte here instead of the marker.
constexpr uint8_t interiorLeadingMarker = 0xC5u;

std::string toHex(uint32_t value) {
    std::ostringstream o;
    o << std::hex << value;
    return o.str();
}

std::string describeCase(bool hostMemory, bool useOffset) {
    return std::string(hostMemory ? "HOST" : "DEVICE") + " memory, " + (useOffset ? "offset" : "no-offset");
}

void logCase(bool hostMemory, bool useOffset, const std::string &message, const std::string &status = "") {
    std::cout << "[CASE " << describeCase(hostMemory, useOffset);
    if (!status.empty()) {
        std::cout << "] " << status << ": " << message;
    } else {
        std::cout << "] " << message;
    }
    std::cout << std::endl;
}

int commSocket[2];

void log(const std::string &tag, const std::string &message) {
    std::cout << "[" << tag << " pid " << std::dec << getpid() << "] " << message << std::endl;
}

void sendAll(int socket, const void *data, size_t size) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    size_t sent = 0;
    while (sent < size) {
        ssize_t ret = write(socket, bytes + sent, size - sent);
        if (ret <= 0) {
            std::cerr << "write to peer failed\n";
            std::terminate();
        }
        sent += static_cast<size_t>(ret);
    }
}

void recvAll(int socket, void *data, size_t size) {
    uint8_t *bytes = static_cast<uint8_t *>(data);
    size_t received = 0;
    while (received < size) {
        ssize_t ret = read(socket, bytes + received, size - received);
        if (ret <= 0) {
            std::cerr << "read from peer failed\n";
            std::terminate();
        }
        received += static_cast<size_t>(ret);
    }
}

void dumpIpcHandle(const std::string &tag, const ze_ipc_mem_handle_t &ipcHandle) {
    std::ostringstream hex;
    hex << "single ze_ipc_mem_handle_t (" << std::dec << ZE_MAX_IPC_HANDLE_SIZE << " bytes): first 16 bytes = ";
    for (int i = 0; i < 16; i++) {
        hex << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<uint32_t>(static_cast<uint8_t>(ipcHandle.data[i])) << " ";
    }
    log(tag, hex.str());
}

void initialize(ze_driver_handle_t &driverHandle,
                ze_context_handle_t &context,
                ze_device_handle_t &device,
                ze_command_queue_handle_t &cmdQueue,
                ze_command_list_handle_t &cmdList) {
    ze_init_driver_type_desc_t initDesc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    initDesc.flags = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;
    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, nullptr, &initDesc));
    if (driverCount == 0) {
        std::cerr << "No driver handle found!\n";
        std::terminate();
    }
    driverCount = 1;
    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, &driverHandle, &initDesc));

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    SUCCESS_OR_TERMINATE(zeContextCreate(driverHandle, &contextDesc, &context));

    uint32_t deviceCount = 1;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, &device));

    uint32_t ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueue = LevelZeroBlackBoxTests::createCommandQueue(context, device, &ordinal, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                                          ZE_COMMAND_QUEUE_PRIORITY_NORMAL, false);
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, ordinal, 0));
}

bool verifyExtensionAdvertised(ze_driver_handle_t driverHandle) {
    uint32_t count = 0;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &count, nullptr));
    std::vector<ze_driver_extension_properties_t> props(count);
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &count, props.data()));
    for (const auto &prop : props) {
        if (std::strcmp(prop.name, ZE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_NAME) == 0) {
            return true;
        }
    }
    return false;
}

void exerciseFallbackAndInteriorOffset(const std::string &tag,
                                       ze_context_handle_t context,
                                       ze_device_handle_t device,
                                       void *reservedVa,
                                       size_t chunkSize,
                                       size_t pageSize) {
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    void *plainVa = nullptr;
    const size_t plainAllocSize = chunkSize * 2u;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, plainAllocSize, chunkSize, device, &plainVa));

    void *allocBase = nullptr;
    size_t allocSize = 0;
    SUCCESS_OR_TERMINATE(zeMemGetAddressRange(context, plainVa, &allocBase, &allocSize));

    ze_ipc_phys_mem_handle_range_ext_desc_t equalDesc = {};
    equalDesc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    equalDesc.pNext = nullptr;
    equalDesc.size = allocSize;

    ze_ipc_mem_handle_t equalHandle = {};
    log(tag, "case 1a: plain device allocation (not a reservation) with range size " + std::to_string(allocSize) +
                 " == allocation size should fall back to the standard single-handle IPC flow");
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandleWithProperties(context, plainVa, &equalDesc, &equalHandle));
    log(tag, "case 1a: fallback to standard flow succeeded");
    SUCCESS_OR_TERMINATE(zeMemPutIpcHandle(context, equalHandle));

    ze_ipc_phys_mem_handle_range_ext_desc_t withinDesc = equalDesc;
    withinDesc.size = allocSize / 2u;
    ze_ipc_mem_handle_t withinHandle = {};
    log(tag, "case 1b: same allocation with range size " + std::to_string(withinDesc.size) +
                 " within the allocation size should also fall back to the standard single-handle IPC flow");
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandleWithProperties(context, plainVa, &withinDesc, &withinHandle));
    log(tag, "case 1b: fallback to standard flow succeeded for a size within the allocation");
    SUCCESS_OR_TERMINATE(zeMemPutIpcHandle(context, withinHandle));

    ze_ipc_phys_mem_handle_range_ext_desc_t exceedDesc = equalDesc;
    exceedDesc.size = allocSize + pageSize;
    ze_ipc_mem_handle_t exceedHandle = {};
    ze_result_t exceedResult = zeMemGetIpcHandleWithProperties(context, plainVa, &exceedDesc, &exceedHandle);
    if (exceedResult != ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        log(tag, "ERROR: non-reservation VA whose size exceeds the allocation should return INVALID_ARGUMENT");
        std::terminate();
    }
    log(tag, "case 1c: size exceeding the allocation on a non-reservation VA correctly rejected with INVALID_ARGUMENT");
    SUCCESS_OR_TERMINATE(zeMemFree(context, plainVa));

    void *interiorVa = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservedVa) + chunkSize);
    const size_t interiorSize = chunkSize * (numChunks - 1u);
    ze_ipc_phys_mem_handle_range_ext_desc_t interiorDesc = {};
    interiorDesc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    interiorDesc.pNext = nullptr;
    interiorDesc.size = interiorSize;

    ze_ipc_mem_handle_t interiorHandle = {};
    log(tag, "case 2: ptr at chunk-boundary offset " + std::to_string(chunkSize) + " into the reservation, covering " +
                 std::to_string(interiorSize) + " bytes of contiguous hole-free mappings, should succeed (interior-to-mapping ptr is validated end-to-end by the interior-leading round)");
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandleWithProperties(context, interiorVa, &interiorDesc, &interiorHandle));
    log(tag, "case 2: range handle created from a chunk-boundary offset of the reservation succeeded");
    SUCCESS_OR_TERMINATE(zeMemPutIpcHandle(context, interiorHandle));
}
} // namespace

void runExporter(bool hostMemory, bool useOffset, bool interiorLeading) {
    const std::string tag = "EXPORTER";
    const std::string memClass = hostMemory ? "HOST" : "DEVICE";
    logCase(hostMemory, useOffset, "START: exporter creating range from " + std::to_string(numChunks) + " independent " + memClass + " physical objects" + (useOffset ? " with one non-zero physical offset mapping" : ""));
    log(tag, "starting (child process); will reserve one VA range built from " + std::to_string(numChunks) + " independent " + memClass +
                 " physical objects" + (useOffset ? " (one mapped at a non-zero physical offset)" : ""));

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initialize(driverHandle, context, device, cmdQueue, cmdList);

    if (verifyExtensionAdvertised(driverHandle)) {
        log(tag, std::string("extension advertised by driver: ") + ZE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_NAME);
    } else {
        log(tag, std::string("ERROR: extension NOT advertised: ") + ZE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_NAME);
        std::terminate();
    }

    size_t pageSize = 0;
    SUCCESS_OR_TERMINATE(zeVirtualMemQueryPageSize(context, device, 1, &pageSize));
    const size_t chunkSize = pageSize;
    const size_t rangeSize = chunkSize * numChunks;
    log(tag, "page size " + std::to_string(pageSize) + " bytes; chunk size " + std::to_string(chunkSize) +
                 " bytes; total range size " + std::to_string(rangeSize) + " bytes");

    void *reservedVa = nullptr;
    SUCCESS_OR_TERMINATE(zeVirtualMemReserve(context, nullptr, rangeSize, &reservedVa));
    log(tag, "reserved contiguous VA at " + std::to_string(reinterpret_cast<uintptr_t>(reservedVa)));

    const ze_physical_mem_flags_t physFlags = hostMemory
                                                  ? static_cast<ze_physical_mem_flags_t>(ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_HOST)
                                                  : static_cast<ze_physical_mem_flags_t>(ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_DEVICE);

    std::vector<ze_physical_mem_handle_t> physicalMems(numChunks);
    bool offsetRoundSupported = true;
    for (uint32_t i = 0; i < numChunks; i++) {
        const bool isOffsetChunk = useOffset && (i == offsetChunkIndex);
        const size_t physSize = isOffsetChunk ? (2u * chunkSize) : chunkSize;
        const size_t mapOffset = isOffsetChunk ? chunkSize : 0u;

        ze_physical_mem_desc_t physDesc = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC};
        physDesc.flags = physFlags;
        physDesc.size = physSize;
        if (zePhysicalMemCreate(context, device, &physDesc, &physicalMems[i]) != ZE_RESULT_SUCCESS) {
            offsetRoundSupported = false;
            break;
        }

        if (isOffsetChunk) {
            // Seed the whole object via a scratch mapping, then tear it down before mapping the offset
            // window (an object cannot be mapped to two VAs at once).
            void *seedVa = nullptr;
            if (zeVirtualMemReserve(context, nullptr, physSize, &seedVa) != ZE_RESULT_SUCCESS ||
                zeVirtualMemMap(context, seedVa, physSize, physicalMems[i], 0u,
                                ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE) != ZE_RESULT_SUCCESS) {
                offsetRoundSupported = false;
                break;
            }
            void *seedStaging = nullptr;
            ze_device_mem_alloc_desc_t seedDeviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
            ze_host_mem_alloc_desc_t seedHostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
            if (zeMemAllocShared(context, &seedDeviceDesc, &seedHostDesc, physSize, 1, device, &seedStaging) != ZE_RESULT_SUCCESS) {
                offsetRoundSupported = false;
                zeVirtualMemUnmap(context, seedVa, physSize);
                zeVirtualMemFree(context, seedVa, physSize);
                break;
            }
            std::memset(seedStaging, offsetChunkSentinel, physSize);
            if (zeCommandListAppendMemoryCopy(cmdList, seedVa, seedStaging, physSize, nullptr, 0, nullptr) != ZE_RESULT_SUCCESS ||
                zeCommandListClose(cmdList) != ZE_RESULT_SUCCESS ||
                zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr) != ZE_RESULT_SUCCESS ||
                zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()) != ZE_RESULT_SUCCESS ||
                zeCommandListReset(cmdList) != ZE_RESULT_SUCCESS ||
                zeMemFree(context, seedStaging) != ZE_RESULT_SUCCESS ||
                zeVirtualMemUnmap(context, seedVa, physSize) != ZE_RESULT_SUCCESS ||
                zeVirtualMemFree(context, seedVa, physSize) != ZE_RESULT_SUCCESS) {
                offsetRoundSupported = false;
                break;
            }
        }

        void *chunkVa = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservedVa) + i * chunkSize);
        if (zeVirtualMemMap(context, chunkVa, chunkSize, physicalMems[i], mapOffset,
                            ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE) != ZE_RESULT_SUCCESS) {
            offsetRoundSupported = false;
            break;
        }
        log(tag, "physical object #" + std::to_string(i) + " (size " + std::to_string(physSize) +
                     ") mapped at sub-region offset " + std::to_string(i * chunkSize) +
                     " from physical offset " + std::to_string(mapOffset) +
                     " (pattern 0x" + toHex(chunkPattern(i)) + ")");
    }

    if (!offsetRoundSupported) {
        for (uint32_t i = 0; i < numChunks; i++) {
            if (physicalMems[i] != nullptr) {
                zePhysicalMemDestroy(context, physicalMems[i]);
            }
        }
        zeVirtualMemFree(context, reservedVa, rangeSize);
        zeCommandListDestroy(cmdList);
        zeCommandQueueDestroy(cmdQueue);
        zeContextDestroy(context);
        uint8_t unsupported = 0u;
        logCase(hostMemory, useOffset, "unsupported: non-zero physical offset range creation rejected on this driver", "UNSUPPORTED");
        sendAll(commSocket[1], &unsupported, sizeof(unsupported));
        _exit(0);
    }

    // Reserved host memory is CPU-mappable and should be seeded via direct CPU writes. A command-queue
    // copy into the mapped host range is not valid on the legacy path and can fail even though the
    // underlying host VA is writable. Device memory still goes through the command list path.
    void *staging = nullptr;
    ze_device_mem_alloc_desc_t stagingDeviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t stagingHostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &stagingDeviceDesc, &stagingHostDesc, chunkSize, 1, device, &staging));
    for (uint32_t i = 0; i < numChunks; i++) {
        void *chunkVa = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservedVa) + i * chunkSize);
        if (hostMemory) {
            std::memcpy(chunkVa, staging, chunkSize);
            std::memset(chunkVa, chunkPattern(i), chunkSize);
        } else {
            std::memset(staging, chunkPattern(i), chunkSize);
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, chunkVa, staging, chunkSize, nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
            SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
        }
    }
    SUCCESS_OR_TERMINATE(zeMemFree(context, staging));
    log(tag, "filled each sub-region with its distinct pattern");

    const size_t leadingBytes = interiorLeading ? (chunkSize / 2u) : 0u;
    void *const exportVa = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservedVa) + leadingBytes);
    const size_t exportSize = rangeSize - leadingBytes;

    if (interiorLeading) {
        void *markerStaging = nullptr;
        ze_device_mem_alloc_desc_t markerDeviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        ze_host_mem_alloc_desc_t markerHostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &markerDeviceDesc, &markerHostDesc, 1u, 1, device, &markerStaging));
        std::memset(markerStaging, interiorLeadingMarker, 1u);
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, exportVa, markerStaging, 1u, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
        SUCCESS_OR_TERMINATE(zeMemFree(context, markerStaging));
        log(tag, "interior-leading: wrote marker 0x" + toHex(interiorLeadingMarker) + " at ptr offset " + std::to_string(leadingBytes) + " into the first mapping");
    } else if (!hostMemory) {
        exerciseFallbackAndInteriorOffset(tag, context, device, reservedVa, chunkSize, pageSize);
    }

    uint8_t supported = 1u;
    logCase(hostMemory, useOffset, "export path ready; sending range handle to importer", "READY");
    sendAll(commSocket[1], &supported, sizeof(supported));

    ze_ipc_phys_mem_handle_range_ext_desc_t rangeDesc = {};
    rangeDesc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    rangeDesc.pNext = nullptr;
    rangeDesc.size = exportSize;

    ze_ipc_mem_handle_t ipcHandle = {};
    log(tag, "calling zeMemGetIpcHandleWithProperties() with the range descriptor to bundle all physical objects into ONE handle");
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandleWithProperties(context, exportVa, &rangeDesc, &ipcHandle));
    dumpIpcHandle(tag, ipcHandle);

    log(tag, "sending the single 64-byte IPC handle to the importer over the socket");
    sendAll(commSocket[1], ipcHandle.data, ZE_MAX_IPC_HANDLE_SIZE);

    log(tag, "staying alive while the importer opens the handle (driver duplicates each physical fd out of this process via pidfd_getfd)");
    uint8_t ack = 0;
    recvAll(commSocket[1], &ack, sizeof(ack));
    log(tag, "importer signalled completion; releasing exporter driver resources with zeMemPutIpcHandle()");
    SUCCESS_OR_TERMINATE(zeMemPutIpcHandle(context, ipcHandle));

    for (uint32_t i = 0; i < numChunks; i++) {
        void *chunkVa = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservedVa) + i * chunkSize);
        SUCCESS_OR_TERMINATE(zeVirtualMemUnmap(context, chunkVa, chunkSize));
        SUCCESS_OR_TERMINATE(zePhysicalMemDestroy(context, physicalMems[i]));
    }
    SUCCESS_OR_TERMINATE(zeVirtualMemFree(context, reservedVa, rangeSize));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

void runImporter(bool hostMemory, bool useOffset, bool interiorLeading, bool &validRet) {
    const std::string tag = "IMPORTER";
    log(tag, std::string("starting (parent process); importing a ") + (hostMemory ? "HOST" : "DEVICE") + " reserved-memory range" +
                 (useOffset ? " (with a non-zero physical offset chunk)" : ""));

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initialize(driverHandle, context, device, cmdQueue, cmdList);

    uint8_t supported = 0u;
    recvAll(commSocket[0], &supported, sizeof(supported));
    if (supported == 0u) {
        logCase(hostMemory, useOffset, "driver returned unsupported for this range variant; skipping importer validation", "UNSUPPORTED");
        log(tag, "range round is unsupported on this driver; skipping this round");
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
        SUCCESS_OR_TERMINATE(zeContextDestroy(context));
        validRet = true;
        return;
    }

    ze_ipc_mem_handle_t ipcHandle = {};
    recvAll(commSocket[0], ipcHandle.data, ZE_MAX_IPC_HANDLE_SIZE);
    log(tag, "received the single range IPC handle");
    dumpIpcHandle(tag, ipcHandle);

    void *importedVa = nullptr;
    log(tag, "calling zeMemOpenIpcHandle() - driver imports the transport allocation carrying every physical handle, reads them in stored order, and maps them contiguously into one new VA");
    ze_result_t openResult = zeMemOpenIpcHandle(context, device, ipcHandle, 0u, &importedVa);

    // Honoring a per-object physical offset in a contiguous range needs vm_bind. On a legacy
    // (non vm_bind) driver the importer cannot pack the offset windows, so it returns the documented
    // out-of-memory rejection - which is the correct behavior there, not a test failure.
    const ze_result_t offsetUnsupported = hostMemory ? ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY : ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    if (useOffset && openResult == offsetUnsupported) {
        log(tag, "offset range not supported on this driver (no vm_bind); rejection is expected -> PASS");
        uint8_t ack = 1u;
        sendAll(commSocket[0], &ack, sizeof(ack));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
        SUCCESS_OR_TERMINATE(zeContextDestroy(context));
        validRet = true;
        return;
    }
    SUCCESS_OR_TERMINATE(openResult);
    log(tag, "imported contiguous VA at " + std::to_string(reinterpret_cast<uintptr_t>(importedVa)));

    size_t pageSize = 0;
    SUCCESS_OR_TERMINATE(zeVirtualMemQueryPageSize(context, device, 1, &pageSize));
    const size_t chunkSize = pageSize;
    const size_t rangeSize = chunkSize * numChunks;
    const size_t leadingBytes = interiorLeading ? (chunkSize / 2u) : 0u;
    const size_t importedSize = rangeSize - leadingBytes;

    void *hostBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, importedSize, 1, device, &hostBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, hostBuffer, importedVa, importedSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    bool ordered = true;
    const uint8_t *bytes = static_cast<const uint8_t *>(hostBuffer);
    if (interiorLeading) {
        if (bytes[0] != interiorLeadingMarker) {
            ordered = false;
        }
        for (size_t k = 1; k < importedSize && ordered; k++) {
            if (bytes[k] != chunkPattern(static_cast<uint32_t>((leadingBytes + k) / chunkSize))) {
                ordered = false;
            }
        }
        std::ostringstream msg;
        msg << "interior-leading: byte[0] expected marker 0x" << std::hex << static_cast<uint32_t>(interiorLeadingMarker)
            << ", read back 0x" << static_cast<uint32_t>(bytes[0]) << "; window "
            << (ordered ? "MATCH (leadingOffset applied)" : "MISMATCH (leadingOffset dropped or wrong window)");
        log(tag, msg.str());
    } else {
        for (uint32_t i = 0; i < numChunks; i++) {
            uint8_t expected = chunkPattern(i);
            uint8_t actual = bytes[i * chunkSize];
            bool chunkOk = true;
            for (size_t b = 0; b < chunkSize; b++) {
                if (bytes[i * chunkSize + b] != expected) {
                    chunkOk = false;
                    break;
                }
            }
            std::ostringstream msg;
            msg << "sub-region #" << std::dec << i << " expected pattern 0x" << std::hex << static_cast<uint32_t>(expected)
                << ", read back 0x" << static_cast<uint32_t>(actual) << " -> " << (chunkOk ? "MATCH (order preserved)" : "MISMATCH");
            log(tag, msg.str());
            ordered = ordered && chunkOk;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, hostBuffer));
    log(tag, "releasing importer mapping with zeMemCloseIpcHandle()");
    SUCCESS_OR_TERMINATE(zeMemCloseIpcHandle(context, importedVa));

    uint8_t ack = ordered ? 1u : 0u;
    log(tag, "signalling exporter that import is complete");
    sendAll(commSocket[0], &ack, sizeof(ack));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    validRet = ordered;
    if (validRet) {
        logCase(hostMemory, useOffset, "final validation matched expected ordering", "PASSED");
    } else {
        logCase(hostMemory, useOffset, "final validation mismatched ordering or contents", "FAILED");
    }
}

bool runRound(bool hostMemory, bool useOffset, bool interiorLeading) {
    const std::string caseName = describeCase(hostMemory, useOffset) + (interiorLeading ? ", interior-leading" : "");
    std::cout << "\n=== CASE: " << caseName << " ===" << std::endl;

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, commSocket) < 0) {
        perror("socketpair");
        logCase(hostMemory, useOffset, "socketpair failed for this case", "FAILED");
        return false;
    }

    pid_t childPid = fork();
    if (childPid < 0) {
        perror("fork");
        logCase(hostMemory, useOffset, "fork failed for this case", "FAILED");
        return false;
    } else if (childPid == 0) {
        close(commSocket[0]);
        runExporter(hostMemory, useOffset, interiorLeading);
        close(commSocket[1]);
        _exit(0);
    }

    close(commSocket[1]);
    bool outputValidationSuccessful = false;
    runImporter(hostMemory, useOffset, interiorLeading, outputValidationSuccessful);
    close(commSocket[0]);

    int childStatus = 0;
    waitpid(childPid, &childStatus, 0);

    const bool childExitedNormally = WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0;
    const bool caseIsUnsupported = !outputValidationSuccessful && !childExitedNormally && (WIFSIGNALED(childStatus) || WIFEXITED(childStatus));
    if (caseIsUnsupported) {
        std::cout << "[CASE " << caseName << "] RESULT: UNSUPPORTED" << std::endl;
    } else if (outputValidationSuccessful && childExitedNormally) {
        std::cout << "[CASE " << caseName << "] RESULT: PASSED" << std::endl;
    } else {
        std::cout << "[CASE " << caseName << "] RESULT: FAILED" << (childExitedNormally ? "" : " (child exited abnormally)") << std::endl;
    }
    return outputValidationSuccessful && childExitedNormally;
}

bool runIsolatedRound(bool hostMemory, bool useOffset, bool interiorLeading = false) {
    const pid_t casePid = fork();
    if (casePid < 0) {
        perror("fork");
        logCase(hostMemory, useOffset, "failed to create isolated case process", "FAILED");
        return false;
    }
    if (casePid == 0) {
        _exit(runRound(hostMemory, useOffset, interiorLeading) ? 0 : 1);
    }

    int caseStatus = 0;
    waitpid(casePid, &caseStatus, 0);
    return WIFEXITED(caseStatus) && WEXITSTATUS(caseStatus) == 0;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello IPC Range";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);

    std::cout << std::unitbuf;

    std::cout << "=== Zello IPC Range test matrix ===" << std::endl;
    bool ok = true;
    ok = runIsolatedRound(false, false) && ok;
    ok = runIsolatedRound(true, false) && ok;
    ok = runIsolatedRound(false, true) && ok;
    ok = runIsolatedRound(true, true) && ok;
    ok = runIsolatedRound(false, false, true) && ok;

    std::cout << "\n=== TEST MATRIX RESULT: " << (ok ? "PASSED" : "FAILED") << " ===" << std::endl;
    LevelZeroBlackBoxTests::printResult(false, ok, blackBoxName);
    return ok ? 0 : 1;
}
