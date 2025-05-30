/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "neo_igfxfmid.h"

#include <cstdint>
#include <memory>
#include <string>

class MessagePrinter;

struct IgaWrapper : NEO::NonCopyableAndNonMovableClass {
    IgaWrapper();
    MOCKABLE_VIRTUAL ~IgaWrapper();

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

static_assert(NEO::NonCopyableAndNonMovable<IgaWrapper>);
