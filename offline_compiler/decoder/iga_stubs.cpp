/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "helper.h"
#include "iga_wrapper.h"

struct IgaWrapper::Impl {
};

IgaWrapper::IgaWrapper() = default;
IgaWrapper::~IgaWrapper() = default;

bool IgaWrapper::tryDisassembleGenISA(const void *kernelPtr, uint32_t kernelSize, std::string &out) {
    messagePrinter->printf("Warning: ocloc built without support for IGA - kernel binaries won't be disassembled.\n");
    return false;
}

bool IgaWrapper::tryAssembleGenISA(const std::string &inAsm, std::string &outBinary) {
    messagePrinter->printf("Warning: ocloc built without support for IGA - kernel binaries won't be assembled.\n");
    return false;
}

bool IgaWrapper::tryLoadIga() {
    return false;
}

void IgaWrapper::setGfxCore(GFXCORE_FAMILY core) {
}

void IgaWrapper::setProductFamily(PRODUCT_FAMILY product) {
}

bool IgaWrapper::isKnownPlatform() const {
    return false;
}
