/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

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