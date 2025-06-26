/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <iostream>

int main(int argc, char *argv[]) {

    //
    // This compilation step uses the ocloc compiler tool set
    // to convert the "cl" program into a ".spv" file.
    // This ".spv" file in turn is passed to the module create
    // API.
    //
    // The module create API will generate debug info
    // ONLY if the ".spv" file contains "DebugInfo Extended Instructions".
    //
    // https://www.khronos.org/registry/spir-v/specs/unified1/OpenCL.DebugInfo.100.pdf
    //
    // This version of the compiler API library function supports the
    // ability to pass compile options.  The "-g" option is required to
    // cause these Extended Instructions to be included in the ".spv".
    // This may also disable some optimizations.
    //
    // An alternative to using this library function to generate ".spv"
    // files with Debuginfo instructions, is to use the ocloc
    // command, which can currently be found under "build/bin/ocloc".
    //
    // For example, from within the build/bin directory:
    //
    // LD_LIBRARY_PATH=.; ./ocloc -device skl -file cstring_module.cl
    //
    // will produce a file with spv extension which does contain
    // the debug symbols.
    //

    std::string buildLog;
    auto moduleBinary = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::memcpyBytesWithPrintfTestKernelSrc, "-g", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == moduleBinary.size()));

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    ze_module_handle_t module;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(moduleBinary.data());
    moduleDesc.inputSize = moduleBinary.size();
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

    size_t debugInfoSize = 0;
    uint8_t *debugInfo;

    SUCCESS_OR_TERMINATE(zetModuleGetDebugInfo(module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF, &debugInfoSize, nullptr));
    debugInfo = new uint8_t[debugInfoSize];
    //
    // This test isn't actually validating the Dwarf information
    // that is to be returned by the GetDebugInfo() call.  So by placing
    // a non-zero value at the beginning of the debugInfo array, and testing
    // that it was changed AFTER we call zeModuleGetDebugInfo(), we can at least
    // verify that the API call actually DID write something in the debugInfo
    // array.
    //
    debugInfo[0] = 0xff;
    SUCCESS_OR_TERMINATE(zetModuleGetDebugInfo(module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF, &debugInfoSize, debugInfo));
    SUCCESS_OR_TERMINATE_BOOL(debugInfoSize != 0);
    //
    // Validate that in fact the debugInfo array was modified
    // by the call.
    // We aren't actually validating the Dwarf structures that
    // are supposed to be returned here.
    //
    SUCCESS_OR_TERMINATE_BOOL(debugInfo[0] != 0xff);

    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    delete[] debugInfo;

    std::cout << "\nZello Debug Info PASSED\n";

    return 0;
}
