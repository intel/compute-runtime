/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

extern bool verbose;

template <bool TerminateOnFailure, typename ResulT>
inline void validate(ResulT result, const char *message) {
    if (result == ZE_RESULT_SUCCESS) {
        if (verbose) {
            std::cerr << "SUCCESS : " << message << std::endl;
        }
        return;
    }

    if (verbose) {
        std::cerr << (TerminateOnFailure ? "ERROR : " : "WARNING : ") << message << " : " << result
                  << std::endl;
    }

    if (TerminateOnFailure) {
        std::terminate();
    }
}

#define SUCCESS_OR_TERMINATE(CALL) validate<true>(CALL, #CALL)
#define SUCCESS_OR_TERMINATE_BOOL(FLAG) validate<true>(!(FLAG), #FLAG)
#define SUCCESS_OR_WARNING(CALL) validate<false>(CALL, #CALL)
#define SUCCESS_OR_WARNING_BOOL(FLAG) validate<false>(!(FLAG), #FLAG)

inline bool isParamEnabled(int argc, char *argv[], const char *shortName, const char *longName) {
    char **arg = &argv[1];
    char **argE = &argv[argc];

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            return true;
        }
    }

    return false;
}

inline int getParamValue(int argc, char *argv[], const char *shortName, const char *longName, int defaultValue) {
    char **arg = &argv[1];
    char **argE = &argv[argc];

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            arg++;
            return atoi(*arg);
        }
    }

    return defaultValue;
}

inline bool isCircularDepTest(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-c", "--circular");
    if (enabled == false) {
        return false;
    }

    std::cerr << "Circular Dependency Test mode detected" << std::endl;

    return true;
}

inline bool isVerbose(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-v", "--verbose");
    if (enabled == false) {
        return false;
    }

    std::cerr << "Verbose mode detected" << std::endl;

    return true;
}

inline bool isSyncQueueEnabled(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-s", "--sync");
    if (enabled == false) {
        std::cerr << "Async Queue detected" << std::endl;
        return false;
    }

    std::cerr << "Sync Queue detected" << std::endl;

    return true;
}

uint32_t getCommandQueueOrdinal(ze_device_handle_t &device) {
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cout << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                queueProperties.data()));
    uint32_t computeQueueGroupOrdinal = numQueueGroups;
    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            computeQueueGroupOrdinal = i;
            break;
        }
    }
    return computeQueueGroupOrdinal;
}

int32_t getCopyOnlyCommandQueueOrdinal(ze_device_handle_t &device) {
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cout << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                queueProperties.data()));
    int32_t copyOnlyQueueGroupOrdinal = -1;
    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (!(queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) && (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY)) {
            copyOnlyQueueGroupOrdinal = i;
            break;
        }
    }
    return copyOnlyQueueGroupOrdinal;
}

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device, uint32_t *ordinal) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t descriptor = {};
    descriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;

    descriptor.pNext = nullptr;
    descriptor.flags = 0;
    descriptor.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    descriptor.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    descriptor.ordinal = getCommandQueueOrdinal(device);
    descriptor.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &descriptor, &cmdQueue));
    if (ordinal != nullptr) {
        *ordinal = descriptor.ordinal;
    }
    return cmdQueue;
}

ze_result_t createCommandList(ze_context_handle_t &context, ze_device_handle_t &device, ze_command_list_handle_t &cmdList) {
    ze_command_list_desc_t descriptor = {};
    descriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;

    descriptor.pNext = nullptr;
    descriptor.flags = 0;
    descriptor.commandQueueGroupOrdinal = getCommandQueueOrdinal(device);

    return zeCommandListCreate(context, device, &descriptor, &cmdList);
}

void createEventPoolAndEvents(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_event_pool_handle_t &eventPool,
                              ze_event_pool_flag_t poolFlag,
                              uint32_t poolSize,
                              ze_event_handle_t *events,
                              ze_event_scope_flag_t signalScope,
                              ze_event_scope_flag_t waitScope) {
    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventPoolDesc.count = poolSize;
    eventPoolDesc.flags = poolFlag;
    SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));

    for (uint32_t i = 0; i < poolSize; i++) {
        eventDesc.index = i;
        eventDesc.signal = signalScope;
        eventDesc.wait = waitScope;
        SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, events + i));
    }
}

std::vector<ze_device_handle_t> zelloGetSubDevices(ze_device_handle_t &device, int &subDevCount) {
    uint32_t deviceCount = 0;
    std::vector<ze_device_handle_t> subdevs(deviceCount, nullptr);
    SUCCESS_OR_TERMINATE(zeDeviceGetSubDevices(device, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cout << "No sub device found!\n";
        subDevCount = 0;
        return subdevs;
    }
    subDevCount = deviceCount;
    subdevs.resize(deviceCount);
    SUCCESS_OR_TERMINATE(zeDeviceGetSubDevices(device, &deviceCount, subdevs.data()));
    return subdevs;
}

std::vector<ze_device_handle_t> zelloInitContextAndGetDevices(ze_context_handle_t &context, ze_driver_handle_t &driverHandle) {
    SUCCESS_OR_TERMINATE(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    uint32_t driverCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, nullptr));
    if (driverCount == 0) {
        std::cout << "No driver handle found!\n";
        std::terminate();
    }

    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, &driverHandle));
    ze_context_desc_t context_desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    SUCCESS_OR_TERMINATE(zeContextCreate(driverHandle, &context_desc, &context));

    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cout << "No device found!\n";
        std::terminate();
    }
    std::vector<ze_device_handle_t> devices(deviceCount, nullptr);
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, devices.data()));
    return devices;
}

std::vector<ze_device_handle_t> zelloInitContextAndGetDevices(ze_context_handle_t &context) {
    ze_driver_handle_t driverHandle;
    return zelloInitContextAndGetDevices(context, driverHandle);
}

void initialize(ze_driver_handle_t &driver, ze_context_handle_t &context, ze_device_handle_t &device, ze_command_queue_handle_t &cmdQueue, uint32_t &ordinal) {
    std::vector<ze_device_handle_t> devices;

    devices = zelloInitContextAndGetDevices(context, driver);
    device = devices[0];
    cmdQueue = createCommandQueue(context, device, &ordinal);
}

static inline void teardown(ze_context_handle_t context, ze_command_queue_handle_t cmdQueue) {
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}
