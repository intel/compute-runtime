/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstring>
#include <iomanip>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// A single-device IPC event pool is exported from one device and opened in a context
// holding every device. For each device d the importer signals event d on a command list
// of device d (checked by the exporter) and waits on event numDevices + d (signaled by
// the exporter), so every device that can access the pool's memory is exercised.

namespace {

constexpr int maxDevices = 16;
constexpr uint64_t eventTimeoutNs = 10'000'000'000ull;
constexpr int handshakeTimeoutMs = 120'000;

struct PoolCase {
    const char *name;
    ze_event_pool_flags_t flags;
    bool hostVisible;
};

const PoolCase poolCases[] = {
    {"host-visible", ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, true},
    {"kernel-timestamp", ZE_EVENT_POOL_FLAG_IPC | ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, true},
    {"device-only", ZE_EVENT_POOL_FLAG_IPC, false},
};
constexpr int numCases = sizeof(poolCases) / sizeof(poolCases[0]);

enum StepResult : int {
    notRun = 0,
    passed = 1,
    failed = -1
};

struct CaseControl {
    volatile int serverReady;
    volatile int clientOpened;
    volatile int clientSignaled;
    volatile int clientWaitsQueued;
    volatile int serverSignaled;
    volatile int clientDone;
    volatile int openResult;
    volatile int canAccessPeer[maxDevices];
    volatile int signalSubmitted[maxDevices];
    volatile int signalSeenByExporter[maxDevices];
    volatile int waitCompleted[maxDevices];
    char poolHandle[ZE_MAX_IPC_HANDLE_SIZE];
};

struct SharedControl {
    volatile int serverExited;
    volatile int clientExited;
    volatile int numDevices;
    volatile int exporterDevice;
    CaseControl cases[numCases];
};

SharedControl *shared = nullptr;

bool spinUntil(volatile int &flag) {
    for (int waitedMs = 0; waitedMs < handshakeTimeoutMs; waitedMs++) {
        if (flag != 0) {
            return true;
        }
        if (shared->serverExited || shared->clientExited) {
            return flag != 0;
        }
        usleep(1000);
    }
    return false;
}

const char *toString(int result) {
    return result == passed ? "ok" : (result == notRun ? "--" : "FAIL");
}

void logStep(const char *process, const PoolCase &poolCase, const std::string &message) {
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << process << " [" << poolCase.name << "] " << message << std::endl;
    }
}

std::vector<ze_device_handle_t> getDevices(ze_driver_handle_t &driver) {
    SUCCESS_OR_TERMINATE(zeInit(ZE_INIT_FLAG_GPU_ONLY));
    uint32_t driverCount = 1;
    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, &driver));
    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driver, &deviceCount, nullptr));
    std::vector<ze_device_handle_t> devices(deviceCount);
    SUCCESS_OR_TERMINATE(zeDeviceGet(driver, &deviceCount, devices.data()));
    return devices;
}

ze_command_list_handle_t createImmediateCmdList(ze_context_handle_t context, ze_device_handle_t device) {
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_command_list_handle_t cmdList = nullptr;
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &queueDesc, &cmdList));
    return cmdList;
}

ze_event_handle_t createEvent(ze_event_pool_handle_t eventPool, uint32_t index) {
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = index;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t event = nullptr;
    SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, &event));
    return event;
}

void runServer(int requestedExporterDevice) {
    ze_driver_handle_t driver;
    auto devices = getDevices(driver);
    const int numDevices = static_cast<int>(devices.size());
    shared->exporterDevice = (requestedExporterDevice >= 0) ? requestedExporterDevice : numDevices - 1;
    shared->numDevices = numDevices;
    if (numDevices < 2 || numDevices > maxDevices || shared->exporterDevice >= numDevices) {
        return;
    }
    ze_device_handle_t device = devices[shared->exporterDevice];

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ze_context_handle_t context;
    SUCCESS_OR_TERMINATE(zeContextCreateEx(driver, &contextDesc, 1, &device, &context));
    auto cmdList = createImmediateCmdList(context, device);

    for (int caseIndex = 0; caseIndex < numCases; caseIndex++) {
        const PoolCase &poolCase = poolCases[caseIndex];
        CaseControl &control = shared->cases[caseIndex];

        ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
        eventPoolDesc.count = 2 * numDevices;
        eventPoolDesc.flags = poolCase.flags;
        ze_event_pool_handle_t eventPool;
        SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));

        // zeEventCreate resets the event, so all events must exist before the importer signals.
        std::vector<ze_event_handle_t> events;
        for (int i = 0; i < 2 * numDevices; i++) {
            events.push_back(createEvent(eventPool, i));
        }

        ze_ipc_event_pool_handle_t ipcHandle = {};
        SUCCESS_OR_TERMINATE(zeEventPoolGetIpcHandle(eventPool, &ipcHandle));
        memcpy(control.poolHandle, ipcHandle.data, ZE_MAX_IPC_HANDLE_SIZE);
        control.serverReady = 1;
        logStep("server", poolCase, "pool exported from device " + std::to_string(shared->exporterDevice));

        if (spinUntil(control.clientOpened) && control.openResult == ZE_RESULT_SUCCESS) {
            spinUntil(control.clientSignaled);
            for (int d = 0; d < numDevices; d++) {
                if (control.signalSubmitted[d] == notRun) {
                    continue;
                }
                ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
                if (control.signalSubmitted[d] == passed) {
                    if (poolCase.hostVisible) {
                        result = zeEventHostSynchronize(events[d], eventTimeoutNs);
                    } else {
                        result = zeCommandListAppendWaitOnEvents(cmdList, 1, &events[d]);
                        if (result == ZE_RESULT_SUCCESS) {
                            result = zeCommandListHostSynchronize(cmdList, eventTimeoutNs);
                        }
                    }
                }
                control.signalSeenByExporter[d] = (result == ZE_RESULT_SUCCESS) ? passed : failed;
                logStep("server", poolCase, "signal from importer device " + std::to_string(d) + ": " + toString(control.signalSeenByExporter[d]));
            }

            spinUntil(control.clientWaitsQueued);
            for (int d = 0; d < numDevices; d++) {
                SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdList, events[numDevices + d]));
            }
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));
            control.serverSignaled = 1;
            logStep("server", poolCase, "signaled wait events");
        }
        spinUntil(control.clientDone);

        for (auto event : events) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        }
        SUCCESS_OR_TERMINATE(zeEventPoolPutIpcHandle(context, ipcHandle));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

void runClient() {
    ze_driver_handle_t driver;
    auto devices = getDevices(driver);
    const int numDevices = static_cast<int>(devices.size());
    if (numDevices < 2 || numDevices > maxDevices) {
        return;
    }

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ze_context_handle_t context;
    SUCCESS_OR_TERMINATE(zeContextCreateEx(driver, &contextDesc, numDevices, devices.data(), &context));

    std::vector<ze_command_list_handle_t> cmdLists;
    for (int d = 0; d < numDevices; d++) {
        cmdLists.push_back(createImmediateCmdList(context, devices[d]));
    }

    for (int caseIndex = 0; caseIndex < numCases; caseIndex++) {
        const PoolCase &poolCase = poolCases[caseIndex];
        CaseControl &control = shared->cases[caseIndex];

        for (int d = 0; d < numDevices; d++) {
            ze_bool_t canAccessPeer = false;
            SUCCESS_OR_TERMINATE(zeDeviceCanAccessPeer(devices[0], devices[d], &canAccessPeer));
            control.canAccessPeer[d] = canAccessPeer ? 1 : 0;
        }

        if (!spinUntil(control.serverReady)) {
            control.openResult = ZE_RESULT_ERROR_UNKNOWN;
            control.clientOpened = 1;
            control.clientDone = 1;
            continue;
        }
        ze_ipc_event_pool_handle_t ipcHandle = {};
        memcpy(ipcHandle.data, control.poolHandle, ZE_MAX_IPC_HANDLE_SIZE);
        ze_event_pool_handle_t eventPool = nullptr;
        control.openResult = zeEventPoolOpenIpcHandle(context, ipcHandle, &eventPool);
        control.clientOpened = 1;
        logStep("client", poolCase, "open result " + std::to_string(control.openResult));
        if (control.openResult != ZE_RESULT_SUCCESS) {
            control.clientDone = 1;
            continue;
        }

        std::vector<ze_event_handle_t> events;
        for (int d = 0; d < numDevices; d++) {
            if (!control.canAccessPeer[d]) {
                continue;
            }
            events.push_back(createEvent(eventPool, d));
            ze_result_t result = zeCommandListAppendSignalEvent(cmdLists[d], events.back());
            if (result == ZE_RESULT_SUCCESS) {
                result = zeCommandListHostSynchronize(cmdLists[d], eventTimeoutNs);
            }
            control.signalSubmitted[d] = (result == ZE_RESULT_SUCCESS) ? passed : failed;
            logStep("client", poolCase, "signal submitted on device " + std::to_string(d) + ": " + toString(control.signalSubmitted[d]));
        }
        control.clientSignaled = 1;

        for (int d = 0; d < numDevices; d++) {
            if (!control.canAccessPeer[d]) {
                continue;
            }
            events.push_back(createEvent(eventPool, numDevices + d));
            SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdLists[d], 1, &events.back()));
        }
        control.clientWaitsQueued = 1;
        spinUntil(control.serverSignaled);
        for (int d = 0; d < numDevices; d++) {
            if (!control.canAccessPeer[d]) {
                continue;
            }
            control.waitCompleted[d] = (zeCommandListHostSynchronize(cmdLists[d], eventTimeoutNs) == ZE_RESULT_SUCCESS) ? passed : failed;
            logStep("client", poolCase, "wait completed on device " + std::to_string(d) + ": " + toString(control.waitCompleted[d]));
        }

        for (auto event : events) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        }
        SUCCESS_OR_TERMINATE(zeEventPoolCloseIpcHandle(eventPool));
        control.clientDone = 1;
    }

    for (auto cmdList : cmdLists) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

bool reportCase(int caseIndex, int numDevices) {
    const CaseControl &control = shared->cases[caseIndex];
    if (!control.clientOpened) {
        std::cout << "\n[" << poolCases[caseIndex].name << "] not run\n";
        return false;
    }
    bool caseValid = control.clientDone && (control.openResult == ZE_RESULT_SUCCESS);

    std::cout << "\n[" << poolCases[caseIndex].name << "] open result 0x" << std::hex << control.openResult << std::dec << "\n";
    std::cout << "  device  p2p  signalSubmitted  signalSeenByExporter  waitCompleted\n";
    for (int d = 0; d < numDevices; d++) {
        std::cout << "  " << std::setw(6) << d << "  " << std::setw(3) << control.canAccessPeer[d]
                  << "  " << std::setw(15) << toString(control.signalSubmitted[d])
                  << "  " << std::setw(20) << toString(control.signalSeenByExporter[d])
                  << "  " << std::setw(13) << toString(control.waitCompleted[d]) << "\n";
        if (control.canAccessPeer[d]) {
            caseValid &= (control.signalSubmitted[d] == passed) && (control.signalSeenByExporter[d] == passed) && (control.waitCompleted[d] == passed);
        }
    }
    std::cout << "  => " << (caseValid ? "PASSED" : "FAILED") << "\n";
    return caseValid;
}

} // namespace

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello IPC Event Pool P2P";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    const int requestedExporterDevice = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-e", "--exporter", -1);

    shared = static_cast<SharedControl *>(mmap(nullptr, sizeof(SharedControl), PROT_READ | PROT_WRITE,
                                               MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (shared == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    memset(shared, 0, sizeof(SharedControl));

    // Both processes are forked before any L0 call so neither inherits driver state.
    pid_t serverPid = fork();
    if (serverPid == 0) {
        runServer(requestedExporterDevice);
        _exit(0);
    }
    pid_t clientPid = fork();
    if (clientPid == 0) {
        runClient();
        _exit(0);
    }

    int serverStatus = 0;
    int clientStatus = 0;
    for (int exitedCount = 0; exitedCount < 2; exitedCount++) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == serverPid) {
            serverStatus = status;
            shared->serverExited = 1;
        } else if (pid == clientPid) {
            clientStatus = status;
            shared->clientExited = 1;
        }
    }

    const int numDevices = shared->numDevices;
    if (numDevices < 2) {
        std::cout << blackBoxName << " requires at least 2 devices, found " << numDevices << ", skipping\n";
        munmap(shared, sizeof(SharedControl));
        return 0;
    }
    std::cout << "devices: " << numDevices << ", exporter device: " << shared->exporterDevice << ", importer primary device: 0\n";

    bool outputValidationSuccessful = WIFEXITED(serverStatus) && WEXITSTATUS(serverStatus) == 0 &&
                                      WIFEXITED(clientStatus) && WEXITSTATUS(clientStatus) == 0;
    if (!outputValidationSuccessful) {
        std::cout << "server status 0x" << std::hex << serverStatus << ", client status 0x" << clientStatus << std::dec << "\n";
    }
    for (int caseIndex = 0; caseIndex < numCases; caseIndex++) {
        outputValidationSuccessful &= reportCase(caseIndex, numDevices);
    }

    munmap(shared, sizeof(SharedControl));
    LevelZeroBlackBoxTests::printResult(false, outputValidationSuccessful, blackBoxName);
    return outputValidationSuccessful ? 0 : 1;
}
