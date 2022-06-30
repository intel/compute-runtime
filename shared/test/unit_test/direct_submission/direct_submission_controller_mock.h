/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/direct_submission/direct_submission_controller.h"

namespace NEO {
struct DirectSubmissionControllerMock : public DirectSubmissionController {
    using DirectSubmissionController::checkNewSubmissions;
    using DirectSubmissionController::directSubmissionControllingThread;
    using DirectSubmissionController::directSubmissions;
    using DirectSubmissionController::directSubmissionsMutex;
    using DirectSubmissionController::keepControlling;
    using DirectSubmissionController::timeout;
    using DirectSubmissionController::timeoutDivisor;

    void sleep() override {
        DirectSubmissionController::sleep();
        this->sleepCalled = true;
    }

    bool sleepCalled = false;
};
} // namespace NEO