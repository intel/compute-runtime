/*
 * Copyright (C) 2020-2026 Intel Corporation
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
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferToBuffer,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferToBufferSrc,
    copyBufferToBufferSrcSize);

static constexpr const char copyBufferToBufferStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyBufferToBufferStatelessSrcSize = sizeof(copyBufferToBufferStatelessSrc);

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferToBufferStateless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferToBufferStatelessSrc,
    copyBufferToBufferStatelessSrcSize);

static constexpr const char copyBufferToBufferStatelessHeaplessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyBufferToBufferStatelessHeaplessSrcSize = sizeof(copyBufferToBufferStatelessHeaplessSrc);

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessHeaplessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferToBufferStatelessHeapless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferToBufferStatelessHeaplessSrc,
    copyBufferToBufferStatelessHeaplessSrcSize);

static constexpr const char copyBufferRectSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
    ;
static constexpr size_t copyBufferRectSrcSize = sizeof(copyBufferRectSrc);

static RegisterEmbeddedResource registerCopyBufferRectSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferRect,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferRectSrc,
    copyBufferRectSrcSize);

static constexpr const char copyBufferRectStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
    ;
static constexpr size_t copyBufferRectStatelessSrcSize = sizeof(copyBufferRectStatelessSrc);

static RegisterEmbeddedResource registerCopyBufferRectStatelessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferRectStateless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferRectStatelessSrc,
    copyBufferRectStatelessSrcSize);

static constexpr const char copyBufferRectStatelessHeaplessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
    ;
static constexpr size_t copyBufferRectStatelessHeaplessSrcSize = sizeof(copyBufferRectStatelessHeaplessSrc);

static RegisterEmbeddedResource registerCopyBufferRectStatelessHeaplessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferRectStatelessHeapless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferRectStatelessHeaplessSrc,
    copyBufferRectStatelessHeaplessSrcSize);

static constexpr const char fillBufferSrc[] =
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
    ;
static constexpr size_t fillBufferSrcSize = sizeof(fillBufferSrc);

static RegisterEmbeddedResource registerFillBufferSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillBuffer,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillBufferSrc,
    fillBufferSrcSize);

static constexpr const char fillBufferStatelessSrc[] =
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
    ;
static constexpr size_t fillBufferStatelessSrcSize = sizeof(fillBufferStatelessSrc);

static RegisterEmbeddedResource registerFillBufferStatelessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillBufferStateless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillBufferStatelessSrc,
    fillBufferStatelessSrcSize);

static constexpr const char fillBufferStatelessHeaplessSrc[] =
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
    ;
static constexpr size_t fillBufferStatelessHeaplessSrcSize = sizeof(fillBufferStatelessHeaplessSrc);

static RegisterEmbeddedResource registerFillBufferStatelessHeaplessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillBufferStatelessHeapless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillBufferStatelessHeaplessSrc,
    fillBufferStatelessHeaplessSrcSize);

static constexpr const char copyBufferToImage3dSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d.builtin_kernel"
    ;
static constexpr size_t copyBufferToImage3dSrcSize = sizeof(copyBufferToImage3dSrc);

static RegisterEmbeddedResource registerCopyBufferToImage3dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferToImage3d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferToImage3dSrc,
    copyBufferToImage3dSrcSize);

static constexpr const char copyBufferToImage3dStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d.builtin_kernel"
    ;
static constexpr size_t copyBufferToImage3dStatelessSrcSize = sizeof(copyBufferToImage3dStatelessSrc);

static RegisterEmbeddedResource registerCopyBufferToImage3dStatelessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyBufferToImage3dStateless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyBufferToImage3dStatelessSrc,
    copyBufferToImage3dStatelessSrcSize);

static constexpr const char copyImage3dToBufferSrc[] =
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyImage3dToBufferSrcSize = sizeof(copyImage3dToBufferSrc);

static RegisterEmbeddedResource registerCopyImage3dToBufferSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyImage3dToBuffer,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyImage3dToBufferSrc,
    copyImage3dToBufferSrcSize);

static constexpr const char copyImage3dToBufferStatelessSrc[] =
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer.builtin_kernel"
    ;
static constexpr size_t copyImage3dToBufferStatelessSrcSize = sizeof(copyImage3dToBufferStatelessSrc);

static RegisterEmbeddedResource registerCopyImage3dToBufferStatelessSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyImage3dToBufferStateless,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyImage3dToBufferStatelessSrc,
    copyImage3dToBufferStatelessSrcSize);

static constexpr const char copyImageToImage1dSrc[] =
#include "shared/source/built_ins/kernels/copy_image_to_image1d.builtin_kernel"
    ;
static constexpr size_t copyImageToImage1dSrcSize = sizeof(copyImageToImage1dSrc);

static RegisterEmbeddedResource registerCopyImageToImage1dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyImageToImage1d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyImageToImage1dSrc,
    copyImageToImage1dSrcSize);

static constexpr const char copyImageToImage2dSrc[] =
#include "shared/source/built_ins/kernels/copy_image_to_image2d.builtin_kernel"
    ;
static constexpr size_t copyImageToImage2dSrcSize = sizeof(copyImageToImage2dSrc);

static RegisterEmbeddedResource registerCopyImageToImage2dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyImageToImage2d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyImageToImage2dSrc,
    copyImageToImage2dSrcSize);

static constexpr const char copyImageToImage3dSrc[] =
#include "shared/source/built_ins/kernels/copy_image_to_image3d.builtin_kernel"
    ;
static constexpr size_t copyImageToImage3dSrcSize = sizeof(copyImageToImage3dSrc);

static RegisterEmbeddedResource registerCopyImageToImage3dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::copyImageToImage3d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyImageToImage3dSrc,
    copyImageToImage3dSrcSize);

static constexpr const char fillImage1dSrc[] =
#include "shared/source/built_ins/kernels/fill_image1d.builtin_kernel"
    ;
static constexpr size_t fillImage1dSrcSize = sizeof(fillImage1dSrc);

static RegisterEmbeddedResource registerFillImage1dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillImage1d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillImage1dSrc,
    fillImage1dSrcSize);

static constexpr const char fillImage2dSrc[] =
#include "shared/source/built_ins/kernels/fill_image2d.builtin_kernel"
    ;
static constexpr size_t fillImage2dSrcSize = sizeof(fillImage2dSrc);

static RegisterEmbeddedResource registerFillImage2dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillImage2d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillImage2dSrc,
    fillImage2dSrcSize);

static constexpr const char fillImage3dSrc[] =
#include "shared/source/built_ins/kernels/fill_image3d.builtin_kernel"
    ;
static constexpr size_t fillImage3dSrcSize = sizeof(fillImage3dSrc);

static RegisterEmbeddedResource registerFillImage3dSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillImage3d,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillImage3dSrc,
    fillImage3dSrcSize);

static constexpr const char auxTranslationSrc[] =
#include "shared/source/built_ins/kernels/aux_translation.builtin_kernel"
    ;
static constexpr size_t auxTranslationSrcSize = sizeof(auxTranslationSrc);

static RegisterEmbeddedResource registerAuxTranslationSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::auxTranslation,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    auxTranslationSrc,
    auxTranslationSrcSize);

static constexpr const char copyKernelTimestampsSrc[] =
#include "shared/source/built_ins/kernels/copy_kernel_timestamps.builtin_kernel"
    ;
static constexpr size_t copyKernelTimestampsSrcSize = sizeof(copyKernelTimestampsSrc);

static RegisterEmbeddedResource registerCopyKernelTimestampsSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::queryKernelTimestamps,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    copyKernelTimestampsSrc,
    copyKernelTimestampsSrcSize);

static constexpr const char fillImage1dBufferSrc[] =
#include "shared/source/built_ins/kernels/fill_image1d_buffer.builtin_kernel"
    ;
static constexpr size_t fillImage1dBufferSrcSize = sizeof(fillImage1dBufferSrc);

static RegisterEmbeddedResource registerFillImage1dBufferSrc(
    BuiltIn::createResourceName(
        BuiltIn::Group::fillImage1dBuffer,
        BuiltIn::Code::getExtension(BuiltIn::CodeType::source))
        .c_str(),
    fillImage1dBufferSrc,
    fillImage1dBufferSrcSize);

} // namespace NEO
