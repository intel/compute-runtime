/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler_definitions.h"

uint GetPatchValueForSLMSize(uint slMsize) {
    uint PatchValue;
    if (slMsize == 0) {
        PatchValue = 0;
    } else {
        uint count4KB = slMsize / 4096;
        if (slMsize % 4096 != 0) {
            count4KB++;
        }
        PatchValue = GetNextPowerof2(count4KB);
    }

    return PatchValue;
}

//on BDW we have only 1 pipe control
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
}

void PutCSStallPipeControl(__global uint *secondaryBatchBuffer, uint dwordOffset, uint pipeControlOffset) {
    dwordOffset += pipeControlOffset;
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