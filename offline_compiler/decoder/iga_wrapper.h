/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

#include <memory>
#include <string>

class MessagePrinter;

struct IgaWrapper {
    IgaWrapper();
    MOCKABLE_VIRTUAL ~IgaWrapper();

    IgaWrapper(IgaWrapper &) = delete;
    IgaWrapper(const IgaWrapper &&) = delete;
    IgaWrapper &operator=(const IgaWrapper &) = delete;
    IgaWrapper &operator=(IgaWrapper &&) = delete;

    MOCKABLE_VIRTUAL bool tryDisassembleGenISA(const void *kernelPtr, uint32_t kernelSize, std::string &out);
    MOCKABLE_VIRTUAL bool tryAssembleGenISA(const std::string &inAsm, std::string &outBinary);

    MOCKABLE_VIRTUAL void setGfxCore(GFXCORE_FAMILY core);
    MOCKABLE_VIRTUAL void setProductFamily(PRODUCT_FAMILY product);
    MOCKABLE_VIRTUAL bool isKnownPlatform() const;
    void setMessagePrinter(MessagePrinter &messagePrinter) {
        this->messagePrinter = &messagePrinter;
    }

  protected:
    MOCKABLE_VIRTUAL bool tryLoadIga();

    struct Impl;
    std::unique_ptr<Impl> pimpl;

    MessagePrinter *messagePrinter = nullptr;
};
