/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

void mockCpuidEnableAll(int *cpuInfo, int functionId);

void mockCpuidFunctionAvailableDisableAll(int *cpuInfo, int functionId);

void mockCpuidFunctionNotAvailableDisableAll(int *cpuInfo, int functionId);

void mockCpuidReport36BitVirtualAddressSize(int *cpuInfo, int functionId);
