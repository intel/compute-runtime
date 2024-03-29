/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

int setAlarm(bool enableAlarm);

int setSegv(bool enableSegv);

int setAbrt(bool enableAbrt);

void cleanupSignals();
