/*
 * Copyright (C) 2020-2025 Intel Corporation
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

bool useCopyEngine = false;

uint8_t uinitializedPattern = 1;
uint8_t expectedPattern = 7;
size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder

inline void initializeProcess(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList,
                              ze_command_queue_handle_t &cmdQueueCopy,
                              ze_command_list_handle_t &cmdListCopy,
                              bool isServer) {
    // Retrieve driver
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.flags = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;

    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, nullptr, &desc));

    ze_driver_handle_t driverHandle;

    SUCCESS_OR_TERMINATE(zeInitDrivers(&driverCount, &driverHandle, &desc));

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

    if (isServer) {
        device = devices[0];
        std::cout << "Server using device 0\n";
    } else {
        if (deviceCount > 1) {
            device = devices[1];
            std::cout << "Client using device 1\n";
        } else {
            device = devices[0];
            std::cout << "Client using device 0\n";
        }
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

    uint32_t copyOrdinal = std::numeric_limits<uint32_t>::max();
    if (useCopyEngine) {
        for (uint32_t i = 0; i < numQueueGroups; i++) {
            if ((queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) == 0 &&
                (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY)) {
                copyOrdinal = i;
                break;
            }
        }
    }

    if (copyOrdinal == std::numeric_limits<uint32_t>::max()) {
        std::cout << "Using EUs for copies\n";
        cmdQueueDescCopy.ordinal = cmdQueueDesc.ordinal;
    } else {
        std::cout << "Using copy engines for copies\n";
        cmdQueueDescCopy.ordinal = copyOrdinal;
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

void runClient(int commSocket) {
    std::cout << "Client process " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    ze_command_queue_handle_t cmdQueueCopy;
    ze_command_list_handle_t cmdListCopy;
    initializeProcess(context, device, cmdQueue, cmdList, cmdQueueCopy, cmdListCopy, false);

    void *zeBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&expectedPattern),
                                                       sizeof(expectedPattern), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // get the dma_buf from the other process
    ze_ipc_mem_handle_t pIpcHandle = {};
    int dmaBufFd = recvmsgForIpcHandle(commSocket, pIpcHandle.data);
    if (dmaBufFd < 0) {
        std::cerr << "Failing to get IPC memory handle from server\n";
        std::terminate();
    }
    memcpy(pIpcHandle.data, &dmaBufFd, sizeof(dmaBufFd));

    // get a memory pointer to the BO associated with the dma_buf
    void *zeIpcBuffer;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, pIpcHandle, 0u, &zeIpcBuffer));

    // Copy from client to server
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, zeIpcBuffer, zeBuffer, allocSize, nullptr, 0, nullptr));
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

void runServer(int commSocket, bool &validRet) {
    std::cout << "Server process " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    ze_command_queue_handle_t cmdQueueCopy;
    ze_command_list_handle_t cmdListCopy;
    initializeProcess(context, device, cmdQueue, cmdList, cmdQueueCopy, cmdListCopy, true);

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
    int dmaBufFd;
    memcpy(static_cast<void *>(&dmaBufFd), pIpcHandle.data, sizeof(dmaBufFd));
    if (sendmsgForIpcHandle(commSocket, dmaBufFd, pIpcHandle.data) < 0) {
        std::cerr << "Failing to send IPC handle to client\n";
        std::terminate();
    }

    char *heapBuffer = new char[allocSize];
    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = expectedPattern;
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
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &validateBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, validateBuffer, reinterpret_cast<void *>(&uinitializedPattern),
                                                       sizeof(uinitializedPattern), allocSize, nullptr, 0, nullptr));

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
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueueCopy));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello IPC P2P";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool outputValidationSuccessful;

    useCopyEngine = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-c", "--copyengine");

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
        runClient(sv[1]);
        close(sv[1]);
        exit(0);
    } else {
        close(sv[1]);
        runServer(sv[0], outputValidationSuccessful);
        close(sv[0]);
    }

    LevelZeroBlackBoxTests::printResult(false, outputValidationSuccessful, blackBoxName);
    return outputValidationSuccessful ? 0 : 1;
}
