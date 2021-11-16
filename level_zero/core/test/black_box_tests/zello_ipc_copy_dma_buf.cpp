/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILDPROCESSES 4

int sv[CHILDPROCESSES][2];
extern bool verbose;
bool verbose = false;

size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder

static int sendmsg_fd(int socket, int fd) {
    char sendBuf[sizeof(ze_ipc_mem_handle_t)] = {};
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
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
        return -1;
    }

    return 0;
}

static int recvmsg_fd(int socket) {
    int fd = -1;
    char recvBuf[sizeof(ze_ipc_mem_handle_t)] = {};
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

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
        return -1;
    }

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    if (CMSG_DATA(controlHeader) == nullptr) {
        return -1;
    }
    memmove(&fd, CMSG_DATA(controlHeader), sizeof(int));
    return fd;
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

    // get the dma_buf from the other process
    int dma_buf_fd = recvmsg_fd(commSocket);
    if (dma_buf_fd < 0) {
        std::cerr << "Failing to get dma_buf fd from server\n";
        std::terminate();
    }
    ze_ipc_mem_handle_t pIpcHandle;
    memcpy(&pIpcHandle, static_cast<void *>(&dma_buf_fd), sizeof(dma_buf_fd));

    // get a memory pointer to the BO associated with the dma_buf
    void *zeIpcBuffer;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, pIpcHandle,
                                            0u, &zeIpcBuffer));
    // Copy from heap to IPC buffer memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeIpcBuffer, heapBuffer, allocSize,
                                                       nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

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

    for (uint32_t i = 0; i < CHILDPROCESSES; i++) {
        // Initialize the IPC buffer
        int value = 3;
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&value),
                                                           sizeof(value), allocSize, nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));

        // Get a dma_buf for the previously allocated pointer
        ze_ipc_mem_handle_t pIpcHandle;
        SUCCESS_OR_TERMINATE(zeMemGetIpcHandle(context, zeBuffer, &pIpcHandle));

        // Pass the dma_buf to the other process
        int dma_buf_fd;
        memcpy(static_cast<void *>(&dma_buf_fd), &pIpcHandle, sizeof(dma_buf_fd));
        int commSocket = sv[i][0];
        if (sendmsg_fd(commSocket, static_cast<int>(dma_buf_fd)) < 0) {
            std::cerr << "Failing to send dma_buf fd to client\n";
            std::terminate();
        }

        char *heapBuffer = new char[allocSize];
        for (size_t i = 0; i < allocSize; ++i) {
            heapBuffer[i] = static_cast<char>(i + 1);
        }

        // Wait for child to exit
        int child_status;
        pid_t clientPId = wait(&child_status);
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
    verbose = isVerbose(argc, argv);
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
            run_client(sv[i][1], i);
            close(sv[i][1]);
            exit(0);
        }
    }

    run_server(outputValidationSuccessful);

    std::cout << "\nZello IPC Results validation "
              << (outputValidationSuccessful ? "PASSED" : "FAILED")
              << std::endl;

    return 0;
}
