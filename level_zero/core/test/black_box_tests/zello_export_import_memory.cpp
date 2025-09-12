/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_ipc_common.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILDPROCESSES 4

int sv[CHILDPROCESSES][2];

size_t allocSize = 4194304;

inline void initializeProcess(ze_driver_handle_t &driverHandle,
                              ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList) {
    // Retrieve driver
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.flags = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;

    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, nullptr, &desc));

    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, &driverHandle, &desc));

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    SUCCESS_OR_TERMINATE(zeContextCreate(driverHandle, &contextDesc, &context));

    // Retrieve device
    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, nullptr));

    deviceCount = 1;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, &device));

    // Print some properties
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));

    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    // Create command queue
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

    // Create command list
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));
}

void runClient(int commSocket, uint32_t clientId) {
    std::cout << "Client " << clientId << ", process ID: " << std::dec << getpid() << "\n";

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(driverHandle, context, device, cmdQueue, cmdList);

    char *heapBuffer = new char[allocSize];
    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }

    // receive the IPC handle for the memory from the other process
    char payload[ZE_MAX_IPC_HANDLE_SIZE];
    int handle = recvmsgForIpcHandle(commSocket, payload);
    if (handle < 0) {
        std::cerr << "Failing to get IPC memory handle from server\n";
        std::terminate();
    }

    // get a memory pointer to the BO associated with the dma_buf handle
    ze_external_memory_import_fd_t importDesc = {};
    importDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    importDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    importDesc.fd = handle;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = &importDesc;

    void *zeIpcBuffer;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, 0, 0, device, &zeIpcBuffer));

    // get the size of the imported allocation
    void *ipcPtr = nullptr;
    size_t ipcSize = 0;
    SUCCESS_OR_TERMINATE(zeMemGetAddressRange(context, zeIpcBuffer, &ipcPtr, &ipcSize));

    // allocation may be bigger than the size requested originally by the exporter in case
    // driver has made some alignment adjustments. In this case, allocSize = 4MB, so size
    // should be the same.
    if (ipcSize != allocSize) {
        std::cerr << "Incorrect size returned for imported allocation\n";
        std::terminate();
    }

    // Copy from heap to IPC buffer memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeIpcBuffer, heapBuffer, ipcSize,
                                                       nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeIpcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    delete[] heapBuffer;
}

void runServer(bool &validRet) {
    std::cout << "Server process ID " << std::dec << getpid() << "\n";

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(driverHandle, context, device, cmdQueue, cmdList);

    void *zeBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (uint32_t i = 0; i < CHILDPROCESSES; i++) {
        // Initialize the IPC buffer
        int value = 3;
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&value),
                                                           sizeof(value), allocSize, nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));

        // get the fd handle
        ze_external_memory_export_fd_t exportFd = {};
        exportFd.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
        exportFd.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

        ze_memory_allocation_properties_t allocProperties = {};
        allocProperties.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
        allocProperties.pNext = &exportFd;

        SUCCESS_OR_TERMINATE(zeMemGetAllocProperties(context, zeBuffer, &allocProperties, nullptr));

        // transmit handle to the client
        int commSocket = sv[i][0];
        char payload[ZE_MAX_IPC_HANDLE_SIZE];
        if (sendmsgForIpcHandle(commSocket, static_cast<int>(exportFd.fd), payload) < 0) {
            std::cerr << "Failing to send IPC event pool handle to client\n";
            std::terminate();
        }

        char *heapBuffer = new char[allocSize];
        for (size_t i = 0; i < allocSize; ++i) {
            heapBuffer[i] = static_cast<char>(i + 1);
        }

        // Wait for child to exit
        int childStatus;
        pid_t clientPId = wait(&childStatus);
        if (clientPId <= 0) {
            std::cerr << "Client terminated abruptly with error code " << strerror(errno) << "\n";
            std::terminate();
        }

        void *validateBuffer = nullptr;
        ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                              allocSize, 1, device, &validateBuffer));

        value = 5;
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, validateBuffer, reinterpret_cast<void *>(&value),
                                                           sizeof(value), allocSize, nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

        // Copy from device-allocated memory
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, validateBuffer, zeBuffer, allocSize,
                                                           nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

        char *ipcBuffer = static_cast<char *>(validateBuffer);
        for (uint32_t h = 0; h < allocSize; h++) {
            if (static_cast<uint32_t>(ipcBuffer[h]) != static_cast<uint32_t>(heapBuffer[h])) {
                printf("Error: ipcBuffer %d %d, heapBuffer %d\n", h, static_cast<uint32_t>(ipcBuffer[h]), heapBuffer[h]);
                break;
            }
        }

        // Validate stack and buffers have the original data from heapBuffer
        validRet = (0 == memcmp(heapBuffer, validateBuffer, allocSize));
        delete[] heapBuffer;
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Export Import";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool outputValidationSuccessful;

    for (uint32_t i = 0; i < CHILDPROCESSES; i++) {
        if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv[i]) < 0) {
            perror("socketpair");
            exit(1);
        }
    }

    pid_t childPids[CHILDPROCESSES];
    for (uint32_t i = 0; i < CHILDPROCESSES; i++) {
        childPids[i] = fork();
        if (childPids[i] < 0) {
            perror("fork");
            exit(1);
        } else if (childPids[i] == 0) {
            close(sv[i][0]);
            runClient(sv[i][1], i);
            close(sv[i][1]);
            exit(0);
        }
    }

    runServer(outputValidationSuccessful);

    LevelZeroBlackBoxTests::printResult(false, outputValidationSuccessful, blackBoxName);
    return outputValidationSuccessful ? 0 : 1;
}
