/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <string_view>

int setAlarm(bool enableAlarm);

int setSegv(bool enableSegv);

int setAbrt(bool enableAbrt);

void cleanupSignals();

void handleTestsTimeout(std::string_view testName, uint32_t elapsedTime);
