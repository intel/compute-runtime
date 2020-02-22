/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler_definitions.h"

uint GetPatchValueForSLMSize(uint slMsize) {
    uint PatchValue = 0;
    if (slMsize == 0) {
        PatchValue = 0;
    } else if (slMsize <= (1 * 1024)) {
        PatchValue = 1;
    } else if (slMsize <= (2 * 1024)) {
        PatchValue = 2;
    } else if (slMsize <= (4 * 1024)) {
        PatchValue = 3;
    } else if (slMsize <= (8 * 1024)) {
        PatchValue = 4;
    } else if (slMsize <= (16 * 1024)) {
        PatchValue = 5;
    } else if (slMsize <= (32 * 1024)) {
        PatchValue = 6;
    } else if (slMsize <= (64 * 1024)) {
        PatchValue = 7;
    }
    return PatchValue;
}

void NOOPCSStallPipeControl(__global uint *secondaryBatchBuffer, uint dwordOffset, uint pipeControlOffset) {
    dwordOffset += pipeControlOffset;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
}

void PutCSStallPipeControl(__global uint *secondaryBatchBuffer, uint dwordOffset, uint pipeControlOffset) {
    dwordOffset += pipeControlOffset;
    //first pipe control doing CS stall
    secondaryBatchBuffer[dwordOffset] = PIPE_CONTROL_CSTALL_DWORD0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = PIPE_CONTROL_CSTALL_DWORD1;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    //second pipe control, doing actual timestamp write
    secondaryBatchBuffer[dwordOffset] = PIPE_CONTROL_CSTALL_DWORD0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = PIPE_CONTROL_CSTALL_DWORD1;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
    secondaryBatchBuffer[dwordOffset] = 0;
    dwordOffset++;
}
