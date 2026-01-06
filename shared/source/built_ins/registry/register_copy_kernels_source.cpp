/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/registry/built_ins_registry.h"

#include <cstddef>

namespace NEO {

static constexpr const char copyBufferToBufferSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyBufferToBufferSrcSize = sizeof(copyBufferToBufferSrc);

static RegisterEmbeddedResource registerCopyBufferToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferToBufferSrc,
    copyBufferToBufferSrcSize);

static constexpr const char copyBufferToBufferStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyBufferToBufferStatelessSrcSize = sizeof(copyBufferToBufferStatelessSrc);

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferToBufferStatelessSrc,
    copyBufferToBufferStatelessSrcSize);

static constexpr const char copyBufferToBufferStatelessHeaplessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyBufferToBufferStatelessHeaplessSrcSize = sizeof(copyBufferToBufferStatelessHeaplessSrc);

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessHeaplessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToBufferStatelessHeapless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferToBufferStatelessHeaplessSrc,
    copyBufferToBufferStatelessHeaplessSrcSize);

static constexpr const char copyBufferRectSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
    ;
static constexpr size_t copyBufferRectSrcSize = sizeof(copyBufferRectSrc);

static RegisterEmbeddedResource registerCopyBufferRectSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferRect,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferRectSrc,
    copyBufferRectSrcSize);

static constexpr const char copyBufferRectStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
    ;
static constexpr size_t copyBufferRectStatelessSrcSize = sizeof(copyBufferRectStatelessSrc);

static RegisterEmbeddedResource registerCopyBufferRectStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferRectStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferRectStatelessSrc,
    copyBufferRectStatelessSrcSize);

static constexpr const char copyBufferRectStatelessHeaplessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
    ;
static constexpr size_t copyBufferRectStatelessHeaplessSrcSize = sizeof(copyBufferRectStatelessHeaplessSrc);

static RegisterEmbeddedResource registerCopyBufferRectStatelessHeaplessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferRectStatelessHeapless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferRectStatelessHeaplessSrc,
    copyBufferRectStatelessHeaplessSrcSize);

static constexpr const char fillBufferSrc[] =
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
    ;
static constexpr size_t fillBufferSrcSize = sizeof(fillBufferSrc);

static RegisterEmbeddedResource registerFillBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillBufferSrc,
    fillBufferSrcSize);

static constexpr const char fillBufferStatelessSrc[] =
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
    ;
static constexpr size_t fillBufferStatelessSrcSize = sizeof(fillBufferStatelessSrc);

static RegisterEmbeddedResource registerFillBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillBufferStatelessSrc,
    fillBufferStatelessSrcSize);

static constexpr const char fillBufferStatelessHeaplessSrc[] =
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
    ;
static constexpr size_t fillBufferStatelessHeaplessSrcSize = sizeof(fillBufferStatelessHeaplessSrc);

static RegisterEmbeddedResource registerFillBufferStatelessHeaplessSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillBufferStatelessHeapless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillBufferStatelessHeaplessSrc,
    fillBufferStatelessHeaplessSrcSize);

static constexpr const char copyBufferToImage3dSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d.builtin_kernel"
    ;
static constexpr size_t copyBufferToImage3dSrcSize = sizeof(copyBufferToImage3dSrc);

static RegisterEmbeddedResource registerCopyBufferToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferToImage3dSrc,
    copyBufferToImage3dSrcSize);

static constexpr const char copyBufferToImage3dStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d.builtin_kernel"
    ;
static constexpr size_t copyBufferToImage3dStatelessSrcSize = sizeof(copyBufferToImage3dStatelessSrc);

static RegisterEmbeddedResource registerCopyBufferToImage3dStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToImage3dStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyBufferToImage3dStatelessSrc,
    copyBufferToImage3dStatelessSrcSize);

static constexpr const char copyImage3dToBufferSrc[] =
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyImage3dToBufferSrcSize = sizeof(copyImage3dToBufferSrc);

static RegisterEmbeddedResource registerCopyImage3dToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImage3dToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyImage3dToBufferSrc,
    copyImage3dToBufferSrcSize);

static constexpr const char copyImage3dToBufferStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyImage3dToBufferStatelessSrcSize = sizeof(copyImage3dToBufferStatelessSrc);

static RegisterEmbeddedResource registerCopyImage3dToBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImage3dToBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyImage3dToBufferStatelessSrc,
    copyImage3dToBufferStatelessSrcSize);

static constexpr const char copyImageToImage1dSrc[] =
#include "shared/source/built_ins/kernels/copy_image_to_image1d.builtin_kernel"
    ;
static constexpr size_t copyImageToImage1dSrcSize = sizeof(copyImageToImage1dSrc);

static RegisterEmbeddedResource registerCopyImageToImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImageToImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyImageToImage1dSrc,
    copyImageToImage1dSrcSize);

static constexpr const char copyImageToImage2dSrc[] =
#include "shared/source/built_ins/kernels/copy_image_to_image2d.builtin_kernel"
    ;
static constexpr size_t copyImageToImage2dSrcSize = sizeof(copyImageToImage2dSrc);

static RegisterEmbeddedResource registerCopyImageToImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImageToImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyImageToImage2dSrc,
    copyImageToImage2dSrcSize);

static constexpr const char copyImageToImage3dSrc[] =
#include "shared/source/built_ins/kernels/copy_image_to_image3d.builtin_kernel"
    ;
static constexpr size_t copyImageToImage3dSrcSize = sizeof(copyImageToImage3dSrc);

static RegisterEmbeddedResource registerCopyImageToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImageToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyImageToImage3dSrc,
    copyImageToImage3dSrcSize);

static constexpr const char fillImage1dSrc[] =
#include "shared/source/built_ins/kernels/fill_image1d.builtin_kernel"
    ;
static constexpr size_t fillImage1dSrcSize = sizeof(fillImage1dSrc);

static RegisterEmbeddedResource registerFillImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillImage1dSrc,
    fillImage1dSrcSize);

static constexpr const char fillImage2dSrc[] =
#include "shared/source/built_ins/kernels/fill_image2d.builtin_kernel"
    ;
static constexpr size_t fillImage2dSrcSize = sizeof(fillImage2dSrc);

static RegisterEmbeddedResource registerFillImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillImage2dSrc,
    fillImage2dSrcSize);

static constexpr const char fillImage3dSrc[] =
#include "shared/source/built_ins/kernels/fill_image3d.builtin_kernel"
    ;
static constexpr size_t fillImage3dSrcSize = sizeof(fillImage3dSrc);

static RegisterEmbeddedResource registerFillImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillImage3dSrc,
    fillImage3dSrcSize);

static constexpr const char auxTranslationSrc[] =
#include "shared/source/built_ins/kernels/aux_translation.builtin_kernel"
    ;
static constexpr size_t auxTranslationSrcSize = sizeof(auxTranslationSrc);

static RegisterEmbeddedResource registerAuxTranslationSrc(
    createBuiltinResourceName(
        EBuiltInOps::auxTranslation,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    auxTranslationSrc,
    auxTranslationSrcSize);

static constexpr const char copyKernelTimestampsSrc[] =
#include "shared/source/built_ins/kernels/copy_kernel_timestamps.builtin_kernel"
    ;
static constexpr size_t copyKernelTimestampsSrcSize = sizeof(copyKernelTimestampsSrc);

static RegisterEmbeddedResource registerCopyKernelTimestampsSrc(
    createBuiltinResourceName(
        EBuiltInOps::queryKernelTimestamps,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    copyKernelTimestampsSrc,
    copyKernelTimestampsSrcSize);

static constexpr const char fillImage1dBufferSrc[] =
#include "shared/source/built_ins/kernels/fill_image1d_buffer.builtin_kernel"
    ;
static constexpr size_t fillImage1dBufferSrcSize = sizeof(fillImage1dBufferSrc);

static RegisterEmbeddedResource registerFillImage1dBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage1dBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    fillImage1dBufferSrc,
    fillImage1dBufferSrcSize);

} // namespace NEO
