/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <string>
#include <string_view>

int setAlarm(bool enableAlarm);

int setSegv(bool enableSegv);

int setAbrt(bool enableAbrt);

void cleanupSignals();

void resetAlarm(bool enableAlarm);

void pauseAlarm(bool enableAlarm);

void handleTestsTimeout(std::string_view testName, uint32_t elapsedTime);

void markIterationStart();

constexpr size_t maxTestNameLength = 256;
extern std::string lastTest;
