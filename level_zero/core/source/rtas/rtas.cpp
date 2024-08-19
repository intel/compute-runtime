/*
 * Copyright (C) 2023-2024 Intel Corporation
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

ze_result_t RTASBuilder::getProperties(const ze_rtas_builder_build_op_exp_desc_t *args,
                                       ze_rtas_builder_exp_properties_t *pProp) {
    return builderGetBuildPropertiesExpImpl(this->handleImpl, args, pProp);
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

ze_result_t DriverHandleImp::formatRTASCompatibilityCheck(ze_rtas_format_exp_t rtasFormatA,
                                                          ze_rtas_format_exp_t rtasFormatB) {
    ze_result_t result = this->loadRTASLibrary();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    return formatCompatibilityCheckExpImpl(this->toHandle(), rtasFormatA, rtasFormatB);
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

ze_result_t RTASBuilder::destroy() {
    ze_result_t result = builderDestroyExpImpl(this->handleImpl);
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
        this->rtasLibraryHandle = std::unique_ptr<NEO::OsLibrary>(NEO::OsLibrary::loadFunc(RTASBuilder::rtasLibraryName));
        if (this->rtasLibraryHandle == nullptr || RTASBuilder::loadEntryPoints(this->rtasLibraryHandle.get()) == false) {
            this->rtasLibraryUnavailable = true;

            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to load Ray Tracing Support Library %s\n", RTASBuilder::rtasLibraryName.c_str());
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

ze_result_t RTASParallelOperation::destroy() {
    ze_result_t result = parallelOperationDestroyExpImpl(this->handleImpl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RTASParallelOperation::getProperties(ze_rtas_parallel_operation_exp_properties_t *pProperties) {
    return parallelOperationGetPropertiesExpImpl(this->handleImpl, pProperties);
}

ze_result_t RTASParallelOperation::join() {
    return parallelOperationJoinExpImpl(this->handleImpl);
}

} // namespace L0
