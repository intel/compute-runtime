/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "offline_compiler/decoder/binary_encoder.h"

struct MockEncoder : public BinaryEncoder {
    MockEncoder() : MockEncoder("", ""){};
    MockEncoder(const std::string &dump, const std::string &elf)
        : BinaryEncoder(dump, elf) {
        setMessagePrinter(MessagePrinter{true});
    };
    using BinaryEncoder::calculatePatchListSizes;
    using BinaryEncoder::copyBinaryToBinary;
    using BinaryEncoder::createElf;
    using BinaryEncoder::elfName;
    using BinaryEncoder::encode;
    using BinaryEncoder::pathToDump;
    using BinaryEncoder::processBinary;
    using BinaryEncoder::processKernel;
    using BinaryEncoder::write;
    using BinaryEncoder::writeDeviceBinary;
};
