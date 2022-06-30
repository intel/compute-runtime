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

extern bool verbose;
bool verbose = false;

uint8_t uinitializedPattern = 1;
uint8_t expectedPattern = 7;
size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder
uint32_t serverDevice = 0;
uint32_t clientDevice = 1;

static int sendmsg_fd(int socket, int fd, char *payload, size_t payloadLen) {
    char sendBuf[sizeof(ze_ipc_mem_handle_t)] = {};
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
    msgBuffer.iov_base = payload;
    msgBuffer.iov_len = payloadLen;

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

static int recvmsg_fd(int socket, char *payload, size_t payloadLen) {
    int fd = -1;
    char recvBuf[sizeof(ze_ipc_mem_handle_t)] = {};
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
    msgBuffer.iov_base = payload;
    msgBuffer.iov_len = payloadLen;

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
    memmove(&fd, CMSG_DATA(controlHeader), sizeof(int));
    return fd;
}

inline void initializeProcess(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList,
                              ze_command_queue_handle_t &cmdQueueCopy,
                              ze_command_list_handle_t &cmdListCopy,
                              bool isServer) {
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
    std::cout << "Number of devices found: " << deviceCount << "\n";

    std::vector<ze_device_handle_t> devices(deviceCount);
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, devices.data()));

    // Make the server use device0 and the client device1 if available
    if (deviceCount > 1) {
        ze_bool_t canAccessPeer = false;
        SUCCESS_OR_TERMINATE(zeDeviceCanAccessPeer(devices[0], devices[1], &canAccessPeer));
        if (canAccessPeer == false) {
            std::cerr << "Two devices found but no P2P capabilities detected\n";
            std::terminate();
        } else {
            std::cerr << "Two devices found and P2P capabilities detected\n";
        }
    }

    if (deviceCount == 1) {
        serverDevice = clientDevice = 0;
    }

    if (isServer == false) {
        device = devices[clientDevice];
        std::cout << "Client using device " << clientDevice << "\n";
    } else {
        device = devices[serverDevice];
        std::cout << "Server using device " << serverDevice << "\n";
    }

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
    ze_command_queue_desc_t cmdQueueDescCopy = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
            break;
        }
    }

    std::cout << (isServer ? "Server " : "Client ") << " using queues "
              << cmdQueueDescCopy.ordinal << " and " << cmdQueueDesc.ordinal << "\n";

    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    cmdQueueDescCopy.index = 0;
    cmdQueueDescCopy.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDescCopy, &cmdQueueCopy));

    // Create command list
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_command_list_desc_t cmdListDescCopy = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDescCopy.commandQueueGroupOrdinal = cmdQueueDescCopy.ordinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDescCopy, &cmdListCopy));
}

void run_client(int commSocket) {
    std::cout << "Client process " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    ze_command_queue_handle_t cmdQueueCopy;
    ze_command_list_handle_t cmdListCopy;
    initializeProcess(context, device, cmdQueue, cmdList, cmdQueueCopy, cmdListCopy, false);

    // receieve the IPC handle for the event pool from the other process
    ze_ipc_event_pool_handle_t pIpcEventPoolHandle = {};
    int dma_buf_fd = recvmsg_fd(commSocket, pIpcEventPoolHandle.data, ZE_MAX_IPC_HANDLE_SIZE);
    if (dma_buf_fd < 0) {
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

    void *zeBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&expectedPattern),
                                                       sizeof(expectedPattern), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // get the dma_buf from the other process
    ze_ipc_mem_handle_t pIpcHandle;
    dma_buf_fd = recvmsg_fd(commSocket, pIpcHandle.data, ZE_MAX_IPC_HANDLE_SIZE);
    if (dma_buf_fd < 0) {
        std::cerr << "Failing to get dma_buf fd from server\n";
        std::terminate();
    }
    memcpy(&pIpcHandle, static_cast<void *>(&dma_buf_fd), sizeof(dma_buf_fd));

    // get a memory pointer to the BO associated with the dma_buf
    void *zeIpcBuffer;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, pIpcHandle, 0u, &zeIpcBuffer));

    // Copy from client to server
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, zeIpcBuffer, zeBuffer, allocSize, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueueCopy, 1, &cmdListCopy, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueueCopy, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeMemCloseIpcHandle(context, zeIpcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueueCopy));

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

void run_server(int commSocket, bool &validRet) {
    std::cout << "Server process " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    ze_command_queue_handle_t cmdQueueCopy;
    ze_command_list_handle_t cmdListCopy;
    initializeProcess(context, device, cmdQueue, cmdList, cmdQueueCopy, cmdListCopy, true);

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

    // Get the IPC handle for the event pool
    ze_ipc_event_pool_handle_t pIpcEventPoolHandle;
    SUCCESS_OR_TERMINATE(zeEventPoolGetIpcHandle(eventPool, &pIpcEventPoolHandle));

    // Pass the IPC handle to the other process
    int dma_buf_fd;
    memcpy(static_cast<void *>(&dma_buf_fd), &pIpcEventPoolHandle, sizeof(dma_buf_fd));
    if (sendmsg_fd(commSocket, static_cast<int>(dma_buf_fd), pIpcEventPoolHandle.data, ZE_MAX_IPC_HANDLE_SIZE) < 0) {
        std::cerr << "Failing to send IPC event pool handle to client\n";
        std::terminate();
    }

    void *zeBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    // Initialize the IPC buffer
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&uinitializedPattern),
                                                       sizeof(uinitializedPattern), allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));

    // Get a dma_buf for the previously allocated pointer
    ze_ipc_mem_handle_t pIpcHandle;
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandle(context, zeBuffer, &pIpcHandle));

    // Pass the dma_buf to the other process
    memcpy(static_cast<void *>(&dma_buf_fd), &pIpcHandle, sizeof(dma_buf_fd));
    if (sendmsg_fd(commSocket, static_cast<int>(dma_buf_fd), pIpcHandle.data, ZE_MAX_IPC_HANDLE_SIZE) < 0) {
        std::cerr << "Failing to send dma_buf fd to client\n";
        std::terminate();
    }

    char *heapBuffer = new char[allocSize];
    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = expectedPattern;
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
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &validateBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, validateBuffer, reinterpret_cast<void *>(&uinitializedPattern),
                                                       sizeof(uinitializedPattern), allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, validateBuffer, zeBuffer, allocSize,
                                                       nullptr, 1, &events[0]));
    //nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate stack and buffers have the original data from heapBuffer
    validRet = (0 == memcmp(heapBuffer, validateBuffer, allocSize));

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueueCopy));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

int main(int argc, char *argv[]) {
    verbose = isVerbose(argc, argv);
    bool outputValidationSuccessful;

    serverDevice = getParamValue(argc, argv, "-s", "--serverdevice", 1);
    clientDevice = getParamValue(argc, argv, "-c", "--clientdevice", 0);

    int sv[2];
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        exit(1);
    }

    int child = fork();
    if (child < 0) {
        perror("fork");
        exit(1);
    } else if (0 == child) {
        close(sv[0]);
        run_client(sv[1]);
        close(sv[1]);
        exit(0);
    } else {
        close(sv[1]);
        run_server(sv[0], outputValidationSuccessful);
        close(sv[0]);
    }

    std::cout << "\nZello IPC P2P With Event Results validation "
              << (outputValidationSuccessful ? "PASSED" : "FAILED")
              << std::endl;

    return 0;
}
