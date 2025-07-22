/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_compile.h"

#include "ocloc_api.h"
#include "zello_common.h"

#include <cstring>

namespace LevelZeroBlackBoxTests {

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, std::string &outCompilerLog) {
    return compileToSpirV(src, options, {}, outCompilerLog);
}

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, const std::string &device, std::string &outCompilerLog) {
    std::vector<uint8_t> ret;

    const char *mainFileName = "main.cl";
    const char *argv[] = {"ocloc", "-q", "-spv_only", "-file", mainFileName, "", "", "", ""};
    uint32_t numArgs = sizeof(argv) / sizeof(argv[0]) - 4;
    uint32_t nextArgIndex = 5;
    if (device.size() > 0) {
        argv[nextArgIndex++] = "-device";
        argv[nextArgIndex++] = device.c_str();
        numArgs += 2;
    }
    if (options.size() > 0) {
        argv[nextArgIndex++] = "-options";
        argv[nextArgIndex++] = options.c_str();
        numArgs += 2;
    }
    const unsigned char *sources[] = {reinterpret_cast<const unsigned char *>(src.c_str())};
    size_t sourcesLengths[] = {src.size() + 1};
    const char *sourcesNames[] = {mainFileName};
    unsigned int numOutputs = 0U;
    unsigned char **outputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    int result = oclocInvoke(numArgs, argv,
                             1, sources, sourcesLengths, sourcesNames,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputs, &ouputLengths, &outputNames);

    unsigned char *spirV = nullptr;
    size_t spirVlen = 0;
    const char *log = nullptr;
    size_t logLen = 0;
    for (unsigned int i = 0; i < numOutputs; ++i) {
        std::string spvExtension = ".spv";
        std::string logFileName = "stdout.log";
        auto nameLen = std::strlen(outputNames[i]);
        if ((nameLen > spvExtension.size()) && (std::strstr(&outputNames[i][nameLen - spvExtension.size()], spvExtension.c_str()) != nullptr)) {
            spirV = outputs[i];
            spirVlen = ouputLengths[i];
        } else if ((nameLen >= logFileName.size()) && (std::strstr(outputNames[i], logFileName.c_str()) != nullptr)) {
            log = reinterpret_cast<const char *>(outputs[i]);
            logLen = ouputLengths[i];
            break;
        }
    }

    if ((result != 0) && (logLen == 0)) {
        outCompilerLog = "Unknown error, ocloc returned : " + std::to_string(result) + "\n";
        return ret;
    }

    if (logLen != 0) {
        outCompilerLog = std::string(log, logLen).c_str();
    }

    ret.assign(spirV, spirV + spirVlen);
    oclocFreeOutput(&numOutputs, &outputs, &ouputLengths, &outputNames);
    return ret;
}

std::vector<uint8_t> compileToNative(const std::string &src, const std::string &deviceName, const std::string &revisionId, const std::string &options, const std::string &internalOptions, const std::string &statefulMode, std::string &outCompilerLog) {
    std::vector<uint8_t> ret;

    const char *mainFileName = "main.cl";
    const char *argv[] = {"ocloc", "-v", "-device", deviceName.c_str(), "-revision_id", revisionId.c_str(), "-file", mainFileName, "-o", "output.bin", "", "", "", "", "", ""};
    uint32_t numArgs = sizeof(argv) / sizeof(argv[0]) - 6;
    int argIndex = 10;
    if (options.size() > 0) {
        argv[argIndex++] = "-options";
        argv[argIndex++] = options.c_str();
        numArgs += 2;
    }
    if (internalOptions.size() > 0) {
        argv[argIndex++] = "-internal_options";
        argv[argIndex++] = internalOptions.c_str();
        numArgs += 2;
    }
    if (statefulMode.size() > 0) {
        argv[argIndex++] = "-stateful_address_mode";
        argv[argIndex++] = statefulMode.c_str();
        numArgs += 2;
    }
    const unsigned char *sources[] = {reinterpret_cast<const unsigned char *>(src.c_str())};
    size_t sourcesLengths[] = {src.size() + 1};
    const char *sourcesNames[] = {mainFileName};
    unsigned int numOutputs = 0U;
    unsigned char **outputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    int result = oclocInvoke(numArgs, argv,
                             1, sources, sourcesLengths, sourcesNames,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputs, &ouputLengths, &outputNames);

    unsigned char *binary = nullptr;
    size_t binaryLen = 0;
    const char *log = nullptr;
    size_t logLen = 0;
    for (unsigned int i = 0; i < numOutputs; ++i) {
        std::string spvExtension = ".spv";
        std::string logFileName = "stdout.log";
        auto nameLen = std::strlen(outputNames[i]);
        if (std::strstr(outputNames[i], "output.bin") != nullptr) {
            binary = outputs[i];
            binaryLen = ouputLengths[i];
        } else if ((nameLen >= logFileName.size()) && (std::strstr(outputNames[i], logFileName.c_str()) != nullptr)) {
            log = reinterpret_cast<const char *>(outputs[i]);
            logLen = ouputLengths[i];
            break;
        }
    }

    if ((result != 0) && (logLen == 0)) {
        outCompilerLog = "Unknown error, ocloc returned : " + std::to_string(result) + "\n";
        return ret;
    }

    if (logLen != 0) {
        outCompilerLog = std::string(log, logLen).c_str();
    }

    ret.assign(binary, binary + binaryLen);
    oclocFreeOutput(&numOutputs, &outputs, &ouputLengths, &outputNames);
    return ret;
}

const char *memcpyBytesTestKernelSrc = R"===(
kernel void memcpy_bytes(__global char *dst, const __global char *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

const char *memcpyBytesWithPrintfTestKernelSrc = R"==(
__kernel void memcpy_bytes(__global uchar *dst, const __global uchar *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = (uchar)(src[gid] + gid);
    if (gid == 0) {
        printf("gid =  %d \n", gid);
    }
}
)==";

const char *openCLKernelsSource = R"OpenCLC(
__kernel void add_constant(global int *values, int addval) {
    const int xid = get_global_id(0);
    values[xid] = values[xid] + addval;
}

__kernel void increment_by_one(__global uchar *dst, __global uchar *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = (uchar)(src[gid] + 1);
}
)OpenCLC";

const char *scratchKernelSrc = R"===(
typedef long16 TYPE;
__attribute__((reqd_work_group_size(32, 1, 1))) // force LWS to 32
__attribute__((intel_reqd_sub_group_size(16)))   // force SIMD to 16
__kernel void
scratch_kernel(__global int *resIdx, global TYPE *src, global TYPE *dst) {
    size_t lid = get_local_id(0);
    size_t gid = get_global_id(0);

    TYPE res1 = src[gid * 3];
    TYPE res2 = src[gid * 3 + 1];
    TYPE res3 = src[gid * 3 + 2];

    __local TYPE locMem[32];
    locMem[lid] = res1;
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    TYPE res = (locMem[resIdx[gid]] * res3) * res2 + res1;
    dst[gid] = res;
}
)===";

const char *scratchKernelBuildOptions = "-igc_opts 'VISAOptions=-forcespills' ";

const char *printfKernelSource = R"===(

#define MACRO_STR1 "string with tab(\\t) new line(\\n):"
#define MACRO_STR2 "using tab \tand new line \nin this string"

void printf_function();

__kernel void printf_kernel(char byteValue, short shortValue, int intValue, long longValue) {
    printf("byte = %hhd\nshort = %hd\nint = %d\nlong = %ld", byteValue, shortValue, intValue, longValue);
}

__kernel void printf_kernel1() {
    uint gid = get_global_id(0);
    if( get_local_id(0) == 0 )
    {
        printf("id == %d\n", 0);
    }
}

__kernel void print_string() {
    printf("%s\n%s", "string with tab(\\t) new line(\\n):", "using tab \tand new line \nin this string");
}

__kernel void print_macros() {
    printf("%s\n%s", MACRO_STR1, MACRO_STR2);
}

__kernel void print_from_function_kernel() {
    printf_function();
}

)===";

const char *printfFunctionSource = R"===(

void printf_function() {
     printf("test function\n");
}

)===";

const char *readNV12Module = R"===(
__kernel void
ReadNV12Kernel(
    read_only image2d_t nv12Img,
    uint width,
    uint height,
    __global uchar *pDest) {
    int tid_x = get_global_id(0);
    int tid_y = get_global_id(1);
    float4 colorY;
    int2 coord;
    const sampler_t samplerA = CLK_NORMALIZED_COORDS_FALSE |
                               CLK_ADDRESS_NONE |
                               CLK_FILTER_NEAREST;
    if (tid_x < width && tid_y < height) {
        coord = (int2)(tid_x, tid_y);
        if (((tid_y * width) + tid_x) < (width * height)) {
            colorY = read_imagef(nv12Img, samplerA, coord);
            pDest[(tid_y * width) + tid_x] = (uchar)(255.0f * colorY.y);
            if ((tid_x % 2 == 0) && (tid_y % 2 == 0)) {
                pDest[(width * height) + (tid_y / 2 * width) + (tid_x)] = (uchar)(255.0f * colorY.z);
                pDest[(width * height) + (tid_y / 2 * width) + (tid_x) + 1] = (uchar)(255.0f * colorY.x);
            }
        }
    }
}
)===";

const char *functionPointersProgram = R"==(
__global char *__builtin_IB_get_function_pointer(__constant char *function_name);
void __builtin_IB_call_function_pointer(__global char *function_pointer,
                                        char *argument_structure);

struct FunctionData {
    __global char *dst;
    const __global char *src;
    unsigned int gid;
};

kernel void memcpy_bytes(__global char *dst, const __global char *src, __global char *pBufferWithFunctionPointer) {
    unsigned int gid = get_global_id(0);
    struct FunctionData functionData;
    functionData.dst = dst;
    functionData.src = src;
    functionData.gid = gid;
    __global char * __global *pBufferWithFunctionPointerChar = (__global char * __global *)pBufferWithFunctionPointer;
    __builtin_IB_call_function_pointer(pBufferWithFunctionPointerChar[0], (char *)&functionData);
}

void copy_helper(char *data) {
    if(data != NULL) {
        struct FunctionData *pFunctionData = (struct FunctionData *)data;
        __global char *dst = pFunctionData->dst;
        const __global char *src = pFunctionData->src;
        unsigned int gid = pFunctionData->gid;
        dst[gid] = src[gid];
    }
}

void other_indirect_f(unsigned int *dimNum) {
    if(dimNum != NULL) {
        if(*dimNum > 2) {
            *dimNum += 2;
        }
    }
}

__kernel void workaround_kernel() {
    __global char *fp = 0;
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
)==";

const char *dynLocalBarrierArgSrc = R"==(
__kernel void local_barrier_arg(__local int *local_dst1, __global int *dst, __local int *local_dst2,
        __local ulong *local_dst3, __local int *local_dst4) {
    unsigned int gid = get_global_id(0);
    if(get_local_id(0) == 0){
        local_dst1[0] = 6;
        local_dst2[0] = 7;
        local_dst3[0] = 8;
        local_dst4[0] = 9;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    if(get_local_id(0) == 0){
        local_dst1[0] = gid + local_dst1[0];
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    if(get_local_id(0) == 0){
        local_dst2[0] = gid + local_dst2[0];
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    if(get_local_id(0) == 0){
        local_dst3[0] = gid + local_dst3[0];
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    if(get_local_id(0) == 0){
        local_dst4[0] = gid + local_dst4[0];
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    if(gid == 0){
        dst[0] = local_dst1[0] + local_dst2[0] + local_dst3[0] + local_dst4[0];
    }
}
)==";

const char *atomicIncSrc = R"===(
__kernel void testKernel(__global uint *dst) {
        atomic_inc(dst);
}
)===";

namespace DynamicLink {

const char *importModuleSrc = R"===(
int lib_func_add(int x, int y);
int lib_func_mult(int x, int y);
int lib_func_sub(int x, int y);

kernel void call_library_funcs(__global int* result) {
    int add_result = lib_func_add(1,2);
    int mult_result = lib_func_mult(add_result,2);
    result[0] = lib_func_sub(mult_result, 1);
}
)===";

const char *exportModuleSrc = R"===(
int lib_func_add(int x, int y) {
    return x+y;
}

int lib_func_mult(int x, int y) {
    return x*y;
}

int lib_func_sub(int x, int y) {
    return x-y;
}
)===";

const char *importModuleSrcCircDep = R"===(
int lib_func_add(int x, int y);
int lib_func_mult(int x, int y);
int lib_func_sub(int x, int y);

kernel void call_library_funcs(__global int* result) {
    int add_result = lib_func_add(1,2);
    int mult_result = lib_func_mult(add_result,2);
    result[0] = lib_func_sub(mult_result, 1);
}

int lib_func_add2(int x) {
    return x+2;
}
)===";

const char *exportModuleSrcCircDep = R"===(
int lib_func_add2(int x);
int lib_func_add5(int x);

int lib_func_add(int x, int y) {
    return lib_func_add5(lib_func_add2(x + y));
}

int lib_func_mult(int x, int y) {
    return x*y;
}

int lib_func_sub(int x, int y) {
    return x-y;
}
)===";

const char *exportModuleSrc2CircDep = R"===(
int lib_func_add5(int x) {
    return x+5;
}
)===";

} // namespace DynamicLink

void createScratchModuleKernel(ze_context_handle_t &context,
                               ze_device_handle_t &device,
                               ze_module_handle_t &module,
                               ze_kernel_handle_t &kernel,
                               std::string *additionalBuildOptions) {
    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::scratchKernelSrc, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    std::string buildOptions = LevelZeroBlackBoxTests::scratchKernelBuildOptions;
    if (additionalBuildOptions != nullptr) {
        buildOptions += (*additionalBuildOptions);
    }
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = buildOptions.c_str();

    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        LevelZeroBlackBoxTests::printBuildLog(strLog);

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << "\nScratch Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "scratch_kernel";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernelProperties));
    std::cout << "Scratch size = " << std::dec << kernelProperties.spillMemSize << "\n";
}

} // namespace LevelZeroBlackBoxTests
