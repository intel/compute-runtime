/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILDPROCESSES 1

int sv[CHILDPROCESSES][2];
extern bool verbose;
bool verbose = false;

size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder

// Helpers to send and receive the IPC handles.
//
// L0 uses a vector of ZE_MAX_IPC_HANDLE_SIZE bytes to send the IPC handle
// char data[ZE_MAX_IPC_HANDLE_SIZE];
// First four bytes (which is the sizeof(int)) of it contain the file descriptor
// associated with the dma-buf,
// Rest is payload to communicate extra info to the other processes.
// For instance, the payload in the event pool's IPC handle contains the
// number of events the pool has.

static int sendmsgForIpcHandle(int socket, char *payload) {
    int fd = 0;
    memcpy(&fd, payload, sizeof(fd));

    char sendBuf[ZE_MAX_IPC_HANDLE_SIZE] = {};
    memcpy(sendBuf, payload + sizeof(int), sizeof(sendBuf) - sizeof(int));

    char cmsgBuf[CMSG_SPACE(ZE_MAX_IPC_HANDLE_SIZE)];

    struct iovec msgBuffer = {};
    msgBuffer.iov_base = sendBuf;
    msgBuffer.iov_len = sizeof(*sendBuf);

    struct msghdr msgHeader = {};
    msgHeader.msg_iov = &msgBuffer;
    msgHeader.msg_iovlen = 1;
    msgHeader.msg_control = cmsgBuf;
    msgHeader.msg_controllen = CMSG_LEN(sizeof(fd));

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    controlHeader->cmsg_type = SCM_RIGHTS;
    controlHeader->cmsg_level = SOL_SOCKET;

    controlHeader->cmsg_len = CMSG_LEN(sizeof(fd));
    *(int *)CMSG_DATA(controlHeader) = fd;

    ssize_t bytesSent = sendmsg(socket, &msgHeader, 0);
    if (bytesSent < 0) {
        std::cerr << "Error on sendmsgForIpcHandle " << strerror(errno) << "\n";
        return -1;
    }

    return 0;
}

static int recvmsgForIpcHandle(int socket, char *payload) {
    int fd = -1;
    char recvBuf[ZE_MAX_IPC_HANDLE_SIZE] = {};
    char cmsgBuf[CMSG_SPACE(ZE_MAX_IPC_HANDLE_SIZE)];

    struct iovec msgBuffer;
    msgBuffer.iov_base = recvBuf;
    msgBuffer.iov_len = sizeof(recvBuf);

    struct msghdr msgHeader = {};
    msgHeader.msg_iov = &msgBuffer;
    msgHeader.msg_iovlen = 1;
    msgHeader.msg_control = cmsgBuf;
    msgHeader.msg_controllen = CMSG_LEN(sizeof(fd));

    ssize_t bytesSent = recvmsg(socket, &msgHeader, 0);
    if (bytesSent < 0) {
        std::cerr << "Error on recvmsgForIpcHandle " << strerror(errno) << "\n";
        return -1;
    }

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    memcpy(&fd, CMSG_DATA(controlHeader), sizeof(int));

    memcpy(payload, &fd, sizeof(fd));
    memcpy(payload + sizeof(int), recvBuf, sizeof(recvBuf) - sizeof(int));

    return 0;
}

inline void initializeProcess(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList) {
    SUCCESS_OR_TERMINATE(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    // Retrieve driver
    uint32_t driverCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, nullptr));

    ze_driver_handle_t driverHandle;
    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, &driverHandle));

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

void run_client(int commSocket, uint32_t clientId) {
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

    // receieve the IPC handle for the memory from the other process
    ze_ipc_mem_handle_t pIpcHandle = {};
    int ret = recvmsgForIpcHandle(commSocket, pIpcHandle.data);
    if (ret < 0) {
        std::cerr << "Failing to get IPC memory handle from server\n";
        std::terminate();
    }

    // get the allocation associated with the IPC handle
    void *zeIpcBuffer;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, pIpcHandle,
                                            0u, &zeIpcBuffer));

    // receieve the IPC handle for the event pool from the other process
    ze_ipc_event_pool_handle_t pIpcEventPoolHandle = {};
    ret = recvmsgForIpcHandle(commSocket, pIpcEventPoolHandle.data);
    if (ret < 0) {
        std::cerr << "Failing to get IPC event pool handle from server\n";
        std::terminate();
    }

    // get the event pool associated with the IPC handle
    ze_event_pool_handle_t eventPool = {};
    SUCCESS_OR_TERMINATE(zeEventPoolOpenIpcHandle(context, pIpcEventPoolHandle, &eventPool));

    // get the number of events from the payload
    uint32_t numEvents = 0;
    memcpy(&numEvents, pIpcEventPoolHandle.data + sizeof(int), sizeof(int));

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
            std::cerr << "Event reset in clinent failed\n";
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

void run_server(bool &validRet) {
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
    eventPoolDesc.flags = {};
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
        ze_ipc_mem_handle_t pIpcHandle;
        SUCCESS_OR_TERMINATE(zeMemGetIpcHandle(context, zeBuffer, &pIpcHandle));

        // Pass the IPC handle to the other process
        int commSocket = sv[i][0];
        int ret = sendmsgForIpcHandle(commSocket, pIpcHandle.data);
        if (ret < 0) {
            std::cerr << "Failing to send IPC memory handle to client\n";
            std::terminate();
        }

        // Get the IPC handle for the event pool
        ze_ipc_event_pool_handle_t pIpcEventPoolHandle;
        SUCCESS_OR_TERMINATE(zeEventPoolGetIpcHandle(eventPool, &pIpcEventPoolHandle));

        // Pass the IPC handle to the other process
        ret = sendmsgForIpcHandle(commSocket, pIpcEventPoolHandle.data);
        if (ret) {
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
    verbose = isVerbose(argc, argv);
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
            run_client(sv[i][1], i);
            close(sv[i][1]);
            exit(0);
        }
    }

    run_server(outputValidationSuccessful);

    std::cout << "\nZello IPC Results validation "
              << (outputValidationSuccessful ? "PASSED" : "FAILED")
              << std::endl;

    return (outputValidationSuccessful ? 0 : 1);
}
