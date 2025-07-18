/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/rtas/rtas.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

const std::string zeRTASBuilderCreateExpImpl = "zeRTASBuilderCreateExpImpl";
const std::string zeRTASBuilderDestroyExpImpl = "zeRTASBuilderDestroyExpImpl";
const std::string zeRTASBuilderGetBuildPropertiesExpImpl = "zeRTASBuilderGetBuildPropertiesExpImpl";
const std::string zeRTASBuilderBuildExpImpl = "zeRTASBuilderBuildExpImpl";
const std::string zeDriverRTASFormatCompatibilityCheckExpImpl = "zeDriverRTASFormatCompatibilityCheckExpImpl";
const std::string zeRTASParallelOperationCreateExpImpl = "zeRTASParallelOperationCreateExpImpl";
const std::string zeRTASParallelOperationDestroyExpImpl = "zeRTASParallelOperationDestroyExpImpl";
const std::string zeRTASParallelOperationGetPropertiesExpImpl = "zeRTASParallelOperationGetPropertiesExpImpl";
const std::string zeRTASParallelOperationJoinExpImpl = "zeRTASParallelOperationJoinExpImpl";

pRTASBuilderCreateExpImpl builderCreateExpImpl;
pRTASBuilderDestroyExpImpl builderDestroyExpImpl;
pRTASBuilderGetBuildPropertiesExpImpl builderGetBuildPropertiesExpImpl;
pRTASBuilderBuildExpImpl builderBuildExpImpl;
pDriverRTASFormatCompatibilityCheckExpImpl formatCompatibilityCheckExpImpl;
pRTASParallelOperationCreateExpImpl parallelOperationCreateExpImpl;
pRTASParallelOperationDestroyExpImpl parallelOperationDestroyExpImpl;
pRTASParallelOperationGetPropertiesExpImpl parallelOperationGetPropertiesExpImpl;
pRTASParallelOperationJoinExpImpl parallelOperationJoinExpImpl;
// RTAS Extension function pointers
const std::string zeRTASBuilderCreateExtImpl = "zeRTASBuilderCreateExtImpl";
const std::string zeRTASBuilderDestroyExtImpl = "zeRTASBuilderDestroyExtImpl";
const std::string zeRTASBuilderGetBuildPropertiesExtImpl = "zeRTASBuilderGetBuildPropertiesExtImpl";
const std::string zeRTASBuilderBuildExtImpl = "zeRTASBuilderBuildExtImpl";
const std::string zeDriverRTASFormatCompatibilityCheckExtImpl = "zeDriverRTASFormatCompatibilityCheckExtImpl";
const std::string zeRTASParallelOperationCreateExtImpl = "zeRTASParallelOperationCreateExtImpl";
const std::string zeRTASParallelOperationDestroyExtImpl = "zeRTASParallelOperationDestroyExtImpl";
const std::string zeRTASParallelOperationGetPropertiesExtImpl = "zeRTASParallelOperationGetPropertiesExtImpl";
const std::string zeRTASParallelOperationJoinExtImpl = "zeRTASParallelOperationJoinExtImpl";

pRTASBuilderCreateExtImpl builderCreateExtImpl;
pRTASBuilderDestroyExtImpl builderDestroyExtImpl;
pDriverRTASFormatCompatibilityCheckExtImpl formatCompatibilityCheckExtImpl;
pRTASBuilderGetBuildPropertiesExtImpl builderGetBuildPropertiesExtImpl;
pRTASBuilderBuildExtImpl builderBuildExtImpl;
pRTASParallelOperationCreateExtImpl parallelOperationCreateExtImpl;
pRTASParallelOperationDestroyExtImpl parallelOperationDestroyExtImpl;
pRTASParallelOperationGetPropertiesExtImpl parallelOperationGetPropertiesExtImpl;
pRTASParallelOperationJoinExtImpl parallelOperationJoinExtImpl;

bool RTASBuilder::loadEntryPoints(NEO::OsLibrary *libraryHandle) {
    bool ok = getSymbolAddr(libraryHandle, zeRTASBuilderCreateExpImpl, builderCreateExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASBuilderDestroyExpImpl, builderDestroyExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASBuilderGetBuildPropertiesExpImpl, builderGetBuildPropertiesExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASBuilderBuildExpImpl, builderBuildExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeDriverRTASFormatCompatibilityCheckExpImpl, formatCompatibilityCheckExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationCreateExpImpl, parallelOperationCreateExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationDestroyExpImpl, parallelOperationDestroyExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationGetPropertiesExpImpl, parallelOperationGetPropertiesExpImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationJoinExpImpl, parallelOperationJoinExpImpl);
    return ok;
}

bool RTASBuilderExt::loadEntryPoints(NEO::OsLibrary *libraryHandle) {
    bool ok = getSymbolAddr(libraryHandle, zeRTASBuilderCreateExtImpl, builderCreateExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASBuilderDestroyExtImpl, builderDestroyExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASBuilderGetBuildPropertiesExtImpl, builderGetBuildPropertiesExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASBuilderBuildExtImpl, builderBuildExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeDriverRTASFormatCompatibilityCheckExtImpl, formatCompatibilityCheckExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationCreateExtImpl, parallelOperationCreateExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationDestroyExtImpl, parallelOperationDestroyExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationGetPropertiesExtImpl, parallelOperationGetPropertiesExtImpl);
    ok = ok && getSymbolAddr(libraryHandle, zeRTASParallelOperationJoinExtImpl, parallelOperationJoinExtImpl);
    return ok;
}

ze_result_t RTASBuilder::getProperties(const ze_rtas_builder_build_op_exp_desc_t *args,
                                       ze_rtas_builder_exp_properties_t *pProp) {
    return builderGetBuildPropertiesExpImpl(this->handleImpl, args, pProp);
}

ze_result_t RTASBuilderExt::getProperties(const ze_rtas_builder_build_op_ext_desc_t *args,
                                          ze_rtas_builder_ext_properties_t *pProp) {
    return builderGetBuildPropertiesExtImpl(this->handleImpl, args, pProp);
}

ze_result_t RTASBuilder::build(const ze_rtas_builder_build_op_exp_desc_t *args,
                               void *pScratchBuffer, size_t scratchBufferSizeBytes,
                               void *pRtasBuffer, size_t rtasBufferSizeBytes,
                               ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                               void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                               size_t *pRtasBufferSizeBytes) {
    RTASParallelOperation *parallelOperation = RTASParallelOperation::fromHandle(hParallelOperation);

    return builderBuildExpImpl(this->handleImpl,
                               args,
                               pScratchBuffer, scratchBufferSizeBytes,
                               pRtasBuffer, rtasBufferSizeBytes,
                               parallelOperation->handleImpl,
                               pBuildUserPtr, pBounds,
                               pRtasBufferSizeBytes);
}

ze_result_t RTASBuilderExt::build(const ze_rtas_builder_build_op_ext_desc_t *args,
                                  void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                  void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                  ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                  void *pBuildUserPtr, ze_rtas_aabb_ext_t *pBounds,
                                  size_t *pRtasBufferSizeBytes) {
    RTASParallelOperationExt *parallelOperation = RTASParallelOperationExt::fromHandle(hParallelOperation);

    return builderBuildExtImpl(this->handleImpl,
                               args,
                               pScratchBuffer, scratchBufferSizeBytes,
                               pRtasBuffer, rtasBufferSizeBytes,
                               parallelOperation->handleImpl,
                               pBuildUserPtr, pBounds,
                               pRtasBufferSizeBytes);
}

ze_result_t DriverHandleImp::formatRTASCompatibilityCheck(ze_rtas_format_exp_t rtasFormatA,
                                                          ze_rtas_format_exp_t rtasFormatB) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    return formatCompatibilityCheckExpImpl(this->toHandle(), rtasFormatA, rtasFormatB);
}

ze_result_t DriverHandleImp::formatRTASCompatibilityCheckExt(ze_rtas_format_ext_t rtasFormatA,
                                                             ze_rtas_format_ext_t rtasFormatB) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    return formatCompatibilityCheckExtImpl(this->toHandle(), rtasFormatA, rtasFormatB);
}

ze_result_t DriverHandleImp::createRTASBuilder(const ze_rtas_builder_exp_desc_t *desc,
                                               ze_rtas_builder_exp_handle_t *phBuilder) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto pRTASBuilder = std::make_unique<RTASBuilder>();

    result = builderCreateExpImpl(this->toHandle(), desc, &pRTASBuilder->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *phBuilder = pRTASBuilder.release();
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::createRTASBuilderExt(const ze_rtas_builder_ext_desc_t *desc,
                                                  ze_rtas_builder_ext_handle_t *phBuilder) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto pRTASBuilder = std::make_unique<RTASBuilderExt>();

    result = builderCreateExtImpl(this->toHandle(), desc, &pRTASBuilder->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *phBuilder = pRTASBuilder.release();
    return ZE_RESULT_SUCCESS;
}

ze_result_t RTASBuilder::destroy() {
    ze_result_t result = builderDestroyExpImpl(this->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RTASBuilderExt::destroy() {
    ze_result_t result = builderDestroyExtImpl(this->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::loadRTASLibrary() {
    std::lock_guard<std::mutex> lock(this->rtasLock);

    if (this->rtasLibraryUnavailable == true) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    if (this->rtasLibraryHandle == nullptr) {
        this->rtasLibraryHandle = std::unique_ptr<NEO::OsLibrary>(NEO::OsLibrary::loadFunc(L0::rtasLibraryName));
        bool rtasExpAvailable = false;
        bool rtasExtAvailable = false;
        if (this->rtasLibraryHandle != nullptr) {
            rtasExpAvailable = RTASBuilder::loadEntryPoints(this->rtasLibraryHandle.get());
            rtasExtAvailable = RTASBuilderExt::loadEntryPoints(this->rtasLibraryHandle.get());
        }
        if (this->rtasLibraryHandle == nullptr || (!rtasExpAvailable && !rtasExtAvailable)) {
            this->rtasLibraryUnavailable = true;

            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to load Ray Tracing Support Library %s\n", L0::rtasLibraryName.c_str());
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::createRTASParallelOperation(ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto pRTASParallelOperation = std::make_unique<RTASParallelOperation>();

    result = parallelOperationCreateExpImpl(this->toHandle(), &pRTASParallelOperation->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *phParallelOperation = pRTASParallelOperation.release();
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::createRTASParallelOperationExt(ze_rtas_parallel_operation_ext_handle_t *phParallelOperation) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto pRTASParallelOperation = std::make_unique<RTASParallelOperationExt>();

    result = parallelOperationCreateExtImpl(this->toHandle(), &pRTASParallelOperation->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *phParallelOperation = pRTASParallelOperation.release();
    return ZE_RESULT_SUCCESS;
}

ze_result_t RTASParallelOperation::destroy() {
    ze_result_t result = parallelOperationDestroyExpImpl(this->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RTASParallelOperationExt::destroy() {
    ze_result_t result = parallelOperationDestroyExtImpl(this->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RTASParallelOperation::getProperties(ze_rtas_parallel_operation_exp_properties_t *pProperties) {
    return parallelOperationGetPropertiesExpImpl(this->handleImpl, pProperties);
}

ze_result_t RTASParallelOperationExt::getProperties(ze_rtas_parallel_operation_ext_properties_t *pProperties) {
    return parallelOperationGetPropertiesExtImpl(this->handleImpl, pProperties);
}

ze_result_t RTASParallelOperation::join() {
    return parallelOperationJoinExpImpl(this->handleImpl);
}

ze_result_t RTASParallelOperationExt::join() {
    return parallelOperationJoinExtImpl(this->handleImpl);
}

} // namespace L0
