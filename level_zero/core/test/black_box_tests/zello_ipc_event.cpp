/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_ipc_common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILDPROCESSES 1

int sv[CHILDPROCESSES][2];

size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder

// L0 uses a vector of ZE_MAX_IPC_HANDLE_SIZE bytes to send the IPC handle
// char data[ZE_MAX_IPC_HANDLE_SIZE];
// First four bytes (which is the sizeof(int)) of it contain the file descriptor
// associated with the dma-buf,
// Rest is payload to communicate extra info to the other processes.
// For instance, the payload in the event pool's IPC handle contains the
// number of events the pool has.

void initializeProcess(ze_context_handle_t &context,
                       ze_device_handle_t &device,
                       ze_command_queue_handle_t &cmdQueue,
                       ze_command_list_handle_t &cmdList) {
    auto devices = zelloInitContextAndGetDevices(context);
    device = devices[0];

    // Print some properties
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    // Create command queue
    cmdQueue = createCommandQueue(context, device, nullptr, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, false);
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList, false));
}

void runClient(int commSocket, uint32_t clientId) {
    std::cout << "Client " << clientId << ", process ID: " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(context, device, cmdQueue, cmdList);

    char *heapBuffer = new char[allocSize];
    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }

    // receive the IPC handle for the memory from the other process
    ze_ipc_mem_handle_t pIpcHandle = {};
    int dmaBufFd = recvmsgForIpcHandle(commSocket, pIpcHandle.data);
    if (dmaBufFd < 0) {
        std::cerr << "Failing to get IPC memory handle from server\n";
        std::terminate();
    }
    memcpy(pIpcHandle.data, &dmaBufFd, sizeof(dmaBufFd));

    // get the allocation associated with the IPC handle
    void *zeIpcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, pIpcHandle,
                                            0u, &zeIpcBuffer));

    // receive the IPC handle for the event pool from the other process
    ze_ipc_event_pool_handle_t pIpcEventPoolHandle = {};
    dmaBufFd = recvmsgForIpcHandle(commSocket, pIpcEventPoolHandle.data);
    if (dmaBufFd < 0) {
        std::cerr << "Failing to get IPC event pool handle from server\n";
        std::terminate();
    }
    memcpy(pIpcEventPoolHandle.data, &dmaBufFd, sizeof(dmaBufFd));

    // get the event pool associated with the IPC handle
    ze_event_pool_handle_t eventPool = {};
    SUCCESS_OR_TERMINATE(zeEventPoolOpenIpcHandle(context, pIpcEventPoolHandle, &eventPool));

    // get the number of events from the payload
    uint32_t numEvents = 0;
    memcpy(&numEvents, pIpcEventPoolHandle.data + sizeof(uint64_t), sizeof(uint64_t));

    std::vector<ze_event_handle_t> events(numEvents);

    uint32_t i = 0;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    for (auto &event : events) {
        eventDesc.index = i++;
        SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, &event));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        ze_result_t eventStatus = zeEventQueryStatus(event);
        if (eventStatus != ZE_RESULT_NOT_READY) {
            std::cerr << "Event reset in client failed\n";
            std::terminate();
        }
    }

    // Copy from heap to IPC buffer memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeIpcBuffer, heapBuffer, allocSize, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    for (auto &event : events) {
        ze_result_t eventStatus = zeEventQueryStatus(event);
        if (eventStatus != ZE_RESULT_SUCCESS) {
            std::cerr << "Event status  in client not correct\n";
            std::terminate();
        }

        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    SUCCESS_OR_TERMINATE(zeEventPoolCloseIpcHandle(eventPool));

    SUCCESS_OR_TERMINATE(zeMemCloseIpcHandle(context, zeIpcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    delete[] heapBuffer;
}

void runServer(bool &validRet) {
    std::cout << "Server process ID " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(context, device, cmdQueue, cmdList);

    void *zeBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    uint32_t numEvents = 2;
    ze_event_pool_handle_t eventPool = {};
    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = numEvents;
    eventPoolDesc.flags = {ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_HOST_VISIBLE};
    SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));

    std::vector<ze_event_handle_t> events(numEvents);

    uint32_t i = 0;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    for (auto &event : events) {
        eventDesc.index = i++;
        SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, &event));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        ze_result_t eventStatus = zeEventQueryStatus(event);
        if (eventStatus != ZE_RESULT_NOT_READY) {
            std::cerr << "Event status in server before starting not correct\n";
            std::terminate();
        }
    }

    for (uint32_t i = 0; i < CHILDPROCESSES; i++) {
        // Initialize the IPC buffer
        char value = 3;
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&value),
                                                           sizeof(value), allocSize, nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));

        // Get the IPC handle for the previously allocated pointer
        ze_ipc_mem_handle_t pIpcHandle{};
        SUCCESS_OR_TERMINATE(zeMemGetIpcHandle(context, zeBuffer, &pIpcHandle));

        // Pass the IPC handle to the other process
        int commSocket = sv[i][0];
        int dmaBufFd;
        memcpy(static_cast<void *>(&dmaBufFd), pIpcHandle.data, sizeof(dmaBufFd));
        if (sendmsgForIpcHandle(commSocket, dmaBufFd, pIpcHandle.data) < 0) {
            std::cerr << "Failing to send IPC handle to client\n";
            std::terminate();
        }

        // Get the IPC handle for the event pool
        ze_ipc_event_pool_handle_t pIpcEventPoolHandle{};
        SUCCESS_OR_TERMINATE(zeEventPoolGetIpcHandle(eventPool, &pIpcEventPoolHandle));

        // Pass the IPC handle to the other process
        memcpy(static_cast<void *>(&dmaBufFd), &pIpcEventPoolHandle, sizeof(dmaBufFd));
        if (sendmsgForIpcHandle(commSocket, dmaBufFd, pIpcEventPoolHandle.data) < 0) {
            std::cerr << "Failing to send IPC event pool handle to client\n";
            std::terminate();
        }

        // Initialize buffer
        char *heapBuffer = new char[allocSize];
        for (size_t i = 0; i < allocSize; ++i) {
            heapBuffer[i] = static_cast<char>(i + 1);
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
        SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &events[0]));
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, validateBuffer, zeBuffer, allocSize, events[1], 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

        // Validate stack and buffers have the original data from heapBuffer
        validRet = (0 == memcmp(heapBuffer, validateBuffer, allocSize));
        char *valBuffer = (char *)validateBuffer;
        if (verbose) {
            for (uint32_t i = 0; i < allocSize; i++) {
                printf("valBuffer[%d] = %d heapBuffer[%d] = %d\n", i, valBuffer[i], i, heapBuffer[i]);
            }
        }
        delete[] heapBuffer;
    }

    for (auto &event : events) {
        ze_result_t eventStatus = zeEventQueryStatus(event);
        if (eventStatus != ZE_RESULT_SUCCESS) {
            std::cerr << "Event status in server after finishing not correct\n";
            std::terminate();
        }

        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello IPC Event";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool outputValidationSuccessful;

    if (verbose) {
        allocSize = 16;
    }

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
    return (outputValidationSuccessful ? 0 : 1);
}
