/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstring>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// This black box exercises exporting/importing an IPC handle for physical
// memory that was mapped to a reserved virtual address at a NON-ZERO physical
// offset (zeVirtualMemMap(..., offset, ...)). It verifies that the physical
// offset survives the IPC round-trip, i.e. the importing process ends up looking
// at the SAME physical pages the exporter mapped at that offset - not at physical
// offset 0. Both physical DEVICE memory and physical HOST memory are covered.
//
// The opaque IPC handle blob is exchanged through a shared-memory control block
// (no sockets, no fd passing): the exporter's dma-buf fd is re-acquired by the
// importer through the driver's opaque-handle mechanism (pidfd). A pair of
// spin flags in the same shared block provides the server<->client handshake.
//
// Layout of the physical allocation (2 pages):
//   page 0 : sentinel bytes (0xCC)          <- would be seen if the offset were dropped
//   page 1 : mapped by the server at offset=pageSize and shared over IPC
//
// The server writes an ascending pattern into page 1, the client validates it
// reads that exact pattern (proving correct offset), then writes its own
// pattern back; the server finally validates it observes the client's pattern
// through its own offset mapping.

struct SharedControl {
    volatile int serverReady;
    volatile int clientDone;
    volatile int clientMatched;
    volatile int serverValid;
    char ipcBlob[ZE_MAX_IPC_HANDLE_SIZE];
};

SharedControl *shared = nullptr;

constexpr uint8_t sentinelByte = 0xCC;

inline uint8_t serverByte(size_t i) {
    return static_cast<uint8_t>(1u + i);
}

inline uint8_t clientByte(size_t i) {
    return static_cast<uint8_t>(0x80u + i);
}

inline bool spinUntil(volatile int &flag) {
    for (int waitedMs = 0; waitedMs < 60000; waitedMs++) {
        if (flag != 0) {
            return true;
        }
        usleep(1000);
    }
    return false;
}

inline void initializeProcess(ze_driver_handle_t &driverHandle,
                              ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList) {
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.flags = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;

    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, nullptr, &desc));
    if (driverCount == 0) {
        std::cerr << "No driver handle found!\n";
        std::terminate();
    }

    driverCount = 1;
    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, &driverHandle, &desc));

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    SUCCESS_OR_TERMINATE(zeContextCreate(driverHandle, &contextDesc, &context));

    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    deviceCount = 1;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, &device));

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cerr << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    for (auto &queueProperty : queueProperties) {
        queueProperty.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
    }
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                queueProperties.data()));

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
        }
    }
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));
}

// Copy through the GPU: physical device memory is host-inaccessible, so all
// reads/writes to the mapped range go through a shared staging buffer. Physical
// host memory is host-accessible, but the same GPU copy path works for it too,
// so the flow is kept uniform for both memory kinds.
inline void gpuCopy(ze_command_queue_handle_t cmdQueue, ze_command_list_handle_t cmdList,
                    void *dst, const void *src, size_t size) {
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dst, const_cast<void *>(src), size,
                                                       nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
}

inline void *allocShared(ze_context_handle_t context, ze_device_handle_t device, size_t size) {
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, size, 1, device, &ptr));
    return ptr;
}

void runClient(bool hostMemory) {
    std::cout << "Client process ID: " << std::dec << getpid()
              << (hostMemory ? " [host]" : " [device]") << "\n";

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(driverHandle, context, device, cmdQueue, cmdList);

    size_t pageSize = 0;
    SUCCESS_OR_TERMINATE(zeVirtualMemQueryPageSize(context, device, 1u, &pageSize));
    const size_t mappedSize = pageSize;

    if (!spinUntil(shared->serverReady)) {
        std::cerr << "Client timed out waiting for server\n";
        std::terminate();
    }

    ze_ipc_mem_handle_t ipcHandle;
    memcpy(ipcHandle.data, shared->ipcBlob, ZE_MAX_IPC_HANDLE_SIZE);

    void *ipcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, ipcHandle, 0u, &ipcBuffer));

    void *readBack = allocShared(context, device, mappedSize);
    memset(readBack, 0, mappedSize);
    gpuCopy(cmdQueue, cmdList, readBack, ipcBuffer, mappedSize);

    bool matches = true;
    auto *observed = static_cast<uint8_t *>(readBack);
    for (size_t i = 0; i < mappedSize; i++) {
        if (observed[i] != serverByte(i)) {
            std::cerr << "Client mismatch at byte " << std::dec << i
                      << ": observed " << static_cast<uint32_t>(observed[i])
                      << ", expected " << static_cast<uint32_t>(serverByte(i)) << "\n";
            matches = false;
            break;
        }
    }

    void *staging = allocShared(context, device, mappedSize);
    auto *stagingBytes = static_cast<uint8_t *>(staging);
    for (size_t i = 0; i < mappedSize; i++) {
        stagingBytes[i] = clientByte(i);
    }
    gpuCopy(cmdQueue, cmdList, ipcBuffer, staging, mappedSize);

    SUCCESS_OR_TERMINATE(zeMemCloseIpcHandle(context, ipcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, staging));
    SUCCESS_OR_TERMINATE(zeMemFree(context, readBack));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    shared->clientMatched = matches ? 1 : 0;
    shared->clientDone = 1;
}

void runServer(bool hostMemory) {
    std::cout << "Server process ID: " << std::dec << getpid()
              << (hostMemory ? " [host]" : " [device]") << "\n";

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(driverHandle, context, device, cmdQueue, cmdList);

    size_t pageSize = 0;
    SUCCESS_OR_TERMINATE(zeVirtualMemQueryPageSize(context, device, 1u, &pageSize));
    if (pageSize == 0) {
        std::cerr << "Queried page size is zero\n";
        std::terminate();
    }
    const size_t mappedSize = pageSize;
    const size_t physSize = 2 * pageSize;
    const size_t physicalOffset = pageSize;

    ze_physical_mem_desc_t physDesc = {ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC};
    physDesc.flags = hostMemory ? static_cast<ze_physical_mem_flags_t>(ZE_PHYSICAL_MEM_FLAG_ALLOCATE_ON_HOST)
                                : static_cast<ze_physical_mem_flags_t>(0u);
    physDesc.size = physSize;
    ze_physical_mem_handle_t physMem = {};
    SUCCESS_OR_TERMINATE(zePhysicalMemCreate(context, device, &physDesc, &physMem));

    // Seed the whole physical allocation with a sentinel so that page 0 has a
    // deterministic value distinct from the pattern written to page 1. If the
    // offset were ignored on import, the client would read this sentinel instead
    // of the server's pattern and fail. The seed mapping is torn down before the
    // real one so it never overlaps the shared mapping.
    void *seedPtr = nullptr;
    SUCCESS_OR_TERMINATE(zeVirtualMemReserve(context, nullptr, physSize, &seedPtr));
    SUCCESS_OR_TERMINATE(zeVirtualMemMap(context, seedPtr, physSize, physMem, 0u,
                                         ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE));
    void *seedStaging = allocShared(context, device, physSize);
    memset(seedStaging, sentinelByte, physSize);
    gpuCopy(cmdQueue, cmdList, seedPtr, seedStaging, physSize);
    SUCCESS_OR_TERMINATE(zeMemFree(context, seedStaging));
    SUCCESS_OR_TERMINATE(zeVirtualMemUnmap(context, seedPtr, physSize));
    SUCCESS_OR_TERMINATE(zeVirtualMemFree(context, seedPtr, physSize));

    // Map only the second page (offset == pageSize) and share that mapping.
    void *serverPtr = nullptr;
    SUCCESS_OR_TERMINATE(zeVirtualMemReserve(context, nullptr, mappedSize, &serverPtr));
    SUCCESS_OR_TERMINATE(zeVirtualMemMap(context, serverPtr, mappedSize, physMem, physicalOffset,
                                         ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE));

    void *staging = allocShared(context, device, mappedSize);
    auto *stagingBytes = static_cast<uint8_t *>(staging);
    for (size_t i = 0; i < mappedSize; i++) {
        stagingBytes[i] = serverByte(i);
    }
    gpuCopy(cmdQueue, cmdList, serverPtr, staging, mappedSize);

    ze_ipc_mem_handle_t ipcHandle;
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandle(context, serverPtr, &ipcHandle));

    memcpy(shared->ipcBlob, ipcHandle.data, ZE_MAX_IPC_HANDLE_SIZE);
    shared->serverReady = 1;

    const bool clientResponded = spinUntil(shared->clientDone);
    const bool clientValidatedServerData = clientResponded && (shared->clientMatched == 1);

    // Read back what the client wrote through its own offset mapping.
    void *readBack = allocShared(context, device, mappedSize);
    memset(readBack, 0, mappedSize);
    gpuCopy(cmdQueue, cmdList, readBack, serverPtr, mappedSize);

    bool serverSeesClientData = true;
    auto *observed = static_cast<uint8_t *>(readBack);
    for (size_t i = 0; i < mappedSize; i++) {
        if (observed[i] != clientByte(i)) {
            std::cerr << "Server mismatch at byte " << std::dec << i
                      << ": observed " << static_cast<uint32_t>(observed[i])
                      << ", expected " << static_cast<uint32_t>(clientByte(i)) << "\n";
            serverSeesClientData = false;
            break;
        }
    }

    shared->serverValid = (clientValidatedServerData && serverSeesClientData) ? 1 : 0;

    SUCCESS_OR_TERMINATE(zeMemFree(context, readBack));
    SUCCESS_OR_TERMINATE(zeMemFree(context, staging));
    SUCCESS_OR_TERMINATE(zeVirtualMemUnmap(context, serverPtr, mappedSize));
    SUCCESS_OR_TERMINATE(zeVirtualMemFree(context, serverPtr, mappedSize));
    SUCCESS_OR_TERMINATE(zePhysicalMemDestroy(context, physMem));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

bool runRound(bool hostMemory) {
    // Flush any buffered parent output before forking so the children do not
    // inherit and re-flush it on exit (which would duplicate previous lines).
    std::cout << std::flush;

    shared->serverReady = 0;
    shared->clientDone = 0;
    shared->clientMatched = 0;
    shared->serverValid = 0;

    // Fork both participants from a process that has NOT initialized Level Zero:
    // the GPU driver state (device fds, GPU virtual address space, contexts) must
    // not be inherited across the fork, so each process initializes Level Zero
    // independently.
    pid_t serverPid = fork();
    if (serverPid < 0) {
        perror("fork");
        exit(1);
    } else if (serverPid == 0) {
        runServer(hostMemory);
        exit(0);
    }

    pid_t clientPid = fork();
    if (clientPid < 0) {
        perror("fork");
        exit(1);
    } else if (clientPid == 0) {
        runClient(hostMemory);
        exit(0);
    }

    int serverStatus = 0;
    int clientStatus = 0;
    waitpid(serverPid, &serverStatus, 0);
    waitpid(clientPid, &clientStatus, 0);

    const bool childrenExitedCleanly =
        WIFEXITED(serverStatus) && WEXITSTATUS(serverStatus) == 0 &&
        WIFEXITED(clientStatus) && WEXITSTATUS(clientStatus) == 0;

    return childrenExitedCleanly && (shared->serverValid == 1);
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello IPC Reserved Physical Offset";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);

    // Shared-memory control block, mapped before fork so all processes share it.
    shared = static_cast<SharedControl *>(mmap(nullptr, sizeof(SharedControl), PROT_READ | PROT_WRITE,
                                               MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (shared == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    const bool deviceRoundValid = runRound(false);
    std::cout << "Reserved physical DEVICE memory offset round: "
              << (deviceRoundValid ? "PASSED" : "FAILED") << "\n";

    const bool hostRoundValid = runRound(true);
    std::cout << "Reserved physical HOST memory offset round: "
              << (hostRoundValid ? "PASSED" : "FAILED") << "\n";

    munmap(shared, sizeof(SharedControl));

    const bool outputValidationSuccessful = deviceRoundValid && hostRoundValid;
    LevelZeroBlackBoxTests::printResult(false, outputValidationSuccessful, blackBoxName);
    return outputValidationSuccessful ? 0 : 1;
}
