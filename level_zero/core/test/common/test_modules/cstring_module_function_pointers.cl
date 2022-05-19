/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

char *__builtin_IB_get_function_pointer(__constant char *function_name);
void __builtin_IB_call_function_pointer(char *function_pointer,
                                        char *argument_structure);

struct FunctionData {
    char *dst;
    const char *src;
    unsigned int gid;
};

kernel void memcpy_bytes(__global char *dst, const __global char *src, __global char *pBufferWithFunctionPointer) {
    unsigned int gid = get_global_id(0);
    struct FunctionData functionData;
    functionData.dst = dst;
    functionData.src = src;
    functionData.gid = gid;
    char **pBufferWithFunctionPointerChar = (char **)pBufferWithFunctionPointer;
    __builtin_IB_call_function_pointer(pBufferWithFunctionPointerChar[0], (char *)&functionData);
}

void copy_helper(char *data) {
    struct FunctionData *pFunctionData = (struct FunctionData *)data;
    char *dst = pFunctionData->dst;
    const char *src = pFunctionData->src;
    unsigned int gid = pFunctionData->gid;
    dst[gid] = src[gid];
}

void other_indirect_f(unsigned int *dimNum) {
    *dimNum = get_global_id(*dimNum);
}

// Workaround for IGC to generate symbols for function pointers:
// They need to be indirectly referenced inside a kernel and we
// need to have at least two
__kernel void workaround_kernel() {
    char *fp = 0;
    switch (get_global_id(0)) {
    case 0:
        fp = __builtin_IB_get_function_pointer("copy_helper");
        break;
    case 1:
        fp = __builtin_IB_get_function_pointer("other_indirect_f");
        break;
    }
    __builtin_IB_call_function_pointer(fp, 0);
}
