/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <cstdint>
#include <string>
#include <vector>

namespace LevelZeroBlackBoxTests {

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, std::string &outCompilerLog);
std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, const std::string &device, std::string &outCompilerLog);
std::vector<uint8_t> compileToNative(const std::string &src, const std::string &deviceName, const std::string &revisionId, const std::string &options, const std::string &internalOptions, const std::string &statefulMode, std::string &outCompilerLog);

extern const char *slmArgKernelSrc;

extern const char *openCLKernelsSource;

extern const char *scratchKernelSrc;
extern const char *scratchKernelBuildOptions;

extern const char *printfKernelSource;
extern const char *printfFunctionSource;
extern const char *memcpyBytesWithPrintfTestKernelSrc;

extern const char *readNV12Module;

extern const char *functionPointersProgram;

extern const char *dynLocalBarrierArgSrc;

extern const char *atomicIncSrc;

namespace DynamicLink {
extern const char *importModuleSrc;
extern const char *exportModuleSrc;
extern const char *importModuleSrcCircDep;
extern const char *exportModuleSrcCircDep;
extern const char *exportModuleSrc2CircDep;
} // namespace DynamicLink

void createScratchModuleKernel(ze_context_handle_t &context,
                               ze_device_handle_t &device,
                               ze_module_handle_t &module,
                               ze_kernel_handle_t &kernel,
                               std::string *additionalBuildOptions);

void createModuleFromSpirV(ze_context_handle_t context, ze_device_handle_t device, const char *kernelSrc, ze_module_handle_t &module);
void createKernelWithName(ze_module_handle_t module, const char *kernelName, ze_kernel_handle_t &kernel);

} // namespace LevelZeroBlackBoxTests
