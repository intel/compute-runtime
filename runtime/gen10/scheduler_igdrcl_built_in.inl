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
    //todo: veryfy this optimization :
    //return ( SLMSize == 0 ) ? 0 : max( 33 - clz( ( SLMSize - 1 ) >> 10 ), 7 );

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

//on SKL we have pipe control in pairs, therefore when we NOOP we need to do this for both pipe controls
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

//on SKL+  with mid thread preemption we need to have 2 pipe controls instead of 1 any time we do post sync operation
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
    //second pipe control , doing actual timestamp write
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
