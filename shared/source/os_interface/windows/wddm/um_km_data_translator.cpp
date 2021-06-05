/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"

#include "shared/source/helpers/string.h"

namespace NEO {

size_t UmKmDataTranslator::getSizeForAdapterInfoInternalRepresentation() {
    return sizeof(ADAPTER_INFO);
}

bool UmKmDataTranslator::translateAdapterInfoFromInternalRepresentation(ADAPTER_INFO_KMD &dst, const void *src, size_t srcSize) {
    return (0 == memcpy_s(&dst, sizeof(ADAPTER_INFO), src, srcSize));
}

size_t UmKmDataTranslator::getSizeForCreateContextDataInternalRepresentation() {
    return sizeof(CREATECONTEXT_PVTDATA);
}

bool UmKmDataTranslator::translateCreateContextDataToInternalRepresentation(void *dst, size_t dstSize, const CREATECONTEXT_PVTDATA &src) {
    return (0 == memcpy_s(dst, dstSize, &src, sizeof(CREATECONTEXT_PVTDATA)));
}

size_t UmKmDataTranslator::getSizeForCommandBufferHeaderDataInternalRepresentation() {
    return sizeof(COMMAND_BUFFER_HEADER);
}

bool UmKmDataTranslator::tranlateCommandBufferHeaderDataToInternalRepresentation(void *dst, size_t dstSize, const COMMAND_BUFFER_HEADER &src) {
    return (0 == memcpy_s(dst, dstSize, &src, sizeof(COMMAND_BUFFER_HEADER)));
}

} // namespace NEO
