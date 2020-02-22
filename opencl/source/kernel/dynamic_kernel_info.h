/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

struct DynamicKernelInfo {
    char *pCrossThreadData;
    size_t crossThreadDataSize;

    char *pSsh;
    size_t sshSize;
};