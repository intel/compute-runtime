/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "sharing_factory.h"

namespace OCLRT {

std::unique_ptr<SharingFactory> SharingFactory::build() {
    std::unique_ptr<SharingFactory> res(new SharingFactory());

    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        res->sharings.push_back(builder->createContextBuilder());
    }

    return res;
}

std::string SharingFactory::getExtensions() {
    std::string res;
    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        res += builder->getExtensions();
    }

    return res;
}

void SharingFactory::fillGlobalDispatchTable() {
    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        builder->fillGlobalDispatchTable();
    }
}

void *SharingFactory::getExtensionFunctionAddress(const std::string &functionName) {
    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        auto ret = builder->getExtensionFunctionAddress(functionName);
        if (ret != nullptr)
            return ret;
    }
    return nullptr;
}

bool SharingFactory::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    for (auto &sharing : sharings) {
        if (sharing->processProperties(propertyType, propertyValue, errcodeRet))
            return true;
    }
    return false;
}

bool SharingFactory::finalizeProperties(Context &context, int32_t &errcodeRet) {
    for (auto &sharing : sharings) {
        if (!sharing->finalizeProperties(context, errcodeRet))
            return false;
    }
    return true;
}

bool SharingFactory::isSharingPresent(SharingType sharingId) {
    return sharingContextBuilder[sharingId] != nullptr;
}

SharingBuilderFactory *SharingFactory::sharingContextBuilder[SharingType::MAX_SHARING_VALUE] = {
    nullptr,
};
SharingFactory sharingFactory;
} // namespace OCLRT
