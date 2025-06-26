/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <bitset>
#include <cstring>
#include <iomanip>
#include <map>

#ifdef _WIN64
#include <windows.h>
#else
#include <stdlib.h>
#endif

namespace LevelZeroBlackBoxTests {

struct LoadedDriverExtensions {
    std::vector<ze_driver_extension_properties_t> extensions;
    bool loaded = false;
};

static LoadedDriverExtensions driverExtensions;

using LoadedDeviceQueueProperties = std::map<ze_device_handle_t, std::vector<ze_command_queue_group_properties_t>>;
static LoadedDeviceQueueProperties deviceQueueProperties;

std::vector<ze_command_queue_group_properties_t> &getDeviceQueueProperties(ze_device_handle_t device) {
    auto &queueProperties = deviceQueueProperties[device];
    if (queueProperties.size() == 0) {
        uint32_t numQueueGroups = 0;
        SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
        if (numQueueGroups == 0) {
            std::cerr << "No queue groups found!\n";
            std::terminate();
        }
        queueProperties.resize(numQueueGroups);
        SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                    queueProperties.data()));
    }
    return queueProperties;
}

bool verbose;
uint32_t overrideErrorMax = 0;

bool isParamEnabled(int argc, char *argv[], const char *shortName, const char *longName) {
    char **arg = &argv[1];
    char **argE = &argv[argc];

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            return true;
        }
    }

    return false;
}

int getParamValue(int argc, char *argv[], const char *shortName, const char *longName, int defaultValue) {
    char **arg = &argv[1];
    char **argE = &argv[argc];
    char *end = nullptr;
    int base = 0;

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            arg++;
            if (arg == argE) {
                break;
            }
            return static_cast<int>(strtol(*arg, &end, base));
        }
    }

    return defaultValue;
}

uint32_t getParamValue(int argc, char *argv[], const char *shortName, const char *longName, uint32_t defaultValue) {
    char **arg = &argv[1];
    char **argE = &argv[argc];
    char *end = nullptr;
    int base = 0;

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            arg++;
            if (arg == argE) {
                break;
            }
            return static_cast<uint32_t>(strtoul(*arg, &end, base));
        }
    }

    return defaultValue;
}

const char *getParamValue(int argc, char *argv[], const char *shortName, const char *longName, const char *defaultString) {
    char **arg = &argv[1];
    char **argE = &argv[argc];

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            arg++;
            if (arg == argE) {
                break;
            }
            return *arg;
        }
    }

    return defaultString;
}

bool isCircularDepTest(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-c", "--circular");
    if (enabled == false) {
        return false;
    }

    if (verbose) {
        std::cout << "Circular Dependency Test mode detected" << std::endl;
    }

    return true;
}

bool isVerbose(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-v", "--verbose");
    if (enabled == false) {
        return false;
    }

    std::cout << "Verbose mode detected" << std::endl;

    return true;
}

bool isSyncQueueEnabled(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-s", "--sync");
    if (enabled == false) {
        if (verbose) {
            std::cout << "Async Queue detected" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "Sync Queue detected" << std::endl;
    }

    return true;
}

bool isAsyncQueueEnabled(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-as", "--async");
    if (enabled == false) {
        if (verbose) {
            std::cout << "Sync Queue detected" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "Async Queue detected" << std::endl;
    }

    return true;
}

bool isAubMode(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-a", "--aub");
    if (enabled == false) {
        return false;
    }

    if (verbose) {
        std::cout << "Aub mode detected" << std::endl;
    }

    return true;
}

bool isCommandListShared(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-c", "--cmdlist");
    if (enabled == false) {
        return false;
    }

    if (verbose) {
        std::cout << "Command List shared between tests" << std::endl;
    }

    return true;
}

bool isImmediateFirst(int argc, char *argv[]) {
    bool enabled = isParamEnabled(argc, argv, "-i", "--immediate");

    if (verbose) {
        if (enabled) {
            std::cout << "Immediate Command List executed first" << std::endl;
        } else {
            std::cout << "Regular Command List executed first" << std::endl;
        }
    }

    return enabled;
}

bool getAllocationFlag(int argc, char *argv[], int defaultValue) {
    int value = getParamValue(argc, argv, "-A", "-allocflag", defaultValue);
    if (verbose) {
        std::cout << "Allocation flag ";
        if (value != defaultValue) {
            std::cout << "override ";
        } else {
            std::cout << "default ";
        }
        std::bitset<4> bitValue(value);
        std::cout << "value 0b" << bitValue << std::endl;
    }

    return value;
}

void selectQueueMode(ze_command_queue_desc_t &desc, bool useSync) {
    if (useSync) {
        if (verbose) {
            std::cout << "Choosing Command Queue mode synchronous" << std::endl;
        }
        desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    } else {
        if (verbose) {
            std::cout << "Choosing Command Queue mode asynchronous" << std::endl;
        }
        desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    }
}

uint32_t getBufferLength(int argc, char *argv[], uint32_t defaultLength) {
    uint32_t length = getParamValue(argc, argv, "-l", "--length", defaultLength);
    if (length == defaultLength) {
        return defaultLength;
    }

    if (verbose) {
        std::cout << "Buffer length detected = " << length << std::endl;
    }

    return length;
}

void getErrorMax(int argc, char *argv[]) {
    overrideErrorMax = getParamValue(argc, argv, "-em", "--errorMax", 0);
}

void printResult(bool aubMode, bool outputValidationSuccessful, const std::string &blackBoxName, const std::string &currentTest) {
    std::cout << std::endl
              << blackBoxName;
    if (!currentTest.empty()) {
        std::cout << " " << currentTest;
    }

    if (aubMode == false) {
        std::cout << " Results validation "
                  << (outputValidationSuccessful ? "PASSED" : "FAILED")
                  << std::endl
                  << std::endl;
    } else {
        std::cout << " Results validation disabled in aub mode."
                  << std::endl
                  << std::endl;
    }
}

void printResult(bool aubMode, bool outputValidationSuccessful, const std::string &blackBoxName) {
    std::string currentTest{};
    printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
}

uint32_t getCommandQueueOrdinal(ze_device_handle_t &device, bool useCooperativeFlag) {
    std::vector<ze_command_queue_group_properties_t> &queueProperties = getDeviceQueueProperties(device);

    ze_command_queue_group_property_flags_t computeFlags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE;
    if (useCooperativeFlag) {
        computeFlags |= ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
    }

    uint32_t computeQueueGroupOrdinal = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < queueProperties.size(); i++) {
        if (queueProperties[i].flags & computeFlags) {
            computeQueueGroupOrdinal = i;
            break;
        }
    }
    return computeQueueGroupOrdinal;
}

std::vector<uint32_t> getComputeQueueOrdinals(ze_device_handle_t &device) {
    std::vector<ze_command_queue_group_properties_t> &queueProperties = getDeviceQueueProperties(device);

    std::vector<uint32_t> ordinals;
    for (uint32_t i = 0; i < queueProperties.size(); i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            ordinals.push_back(i);
        }
    }
    return ordinals;
}

uint32_t getCopyOnlyCommandQueueOrdinal(ze_device_handle_t &device) {
    std::vector<ze_command_queue_group_properties_t> &queueProperties = getDeviceQueueProperties(device);

    uint32_t copyOnlyQueueGroupOrdinal = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < queueProperties.size(); i++) {
        if (!(queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) && (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY)) {
            copyOnlyQueueGroupOrdinal = i;
            break;
        }
    }
    return copyOnlyQueueGroupOrdinal;
}

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device,
                                             uint32_t *ordinal, ze_command_queue_mode_t mode,
                                             ze_command_queue_priority_t priority, bool useCooperativeFlag) {

    uint32_t queueOrdinal = getCommandQueueOrdinal(device, useCooperativeFlag);
    auto cmdQueue = createCommandQueueWithOrdinal(context, device, queueOrdinal, mode, priority);
    if (ordinal != nullptr) {
        *ordinal = queueOrdinal;
    }
    return cmdQueue;
}

ze_command_queue_handle_t createCommandQueueWithOrdinal(ze_context_handle_t &context, ze_device_handle_t &device,
                                                        uint32_t ordinal, ze_command_queue_mode_t mode,
                                                        ze_command_queue_priority_t priority) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t descriptor = {};
    descriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;

    descriptor.pNext = nullptr;
    descriptor.flags = 0;
    descriptor.mode = mode;
    descriptor.priority = priority;

    descriptor.ordinal = ordinal;
    descriptor.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &descriptor, &cmdQueue));

    return cmdQueue;
}

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device, uint32_t *ordinal, bool useCooperativeFlag) {
    return createCommandQueue(context, device, ordinal, ZE_COMMAND_QUEUE_MODE_DEFAULT, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, useCooperativeFlag);
}

ze_result_t createCommandList(ze_context_handle_t &context, ze_device_handle_t &device, ze_command_list_handle_t &cmdList, uint32_t ordinal) {
    ze_command_list_desc_t descriptor = {};
    descriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;

    descriptor.pNext = nullptr;
    descriptor.flags = 0;
    descriptor.commandQueueGroupOrdinal = ordinal;

    return zeCommandListCreate(context, device, &descriptor, &cmdList);
}

ze_result_t createCommandList(ze_context_handle_t &context, ze_device_handle_t &device, ze_command_list_handle_t &cmdList, bool useCooperativeFlag) {
    return createCommandList(context, device, cmdList, getCommandQueueOrdinal(device, useCooperativeFlag));
}

void createEventPoolAndEvents(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_event_pool_handle_t &eventPool,
                              ze_event_pool_flags_t poolFlag,
                              bool counterEvents,
                              const zex_counter_based_event_desc_t *counterBasedDesc,
                              pfnZexCounterBasedEventCreate2 zexCounterBasedEventCreate2Func,
                              uint32_t poolSize,
                              ze_event_handle_t *events,
                              ze_event_scope_flags_t signalScope,
                              ze_event_scope_flags_t waitScope) {

    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = poolSize;
    eventPoolDesc.flags = poolFlag;

    if (!counterEvents) {
        SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));
    }

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    for (uint32_t i = 0; i < poolSize; i++) {
        if (counterEvents) {
            SUCCESS_OR_TERMINATE(zexCounterBasedEventCreate2Func(context, device, counterBasedDesc, events + i));
        } else {
            eventDesc.index = i;
            eventDesc.signal = signalScope;
            eventDesc.wait = waitScope;
            SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, events + i));
        }
    }
}

bool counterBasedEventsExtensionPresent(ze_driver_handle_t &driverHandle) {
    std::string cbEventsExtensionString("ZE_experimental_event_pool_counter_based");
    ze_driver_extension_properties_t cbEventsExtension{};

    std::snprintf(cbEventsExtension.name, sizeof(cbEventsExtension.name), "%s", cbEventsExtensionString.c_str());
    cbEventsExtension.version = ZE_EVENT_POOL_COUNTER_BASED_EXP_VERSION_CURRENT;

    std::vector<ze_driver_extension_properties_t> extensionsToCheck;
    extensionsToCheck.push_back(cbEventsExtension);

    return LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionsToCheck);
}

std::vector<ze_device_handle_t> zelloGetSubDevices(ze_device_handle_t &device, uint32_t &subDevCount) {
    uint32_t deviceCount = 0;
    std::vector<ze_device_handle_t> subdevs(deviceCount, nullptr);
    SUCCESS_OR_TERMINATE(zeDeviceGetSubDevices(device, &deviceCount, nullptr));
    if (deviceCount == 0) {
        if (verbose) {
            std::cout << "No sub device found!\n";
        }
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
        std::cerr << "No driver handle found!\n";
        std::terminate();
    }

    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, &driverHandle));
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    SUCCESS_OR_TERMINATE(zeContextCreate(driverHandle, &contextDesc, &context));

    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cerr << "No device found!\n";
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
    cmdQueue = createCommandQueue(context, device, &ordinal, false);
}

bool checkImageSupport(ze_device_handle_t hDevice, bool test1D, bool test2D, bool test3D, bool testArray) {
    ze_device_image_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_IMAGE_PROPERTIES;

    SUCCESS_OR_TERMINATE(zeDeviceGetImageProperties(hDevice, &imageProperties));

    if (test1D && (imageProperties.maxImageDims1D == 0)) {
        return false;
    }
    if (test2D && (imageProperties.maxImageDims2D == 0)) {
        return false;
    }
    if (test3D && (imageProperties.maxImageDims3D == 0)) {
        return false;
    }
    if (testArray && (imageProperties.maxImageArraySlices == 0)) {
        return false;
    }

    return true;
}

void teardown(ze_context_handle_t context, ze_command_queue_handle_t cmdQueue) {
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

void printDeviceProperties(const ze_device_properties_t &props) {
    if (verbose) {
        std::cout << "Device : "
                  << "\n"
                  << " * name : " << props.name << "\n"
                  << " * type : " << ((props.type == ZE_DEVICE_TYPE_GPU) ? "GPU" : "FPGA") << "\n"
                  << std::hex
                  << " * vendorId : 0x" << std::setw(4) << std::setfill('0') << props.vendorId << "\n"
                  << " * deviceId : 0x" << std::setw(4) << std::setfill('0') << props.deviceId << "\n"
                  << std::dec
                  << " * subdeviceId : " << props.subdeviceId << "\n"
                  << " * coreClockRate : " << props.coreClockRate << "\n"
                  << " * maxMemAllocSize : " << props.maxMemAllocSize << "\n"
                  << " * maxHardwareContexts : " << props.maxHardwareContexts << "\n"
                  << " * isSubdevice : " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) << "\n"
                  << " * eccMemorySupported : " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_PROPERTY_FLAG_ECC)) << "\n"
                  << " * onDemandPageFaultsSupported : " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING)) << "\n"
                  << " * integrated : " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED)) << "\n"
                  << " * maxCommandQueuePriority  : " << props.maxCommandQueuePriority << "\n"
                  << " * numThreadsPerEU  : " << props.numThreadsPerEU << "\n"
                  << " * numEUsPerSubslice  : " << props.numEUsPerSubslice << "\n"
                  << " * numSubslicesPerSlice  : " << props.numSubslicesPerSlice << "\n"
                  << " * numSlices  : " << props.numSlices << "\n"
                  << " * physicalEUSimdWidth  : " << props.physicalEUSimdWidth << "\n"
                  << " * timerResolution : " << props.timerResolution << "\n"
                  << " * uuid : ";
        for (uint32_t i = 0; i < ZE_MAX_UUID_SIZE; i++) {
            std::cout << std::hex << static_cast<uint32_t>(props.uuid.id[i]) << " ";
        }
        std::cout << "\n";
    } else {
        std::cout << "Device : \n"
                  << " * name : " << props.name << "\n"
                  << std::hex
                  << " * vendorId : 0x" << std::setw(4) << std::setfill('0') << props.vendorId << "\n"
                  << " * deviceId : 0x" << std::setw(4) << std::setfill('0') << props.deviceId << "\n"
                  << std::dec;
    }
}

void printCacheProperties(uint32_t index, const ze_device_cache_properties_t &props) {
    if (verbose) {
        std::cout << "Cache properties: \n"
                  << index << "\n"
                  << " * User Cache Control : " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_CACHE_PROPERTY_FLAG_USER_CONTROL)) << "\n"
                  << " * cache size : " << props.cacheSize << "\n";
    }
}

void printP2PProperties(const ze_device_p2p_properties_t &props, bool canAccessPeer, uint32_t device0Index, uint32_t device1Index) {
    if (verbose) {
        std::cout << " * P2P Properties device " << device0Index << " to peer " << device1Index << "\n";
        std::cout << "\t* accessSupported: " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS)) << "\n";
        std::cout << "\t* atomicsSupported: " << std::boolalpha << static_cast<bool>(!!(props.flags & ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS)) << "\n";
        std::cout << "\t* canAccessPeer: " << std::boolalpha << static_cast<bool>(canAccessPeer) << "\n";
        if (props.pNext != nullptr) {
            ze_base_desc_t *desc = reinterpret_cast<ze_base_desc_t *>(props.pNext);
            if (desc->stype == ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES) {
                ze_device_p2p_bandwidth_exp_properties_t *expBwProperties =
                    reinterpret_cast<ze_device_p2p_bandwidth_exp_properties_t *>(desc);
                std::cout << " * P2P Exp Properties: Logical BW: " << std::dec << expBwProperties->logicalBandwidth << "\n";
                std::cout << " * P2P Exp Properties: Physical BW: " << std::dec << expBwProperties->physicalBandwidth << "\n";
                std::cout << " * P2P Exp Properties: BW unit: " << std::dec << expBwProperties->bandwidthUnit << "\n";

                std::cout << " * P2P Exp Properties: Logical Latency: " << std::dec << expBwProperties->logicalLatency << "\n";
                std::cout << " * P2P Exp Properties: Physical Latency: " << std::dec << expBwProperties->physicalLatency << "\n";
                std::cout << " * P2P Exp Properties: Latency unit: " << std::dec << expBwProperties->latencyUnit << "\n";
            }
        }
    }
}

void printKernelProperties(const ze_kernel_properties_t &props, const char *kernelName) {
    if (verbose) {
        std::cout << "Kernel : \n"
                  << " * name : " << kernelName << "\n"
                  << " * uuid.mid : " << props.uuid.mid << "\n"
                  << " * uuid.kid : " << props.uuid.kid << "\n"
                  << " * maxSubgroupSize : " << props.maxSubgroupSize << "\n"
                  << " * localMemSize : " << props.localMemSize << "\n"
                  << " * spillMemSize : " << props.spillMemSize << "\n"
                  << " * privateMemSize : " << props.privateMemSize << "\n"
                  << " * maxNumSubgroups : " << props.maxNumSubgroups << "\n"
                  << " * numKernelArgs : " << props.numKernelArgs << "\n"
                  << " * requiredSubgroupSize : " << props.requiredSubgroupSize << "\n"
                  << " * requiredNumSubGroups : " << props.requiredNumSubGroups << "\n"
                  << " * requiredGroupSizeX : " << props.requiredGroupSizeX << "\n"
                  << " * requiredGroupSizeY : " << props.requiredGroupSizeY << "\n"
                  << " * requiredGroupSizeZ : " << props.requiredGroupSizeZ << "\n";
    }
}

void printCommandQueueGroupsProperties(ze_device_handle_t &device) {
    if (verbose) {
        uint32_t numQueueGroups = 0;
        SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
        if (numQueueGroups == 0) {
            std::cerr << "No queue groups found!\n";
            std::terminate();
        }
        std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
        SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                    queueProperties.data()));

        for (uint32_t i = 0; i < numQueueGroups; i++) {
            bool computeQueue = (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            bool copyQueue = (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            std::cout << "CommandQueue group : " << i << "\n"
                      << " * flags : " << std::hex << queueProperties[i].flags << std::dec << "\n";
            if (computeQueue) {
                std::cout << "    flag: ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE \n";
            }
            if (copyQueue) {
                std::cout << "    flag: ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY \n";
            }
            std::cout << " * maxMemoryFillPatternSize : " << queueProperties[i].maxMemoryFillPatternSize << "\n"
                      << " * numQueues : " << queueProperties[i].numQueues << "\n";
        }
    }
}

const std::vector<const char *> &getResourcesSearchLocations() {
    static std::vector<const char *> locations {
        "test_files/spv_modules/",
#if defined(OS_DATADIR)
            TOSTR(OS_DATADIR),
#endif
    };
    return locations;
}

void setEnvironmentVariable(const char *variableName, const char *variableValue) {
#ifdef _WIN64
    SetEnvironmentVariableA(variableName, variableValue);
#else
    setenv(variableName, variableValue, 1);
#endif
}

ze_result_t CommandHandler::create(ze_context_handle_t context, ze_device_handle_t device, bool immediate) {
    isImmediate = immediate;
    ze_result_t result;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;

    if (isImmediate) {
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        result = zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList);
    } else {
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        result = zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        result = createCommandList(context, device, cmdList, false);
    }

    return result;
}

ze_result_t CommandHandler::execute() {
    auto result = ZE_RESULT_SUCCESS;

    if (!isImmediate) {
        result = zeCommandListClose(cmdList);
        if (result == ZE_RESULT_SUCCESS) {
            result = zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr);
        }
    }
    return result;
}

ze_result_t CommandHandler::synchronize() {
    if (!isImmediate) {
        return zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max());
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandHandler::destroy() {
    auto result = zeCommandListDestroy(cmdList);
    if (result == ZE_RESULT_SUCCESS && !isImmediate) {
        result = zeCommandQueueDestroy(cmdQueue);
    }
    return result;
}

TestBitMask getTestMask(int argc, char *argv[], uint32_t defaultValue) {
    uint32_t value = getParamValue(argc, argv, "-m", "-mask", defaultValue);
    std::cout << "Test mask ";
    if (value != defaultValue) {
        std::cout << "override ";
    } else {
        std::cout << "default ";
    }
    TestBitMask bitValue(value);
    std::cout << "value 0b" << bitValue << std::endl;

    return bitValue;
}

void printGroupCount(ze_group_count_t &groupCount) {
    if (verbose) {
        std::cout << "Number of groups : (" << groupCount.groupCountX << ", "
                  << groupCount.groupCountY << ", " << groupCount.groupCountZ << ")"
                  << std::endl;
    }
}

void printBuildLog(std::string &buildLog) {
    if (buildLog.size() > 0) {
        std::cerr << "Build log:" << std::endl
                  << buildLog << std::endl;
    }
}

void printBuildLog(const char *strLog) {
    std::cerr << "Build log:" << std::endl
              << strLog << std::endl;
}

void loadDriverExtensions(ze_driver_handle_t &driverHandle, std::vector<ze_driver_extension_properties_t> &driverExtensions) {
    uint32_t extensionsCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionsCount, nullptr));
    if (extensionsCount == 0) {
        std::cerr << "No extensions supported on this driver" << std::endl;
        driverExtensions.resize(extensionsCount);
        return;
    }

    driverExtensions.resize(extensionsCount);
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionsCount, driverExtensions.data()));

    for (uint32_t i = 0; i < extensionsCount; i++) {
        uint32_t supportedVersion = driverExtensions[i].version;
        if (verbose) {
            std::cout << "Extension #" << i << " name: " << driverExtensions[i].name << " version: " << ZE_MAJOR_VERSION(supportedVersion) << "." << ZE_MINOR_VERSION(supportedVersion) << std::endl;
        }
    }
}

bool checkExtensionIsPresent(ze_driver_handle_t &driverHandle, std::vector<ze_driver_extension_properties_t> &extensionsToCheck) {
    uint32_t numMatchedExtensions = 0;

    if (!driverExtensions.loaded) {
        loadDriverExtensions(driverHandle, driverExtensions.extensions);
        driverExtensions.loaded = true;
    }

    for (uint32_t i = 0; i < driverExtensions.extensions.size(); i++) {
        for (const auto &checkedExtension : extensionsToCheck) {
            std::string checkedExtensionName = checkedExtension.name;
            if (strncmp(driverExtensions.extensions[i].name, checkedExtensionName.c_str(), checkedExtensionName.size()) == 0) {
                uint32_t version = checkedExtension.version;
                uint32_t supportedVersion = driverExtensions.extensions[i].version;
                if (verbose) {
                    std::cout << "Checked extension: " << checkedExtensionName << " found in the driver." << std::endl;
                }
                if (version <= supportedVersion) {
                    if (verbose) {
                        std::cout << "Checked extension version: " << ZE_MAJOR_VERSION(version) << "." << ZE_MINOR_VERSION(version) << " is equal or lower than present." << std::endl;
                    }
                    numMatchedExtensions++;
                } else {
                    if (verbose) {
                        std::cout << "Checked extension version: " << ZE_MAJOR_VERSION(version) << "." << ZE_MINOR_VERSION(version) << " is greater than present." << std::endl;
                    }
                }
            }
        }
    }

    return (numMatchedExtensions == extensionsToCheck.size());
}

void prepareScratchTestValues(uint32_t &arraySize, uint32_t &vectorSize, uint32_t &expectedMemorySize, uint32_t &srcAdditionalMul, uint32_t &srcMemorySize, uint32_t &idxMemorySize) {
    uint32_t typeSize = sizeof(uint32_t);

    arraySize = 32;
    vectorSize = 16;
    srcAdditionalMul = 3u;

    expectedMemorySize = arraySize * vectorSize * typeSize * 2;
    srcMemorySize = expectedMemorySize * srcAdditionalMul;
    idxMemorySize = arraySize * typeSize;
}

void prepareScratchTestBuffers(void *srcBuffer, void *idxBuffer, void *expectedMemory, uint32_t arraySize, uint32_t vectorSize, uint32_t expectedMemorySize, uint32_t srcAdditionalMul) {
    auto srcBufferLong = static_cast<uint64_t *>(srcBuffer);
    auto expectedMemoryLong = static_cast<uint64_t *>(expectedMemory);

    for (uint32_t i = 0; i < arraySize; ++i) {
        static_cast<uint32_t *>(idxBuffer)[i] = 2;
        for (uint32_t vecIdx = 0; vecIdx < vectorSize; ++vecIdx) {
            for (uint32_t srcMulIdx = 0; srcMulIdx < srcAdditionalMul; ++srcMulIdx) {
                srcBufferLong[(i * vectorSize * srcAdditionalMul) + srcMulIdx * vectorSize + vecIdx] = 1l;
            }
            expectedMemoryLong[i * vectorSize + vecIdx] = 2l;
        }
    }
}

} // namespace LevelZeroBlackBoxTests
