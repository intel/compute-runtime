/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

namespace L0 {
namespace ult {

std::string testXmlBuffer(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<testElement>\n"
    "    <testSubElement prop=\"A\">\n"
    "        <testSubSubElement id=\"one\">\n"
    "            <testEmptyElement attribOne=\"valueOne\" attribTwo=\"valueTwo\"/>\n"
    "            <testTextElement>text one</testTextElement>\n"
    "        </testSubSubElement>\n"
    "        <testSubSubElement id=\"two\">\n"
    "            <testTextElement>text two</testTextElement>\n"
    "            <testNumericElement>2</testNumericElement>\n"
    "        </testSubSubElement>\n"
    "        <testSubSubElement id=\"three\">\n"
    "            <testTextElement>text three</testTextElement>\n"
    "            <testNumericElement>3</testNumericElement>\n"
    "        </testSubSubElement>\n"
    "    </testSubElement>\n"
    "    <testSubElement prop=\"B\">\n"
    "        <testSubSubElement id=\"four\">\n"
    "            <testEmptyElement/>\n"
    "        </testSubSubElement>\n"
    "    </testSubElement>\n"
    "</testElement>\n");

} // namespace ult
} // namespace L0
