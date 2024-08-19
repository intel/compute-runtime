/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include <level_zero/ze_api.h>

#include <string>

struct _ze_rtas_builder_exp_handle_t {};
struct _ze_rtas_parallel_operation_exp_handle_t {};

namespace L0 {
/*
 * Note: RTAS Library using same headers as Level Zero, but using function
 * pointers to access these symbols in the external Library.
 */
typedef ze_result_t (*pRTASBuilderCreateExpImpl)(ze_driver_handle_t hDriver,
                                                 const ze_rtas_builder_exp_desc_t *pDescriptor,
                                                 ze_rtas_builder_exp_handle_t *phBuilder);

typedef ze_result_t (*pRTASBuilderDestroyExpImpl)(ze_rtas_builder_exp_handle_t hBuilder);

typedef ze_result_t (*pRTASBuilderGetBuildPropertiesExpImpl)(ze_rtas_builder_exp_handle_t hBuilder,
                                                             const ze_rtas_builder_build_op_exp_desc_t *args,
                                                             ze_rtas_builder_exp_properties_t *pProp);

typedef ze_result_t (*pRTASBuilderBuildExpImpl)(ze_rtas_builder_exp_handle_t hBuilder,
                                                const ze_rtas_builder_build_op_exp_desc_t *args,
                                                void *pScratchBuffer, size_t scratchBufferSizeBytes,
                                                void *pRtasBuffer, size_t rtasBufferSizeBytes,
                                                ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                                                size_t *pRtasBufferSizeBytes);

typedef ze_result_t (*pDriverRTASFormatCompatibilityCheckExpImpl)(ze_driver_handle_t hDriver,
                                                                  const ze_rtas_format_exp_t accelFormat,
                                                                  const ze_rtas_format_exp_t otherAccelFormat);

typedef ze_result_t (*pRTASParallelOperationCreateExpImpl)(ze_driver_handle_t hDriver,
                                                           ze_rtas_parallel_operation_exp_handle_t *phParallelOperation);

typedef ze_result_t (*pRTASParallelOperationDestroyExpImpl)(ze_rtas_parallel_operation_exp_handle_t hParallelOperation);

typedef ze_result_t (*pRTASParallelOperationGetPropertiesExpImpl)(ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                                  ze_rtas_parallel_operation_exp_properties_t *pProperties);

typedef ze_result_t (*pRTASParallelOperationJoinExpImpl)(ze_rtas_parallel_operation_exp_handle_t hParallelOperation);

extern pRTASBuilderCreateExpImpl builderCreateExpImpl;
extern pRTASBuilderDestroyExpImpl builderDestroyExpImpl;
extern pRTASBuilderGetBuildPropertiesExpImpl builderGetBuildPropertiesExpImpl;
extern pRTASBuilderBuildExpImpl builderBuildExpImpl;
extern pDriverRTASFormatCompatibilityCheckExpImpl formatCompatibilityCheckExpImpl;
extern pRTASParallelOperationCreateExpImpl parallelOperationCreateExpImpl;
extern pRTASParallelOperationDestroyExpImpl parallelOperationDestroyExpImpl;
extern pRTASParallelOperationGetPropertiesExpImpl parallelOperationGetPropertiesExpImpl;
extern pRTASParallelOperationJoinExpImpl parallelOperationJoinExpImpl;

struct RTASBuilder : _ze_rtas_builder_exp_handle_t {
  public:
    virtual ~RTASBuilder() = default;

    ze_result_t destroy();
    ze_result_t getProperties(const ze_rtas_builder_build_op_exp_desc_t *args,
                              ze_rtas_builder_exp_properties_t *pProp);
    ze_result_t build(const ze_rtas_builder_build_op_exp_desc_t *args,
                      void *pScratchBuffer, size_t scratchBufferSizeBytes,
                      void *pRtasBuffer, size_t rtasBufferSizeBytes,
                      ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                      void *pBuildUserPtr, ze_rtas_aabb_exp_t *pBounds,
                      size_t *pRtasBufferSizeBytes);

    static RTASBuilder *fromHandle(ze_rtas_builder_exp_handle_t handle) { return static_cast<RTASBuilder *>(handle); }
    inline ze_rtas_builder_exp_handle_t toHandle() { return this; }

    static std::string rtasLibraryName;
    static bool loadEntryPoints(NEO::OsLibrary *libraryHandle);

    template <class T>
    static bool getSymbolAddr(NEO::OsLibrary *libraryHandle, const std::string name, T &proc) {
        void *addr = libraryHandle->getProcAddress(name);
        proc = reinterpret_cast<T>(addr);
        return nullptr != proc;
    }

    ze_rtas_builder_exp_handle_t handleImpl;
};

struct RTASParallelOperation : _ze_rtas_parallel_operation_exp_handle_t {
  public:
    virtual ~RTASParallelOperation() = default;

    ze_result_t destroy();
    ze_result_t getProperties(ze_rtas_parallel_operation_exp_properties_t *pProperties);
    ze_result_t join();

    static RTASParallelOperation *fromHandle(ze_rtas_parallel_operation_exp_handle_t handle) { return static_cast<RTASParallelOperation *>(handle); }
    inline ze_rtas_parallel_operation_exp_handle_t toHandle() { return this; }

    ze_rtas_parallel_operation_exp_handle_t handleImpl;
};

} // namespace L0
