/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/os_thread.h"

#include <chrono>

namespace NEO {

DirectSubmissionController::DirectSubmissionController() {
    timeout = 5;

    if (DebugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeout = DebugManager.flags.DirectSubmissionControllerTimeout.get();
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

        std::this_thread::sleep_for(std::chrono::milliseconds(controller->timeout));
    }

    while (true) {

        if (!controller->keepControlling.load()) {
            return nullptr;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(controller->timeout));

        controller->checkNewSubmissions();
    }
}

void DirectSubmissionController::checkNewSubmissions() {
    std::lock_guard<std::mutex> lock(this->directSubmissionsMutex);

    for (auto &directSubmission : this->directSubmissions) {
        auto csr = directSubmission.first;
        auto &state = directSubmission.second;

        auto taskCount = csr->peekTaskCount();
        if (csr->testTaskCountReady(csr->getTagAddress(), taskCount)) {
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
        } else {
            state.isStopped = false;
        }
    }
}

} // namespace NEO