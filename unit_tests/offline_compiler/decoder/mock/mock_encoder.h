/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "offline_compiler/decoder/binary_encoder.h"
#include "runtime/helpers/hash.h"

#include <map>
#include <string>

struct MockEncoder : public BinaryEncoder {
    MockEncoder() : MockEncoder("", ""){};
    MockEncoder(const std::string &dump, const std::string &elf)
        : BinaryEncoder(dump, elf) {
        setMessagePrinter(MessagePrinter{true});
    };

    bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength) override {
        auto it = filesMap.find(srcFileName);
        if (it == filesMap.end()) {
            return false;
        }
        outBinary.write(it->second.c_str(), it->second.size());
        if (binaryLength != nullptr) {
            *binaryLength = static_cast<uint32_t>(it->second.size());
        }
        return true;
    }

    using BinaryEncoder::addPadding;
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

    std::map<std::string, std::string> filesMap;
};
