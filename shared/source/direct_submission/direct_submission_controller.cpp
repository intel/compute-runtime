/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"

#include <chrono>

namespace NEO {

DirectSubmissionController::DirectSubmissionController() {
    if (DebugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeout = DebugManager.flags.DirectSubmissionControllerTimeout.get();
    }
    if (DebugManager.flags.DirectSubmissionControllerDivisor.get() != -1) {
        timeoutDivisor = DebugManager.flags.DirectSubmissionControllerDivisor.get();
    }

    directSubmissionControllingThread = Thread::create(controlDirectSubmissionsState, reinterpret_cast<void *>(this));
};

DirectSubmissionController::~DirectSubmissionController() {
    keepControlling.store(false);
    if (directSubmissionControllingThread) {
        directSubmissionControllingThread->join();
        directSubmissionControllingThread.reset();
    }
}

void DirectSubmissionController::registerDirectSubmission(CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(directSubmissionsMutex);
    directSubmissions.insert(std::make_pair(csr, DirectSubmissionState{}));
    this->adjustTimeout(csr);
}

void DirectSubmissionController::unregisterDirectSubmission(CommandStreamReceiver *csr) {
    std::lock_guard<std::mutex> lock(directSubmissionsMutex);
    directSubmissions.erase(csr);
}

void DirectSubmissionController::startControlling() {
    this->runControlling.store(true);
}

void *DirectSubmissionController::controlDirectSubmissionsState(void *self) {
    auto controller = reinterpret_cast<DirectSubmissionController *>(self);

    while (!controller->runControlling.load()) {
        if (!controller->keepControlling.load()) {
            return nullptr;
        }

        controller->sleep();
    }

    while (true) {
        if (!controller->keepControlling.load()) {
            return nullptr;
        }

        controller->sleep();
        controller->checkNewSubmissions();
    }
}

void DirectSubmissionController::checkNewSubmissions() {
    std::lock_guard<std::mutex> lock(this->directSubmissionsMutex);

    for (auto &directSubmission : this->directSubmissions) {
        auto csr = directSubmission.first;
        auto &state = directSubmission.second;

        auto taskCount = csr->peekTaskCount();
        if (taskCount == state.taskCount) {
            if (state.isStopped) {
                continue;
            } else {
                auto lock = csr->obtainUniqueOwnership();
                csr->stopDirectSubmission();
                state.isStopped = true;
            }
        } else {
            state.isStopped = false;
            state.taskCount = taskCount;
        }
    }
}

void DirectSubmissionController::sleep() {
    std::this_thread::sleep_for(std::chrono::microseconds(this->timeout));
}

void DirectSubmissionController::adjustTimeout(CommandStreamReceiver *csr) {
    if (EngineHelpers::isCcs(csr->getOsContext().getEngineType())) {
        for (size_t subDeviceIndex = 0u; subDeviceIndex < csr->getOsContext().getDeviceBitfield().size(); ++subDeviceIndex) {
            if (csr->getOsContext().getDeviceBitfield().test(subDeviceIndex)) {
                ++this->ccsCount[subDeviceIndex];
            }
        }
        auto curentMaxCcsCount = std::max_element(this->ccsCount.begin(), this->ccsCount.end());
        if (*curentMaxCcsCount > this->maxCcsCount) {
            this->maxCcsCount = *curentMaxCcsCount;
            this->timeout /= this->timeoutDivisor;
        }
    }
}

} // namespace NEO