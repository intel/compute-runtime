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

void resetAlarm();

void handleTestsTimeout(std::string_view testName, uint32_t elapsedTime);

constexpr size_t maxTestNameLength = 256;
extern std::string lastTest;
