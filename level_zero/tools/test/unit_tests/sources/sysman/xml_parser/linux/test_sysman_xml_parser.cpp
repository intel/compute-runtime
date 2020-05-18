/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/linux/xml_parser/xml_parser.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gtest/gtest.h"
#include "sample_xml.h"

namespace L0 {
namespace ult {

class SysmanXmlParserFixture : public ::testing::Test {

  protected:
    XmlParser *pXmlParser = nullptr;
    XmlDoc *pXmlDoc = nullptr;

    void SetUp() override {
        pXmlParser = XmlParser::create();

        if (nullptr == pXmlParser) {
            GTEST_SKIP();
        }

        pXmlDoc = pXmlParser->parseBuffer(testXmlBuffer);
        ASSERT_NE(pXmlDoc, nullptr);
    }
    void TearDown() override {
        if (nullptr != pXmlDoc) {
            delete pXmlDoc;
            pXmlDoc = nullptr;
        }
        if (nullptr != pXmlParser) {
            delete pXmlParser;
            pXmlParser = nullptr;
        }
    }
};
TEST_F(SysmanXmlParserFixture, GivenValidXPathWhenCallingXmlDocXPathWithValidXmlDocVerifyXPathReturnsCorrectXmlNodes) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("/testElement/testSubElement"));
    EXPECT_EQ(xmlNodes.size(), 2U);
    for (auto pXmlNode : xmlNodes) {
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenValidXPathWhenCallingXmlNodeXPathWithValidXmlNodeVerifyXPathReturnsCorrectXmlNodes) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("/testElement/testSubElement[@prop='A']"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::vector<XmlNode *> subXmlNodes = pXmlNode->xPath(std::string("testSubSubElement"));
        EXPECT_EQ(subXmlNodes.size(), 3U);
        for (auto pSubXmlNode : subXmlNodes) {
            delete pSubXmlNode;
        }
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenValidXmlodeWhenCallingXmlNodeGetTextVerifyGetTextReturnsCorrectText) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("/testElement/testSubElement[@prop='A']"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::vector<XmlNode *> subXmlNodes = pXmlNode->xPath(std::string("testSubSubElement/testTextElement[contains(text(),'two')]"));
        EXPECT_EQ(subXmlNodes.size(), 1U);
        for (auto pSubXmlNode : subXmlNodes) {
            std::string text = pSubXmlNode->getText();
            EXPECT_STREQ(text.c_str(), "text two");
            delete pSubXmlNode;
        }
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenEmptyXmlNodeWhenCallingXmlNodeGetTextVerifyGetTextReturnsEmptyString) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/testEmptyElement"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::string text = pXmlNode->getText();
        EXPECT_EQ(0U, text.length());
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenValidAttributeWhenCallingXmlNodeGetAttributeWithValidNodeVerifyXmlNodeGetAttributeReturnsCorrectString) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/testEmptyElement"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::string attrib;
        attrib = pXmlNode->getAttribute(std::string("attribOne"));
        EXPECT_STREQ(attrib.c_str(), "valueOne");
        attrib = pXmlNode->getAttribute(std::string("attribTwo"));
        EXPECT_STREQ(attrib.c_str(), "valueTwo");
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenValidXmlNodeWhenCallingXmlNodeGetNameVerifyXmlNodeGetNameReturnsCorrectString) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/testEmptyElement"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::string name;
        name = pXmlNode->getName();
        EXPECT_STREQ(name.c_str(), "testEmptyElement");
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenValidXmlNodeWhenCallingXmlNodeGetPathVerifyXmlNodeGetPathReturnsCorrectString) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/testEmptyElement"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::string path;
        path = pXmlNode->getPath();
        EXPECT_STREQ(path.c_str(), "/testElement/testSubElement[1]/testSubSubElement[1]/testEmptyElement");
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenMissingAttributeWhenCallingXmlNodeGetAttributeWithValidNodeVerifyXmlNodeGetAttributeReturnsEmptyString) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/testEmptyElement"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::string attrib;
        attrib = pXmlNode->getAttribute(std::string("noSushAttrib"));
        EXPECT_EQ(0U, attrib.length());
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenNonExistentFileWhenCallingParseFileVerifyParseFileRetursNullptr) {

    XmlDoc *pNoXmlDoc = pXmlParser->parseFile(std::string("NoSuchFile.xml"));
    EXPECT_EQ(pNoXmlDoc, nullptr);
}
TEST_F(SysmanXmlParserFixture, GivenInvalidXmlBufferFileWhenCallingParseBufferVerifyParseBufferRetursNullptr) {

    std::string invalidXmlBuffer = testXmlBuffer.substr(2); // omit starting <?
    XmlDoc *pInvalidXmlDoc = pXmlParser->parseBuffer(invalidXmlBuffer);
    EXPECT_EQ(pInvalidXmlDoc, nullptr);
}
TEST_F(SysmanXmlParserFixture, GivenNonExistentXPathWhenCallingXmlDocXPathWithValidXmlDocVerifyXmlDocXPathReturnsEmptyVector) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/NoSuchElement"));
    EXPECT_EQ(xmlNodes.size(), 0U);
    for (auto pXmlNode : xmlNodes) {
        delete pXmlNode;
    }
}
TEST_F(SysmanXmlParserFixture, GivenNonExistentXPathWhenCallingXmlNodeXPathWithValidXmlNodeVerifyXmlNodeXPathReturnsEmptyVector) {

    std::vector<XmlNode *> xmlNodes = pXmlDoc->xPath(std::string("//testSubSubElement[@id='one']/testEmptyElement"));
    EXPECT_EQ(xmlNodes.size(), 1U);
    for (auto pXmlNode : xmlNodes) {
        std::vector<XmlNode *> subXmlNodes = pXmlNode->xPath(std::string("NoSuchElement"));
        EXPECT_EQ(subXmlNodes.size(), 0U);
        for (auto pSubXmlNode : subXmlNodes) {
            delete pSubXmlNode;
        }
        delete pXmlNode;
    }
}

} // namespace ult
} // namespace L0
