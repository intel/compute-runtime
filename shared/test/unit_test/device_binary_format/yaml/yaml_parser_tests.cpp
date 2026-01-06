/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/test/common/test_macros/test.h"

#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace NEO::Yaml;
using namespace NEO;

TEST(YamlIsWhitespace, GivenCharThenReturnsTrueOnlyWhenCharIsWhitespace) {
    std::set<char> whitespaces{' ', '\t', '\r', '\n'};
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = whitespaces.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isWhitespace(static_cast<char>(c))) << static_cast<char>(c);
    }
}

template <typename T>
struct IteratorAsValue {
    // iterator traits
    using difference_type = long;
    using value_type = long;
    using pointer = const long *;
    using reference = const long &;
    using iterator_category = std::forward_iterator_tag;

    IteratorAsValue(const T &v) : value(v) {}
    T operator*() const { return value; }
    IteratorAsValue<T> &operator++() {
        ++value;
        return *this;
    }
    bool operator==(const IteratorAsValue<T> &rhs) const { return this->value == rhs.value; }
    bool operator!=(const IteratorAsValue<T> &rhs) const { return this->value != rhs.value; }
    bool operator<(const IteratorAsValue<T> &rhs) const {
        return this->value < rhs.value;
    }
    T value;
};

TEST(YamlIsLetter, GivenCharThenReturnsTrueOnlyWhenCharIsLetter) {
    std::set<char> validChars{};
    using It = IteratorAsValue<char>;
    validChars.insert(It{'a'}, ++It{'z'});
    validChars.insert(It{'A'}, ++It{'Z'});
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = validChars.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isLetter(static_cast<char>(c))) << static_cast<char>(c);
    }
}

TEST(YamlIsNumber, GivenCharThenReturnsTrueOnlyWhenCharIsNumber) {
    std::set<char> validChars{};
    using It = IteratorAsValue<char>;
    validChars.insert(It{'0'}, ++It{'9'});
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = validChars.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isNumber(static_cast<char>(c))) << static_cast<char>(c);
    }
}

TEST(YamlIsAlphaNumeric, GivenCharThenReturnsTrueOnlyWhenCharIsNumberOrLetter) {
    std::set<char> validChars{};
    using It = IteratorAsValue<char>;
    validChars.insert(It{'a'}, ++It{'z'});
    validChars.insert(It{'A'}, ++It{'Z'});
    validChars.insert(It{'0'}, ++It{'9'});
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = validChars.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isAlphaNumeric(static_cast<char>(c))) << static_cast<char>(c);
    }
}

TEST(YamlIsNameIdentifierCharacter, GivenCharThenReturnsTrueOnlyWhenCharIsNumberOrLetterOrUnderscoreOrHyphen) {
    std::set<char> validChars{};
    using It = IteratorAsValue<char>;
    validChars.insert(It{'a'}, ++It{'z'});
    validChars.insert(It{'A'}, ++It{'Z'});
    validChars.insert(It{'0'}, ++It{'9'});
    validChars.insert('_');
    validChars.insert('-');
    validChars.insert('.');
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = validChars.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isNameIdentifierCharacter(static_cast<char>(c))) << static_cast<char>(c);
    }
}

TEST(YamlIsNameBeginningCharacter, GivenCharThenReturnsTrueOnlyWhenCharIsLetterOrUnderscore) {
    std::set<char> validChars{};
    using It = IteratorAsValue<char>;
    validChars.insert(It{'a'}, ++It{'z'});
    validChars.insert(It{'A'}, ++It{'Z'});
    validChars.insert('_');
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = validChars.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isNameIdentifierBeginningCharacter(static_cast<char>(c))) << static_cast<char>(c);
    }
}

TEST(YamlIsSign, GivenCharThenReturnsTrueOnlyWhenCharIsPlusOrMinus) {
    std::set<char> validChars{};
    validChars.insert('+');
    validChars.insert('-');
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool expected = validChars.count(static_cast<char>(c)) > 0;
        EXPECT_EQ(expected, NEO::Yaml::isSign(static_cast<char>(c))) << static_cast<char>(c);
    }
}

TEST(YamlIsSpecificNameIdentifier, WhenTextIsEmptyThenReturnFalse) {
    ConstStringRef text = "a";
    EXPECT_TRUE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin(), "a"));

    ConstStringRef empty;
    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(empty, empty.begin(), "a"));
}

TEST(YamlIsSpecificNameIdentifier, WhenTextIsTooShortThenReturnFalse) {
    ConstStringRef text = "abc";
    EXPECT_TRUE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 1, "bc"));

    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin(), "bcd"));
}

TEST(YamlIsSpecificNameIdentifier, WhenIdetifierIsSubstringThenReturnFalse) {
    ConstStringRef text = " bcD efg ij_ kl5";
    EXPECT_TRUE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 1, "bcD"));
    EXPECT_TRUE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 5, "efg"));
    EXPECT_TRUE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 9, "ij_"));
    EXPECT_TRUE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 13, "kl5"));

    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 1, "bc"));
    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 1, "b"));

    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 5, "ef"));
    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 5, "e"));

    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 9, "ij"));
    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 9, "i"));

    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 13, "kl"));
    EXPECT_FALSE(NEO::Yaml::isSpecificNameIdentifier(text, text.begin() + 13, "k"));
}

TEST(YamlIsMatched, WhenTextIsEmptyThenReturnFalse) {
    ConstStringRef text = "a";
    EXPECT_TRUE(NEO::Yaml::isMatched(text, text.begin(), "a"));

    ConstStringRef empty;
    EXPECT_FALSE(NEO::Yaml::isMatched(empty, empty.begin(), "a"));
}

TEST(YamlIsMatched, WhenTextIsTooShortThenReturnFalse) {
    ConstStringRef text = "abc";
    EXPECT_TRUE(NEO::Yaml::isMatched(text, text.begin() + 1, "bc"));

    EXPECT_FALSE(NEO::Yaml::isMatched(text, text.begin(), "bcd"));
}

TEST(YamlIsValidInlineCollectionFormat, WhenEndIsReachedThenReturnFalse) {
    const char coll[8] = "[ 1, 2 ";
    EXPECT_FALSE(NEO::Yaml::isValidInlineCollectionFormat(coll, coll + 7));
}

TEST(YamlIsValidInlineCollectionFormat, WhenCollectionIsEmptyThenReturnTrue) {
    const char coll[3] = "[]";
    EXPECT_TRUE(NEO::Yaml::isValidInlineCollectionFormat(coll, coll + 2));
}

TEST(YamlConsumeNumberOrSign, GivenValidNumberOrSignThenReturnProperEndingPosition) {
    ConstStringRef plus5 = "a+5";
    ConstStringRef minus7 = "b  -7 [";
    ConstStringRef plus = "c+";
    ConstStringRef minus = "d- ";
    ConstStringRef num957 = "e957";
    ConstStringRef num9dot57 = "f9.57";
    EXPECT_EQ(plus5.begin() + 3, NEO::Yaml::consumeNumberOrSign(plus5, plus5.begin() + 1));
    EXPECT_EQ(minus7.begin() + 5, NEO::Yaml::consumeNumberOrSign(minus7, minus7.begin() + 3));
    EXPECT_EQ(plus.begin() + 2, NEO::Yaml::consumeNumberOrSign(plus, plus.begin() + 1));
    EXPECT_EQ(minus.begin() + 2, NEO::Yaml::consumeNumberOrSign(minus, minus.begin() + 1));
    EXPECT_EQ(num957.begin() + 4, NEO::Yaml::consumeNumberOrSign(num957, num957.begin() + 1));
    EXPECT_EQ(num9dot57.begin() + 5, NEO::Yaml::consumeNumberOrSign(num9dot57, num9dot57.begin() + 1));
}

TEST(YamlConsumeNumberOrSign, GivenInvalidCharacterThenReturnCurrentParsePosition) {
    ConstStringRef plus5 = "a+5";
    ConstStringRef minus7 = "b  -7 [";
    ConstStringRef plus = "c+";
    ConstStringRef minus = "d- ";
    ConstStringRef num957 = "e957";
    ConstStringRef num9dot57 = "f9.57";
    ConstStringRef num9dot57X = "f9.57X";
    ConstStringRef plusPlusSeven = "++7";

    EXPECT_EQ(plus5.begin(), NEO::Yaml::consumeNumberOrSign(plus5, plus5.begin()));
    EXPECT_EQ(minus7.begin(), NEO::Yaml::consumeNumberOrSign(minus7, minus7.begin()));
    EXPECT_EQ(plus.begin(), NEO::Yaml::consumeNumberOrSign(plus, plus.begin()));
    EXPECT_EQ(minus.begin(), NEO::Yaml::consumeNumberOrSign(minus, minus.begin()));
    EXPECT_EQ(num957.begin(), NEO::Yaml::consumeNumberOrSign(num957, num957.begin()));
    EXPECT_EQ(num9dot57.begin(), NEO::Yaml::consumeNumberOrSign(num9dot57, num9dot57.begin()));
    EXPECT_EQ(num9dot57X.begin() + 1, NEO::Yaml::consumeNumberOrSign(num9dot57X, num9dot57X.begin() + 1));
    EXPECT_EQ(plusPlusSeven.begin() + 1, NEO::Yaml::consumeNumberOrSign(plusPlusSeven, plusPlusSeven.begin()));

    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool isSignOrNumber = NEO::Yaml::isSign(static_cast<char>(c)) || NEO::Yaml::isNumber(static_cast<char>(c));
        std::string numberStr(1, static_cast<char>(c));
        auto expected = numberStr.c_str() + (isSignOrNumber ? 1 : 0);
        EXPECT_EQ(expected, NEO::Yaml::consumeNumberOrSign(ConstStringRef(numberStr.c_str(), 1), numberStr.c_str())) << c;
    }
}

TEST(YamlConsumeNameIdentifier, GivenInvalidNameIdentifierBeginningCharacterThenReturnCurrentParsePosition) {
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool isNameIndentifierBeginChar = NEO::Yaml::isNameIdentifierBeginningCharacter(static_cast<char>(c));
        char nameIdentifierStr[] = {static_cast<char>(c), '\0'};
        auto expected = nameIdentifierStr + (isNameIndentifierBeginChar ? 1 : 0);
        EXPECT_EQ(expected, NEO::Yaml::consumeNameIdentifier(ConstStringRef::fromArray(nameIdentifierStr), nameIdentifierStr)) << c;
    }
}

TEST(YamlConsumeNameIdentifier, GivenNameIdentifierBeginningCharacterThenConsumeWholeNameIdentifier) {
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
        bool isNameOrSeparationWhitespaceIndentifierChar = NEO::Yaml::isNameIdentifierCharacter(static_cast<char>(c));
        isNameOrSeparationWhitespaceIndentifierChar |= NEO::Yaml::isSeparationWhitespace(static_cast<char>(c));
        char nameIdentifierStr[] = {'A', static_cast<char>(c)};
        auto expected = nameIdentifierStr + (isNameOrSeparationWhitespaceIndentifierChar ? 2 : 1);
        EXPECT_EQ(expected, NEO::Yaml::consumeNameIdentifier(ConstStringRef::fromArray(nameIdentifierStr), nameIdentifierStr)) << c;
    }
}

TEST(YamlConsumeStringLiteral, GivenQuotedStringThenConsumeUntilEndingMarkIsMet) {
    ConstStringRef notQuoted = "a+5";
    ConstStringRef singleQuote = "\'abc de fg\'ijkl";
    ConstStringRef doubleQuote = "\"abc de fg\"ijkl";
    ConstStringRef escapeSingleQuote = "\'abc de\\\' fg\'ijkl";
    ConstStringRef escapeDoubleQuote = "\"abc de\\\" fg\"ijkl";
    ConstStringRef mixedQuoteOuterDouble = "\"abc de\' fg\"ijkl";
    ConstStringRef mixedQuoteOuterSingle = "\'abc de\" fg\'ijkl";
    ConstStringRef unterminatedSingleQuote = "\'abc de";
    ConstStringRef unterminatedDoubleQuote = "\"abc de";

    EXPECT_EQ(notQuoted.begin(), NEO::Yaml::consumeStringLiteral(notQuoted, notQuoted.begin())) << notQuoted.data();
    EXPECT_EQ(singleQuote.begin() + 11, NEO::Yaml::consumeStringLiteral(singleQuote, singleQuote.begin())) << singleQuote.data();
    EXPECT_EQ(doubleQuote.begin() + 11, NEO::Yaml::consumeStringLiteral(doubleQuote, doubleQuote.begin())) << doubleQuote.data();
    EXPECT_EQ(escapeSingleQuote.begin() + 13, NEO::Yaml::consumeStringLiteral(escapeSingleQuote, escapeSingleQuote.begin())) << escapeSingleQuote.data();
    EXPECT_EQ(escapeDoubleQuote.begin() + 13, NEO::Yaml::consumeStringLiteral(escapeDoubleQuote, escapeDoubleQuote.begin())) << escapeDoubleQuote.data();
    EXPECT_EQ(mixedQuoteOuterDouble.begin() + 12, NEO::Yaml::consumeStringLiteral(mixedQuoteOuterDouble, mixedQuoteOuterDouble.begin())) << mixedQuoteOuterDouble.data();
    EXPECT_EQ(mixedQuoteOuterSingle.begin() + 12, NEO::Yaml::consumeStringLiteral(mixedQuoteOuterSingle, mixedQuoteOuterSingle.begin())) << mixedQuoteOuterSingle.data();
    EXPECT_EQ(unterminatedSingleQuote.begin(), NEO::Yaml::consumeStringLiteral(unterminatedSingleQuote, unterminatedSingleQuote.begin())) << unterminatedSingleQuote.data();
    EXPECT_EQ(unterminatedDoubleQuote.begin(), NEO::Yaml::consumeStringLiteral(unterminatedDoubleQuote, unterminatedDoubleQuote.begin())) << unterminatedDoubleQuote.data();
}

TEST(YamlToken, WhenConstructedThenSetsUpProperDefaults) {
    ConstStringRef str = "\"some string\"";
    ConstStringRef identifier = "someIdentifier";
    NEO::Yaml::Token tokenString{str, NEO::Yaml::Token::literalString};
    NEO::Yaml::Token tokenNameIdentifier{identifier, NEO::Yaml::Token::identifier};

    EXPECT_EQ(str.begin(), tokenString.pos);
    EXPECT_EQ(str.length(), tokenString.len);
    EXPECT_EQ(str[0], tokenString.traits.character0);
    EXPECT_EQ(NEO::Yaml::Token::literalString, tokenString.traits.type);
    EXPECT_EQ(str, tokenString.cstrref());

    EXPECT_EQ(identifier.begin(), tokenNameIdentifier.pos);
    EXPECT_EQ(identifier.length(), tokenNameIdentifier.len);
    EXPECT_EQ(identifier[0], tokenNameIdentifier.traits.character0);
    EXPECT_EQ(NEO::Yaml::Token::identifier, tokenNameIdentifier.traits.type);
    EXPECT_EQ(identifier, tokenNameIdentifier.cstrref());
}

TEST(YamlTokenMatchers, WhenTokenIsComparedAgainstTextThenReturnTrueOnlyIfMatched) {
    NEO::Yaml::Token tokenString{"abc", NEO::Yaml::Token::identifier};
    EXPECT_TRUE(ConstStringRef("abc") == tokenString);
    EXPECT_TRUE(tokenString == ConstStringRef("abc"));
    EXPECT_FALSE(ConstStringRef("abd") == tokenString);
    EXPECT_FALSE(tokenString == ConstStringRef("abd"));
    EXPECT_TRUE(ConstStringRef("abd") != tokenString);
    EXPECT_TRUE(tokenString != ConstStringRef("abd"));
}

TEST(YamlTokenMatchers, WhenTokenIsComparedAgainstCharacterThenReturnTrueOnlyIfMatched) {
    NEO::Yaml::Token tokenString{"+", NEO::Yaml::Token::identifier};
    EXPECT_TRUE('+' == tokenString);
    EXPECT_TRUE(tokenString == '+');
    EXPECT_FALSE('-' == tokenString);
    EXPECT_FALSE(tokenString == '-');
    EXPECT_TRUE('-' != tokenString);
    EXPECT_TRUE(tokenString != '-');
}

TEST(YamlLineTraits, WhenResetThenFlagsGetCleared) {
    NEO::Yaml::Line::LineTraits traits;
    traits.hasDictionaryEntry = true;
    traits.hasInlineDataMarkers = true;
    EXPECT_NE(0U, traits.packed);

    traits.reset();
    EXPECT_EQ(0U, traits.packed);
    EXPECT_FALSE(traits.hasDictionaryEntry);
    EXPECT_FALSE(traits.hasInlineDataMarkers);
}

TEST(YamlLine, WhenConstructedThenSetsUpProperDefaults) {
    NEO::Yaml::Line::LineTraits emptyTraits;
    emptyTraits.reset();
    NEO::Yaml::Line commentLine{NEO::Yaml::Line::LineType::comment, 4, 6, 8, emptyTraits};

    NEO::Yaml::Line::LineTraits dictionaryEntryTraits;
    dictionaryEntryTraits.reset();
    dictionaryEntryTraits.hasDictionaryEntry = true;
    NEO::Yaml::Line dictEntryLine{NEO::Yaml::Line::LineType::dictionaryEntry, 2, 128, 146, dictionaryEntryTraits};

    EXPECT_EQ(4U, commentLine.indent);
    EXPECT_EQ(6U, commentLine.first);
    EXPECT_EQ(8U, commentLine.last);
    EXPECT_EQ(NEO::Yaml::Line::LineType::comment, commentLine.lineType);
    EXPECT_EQ(emptyTraits.packed, commentLine.traits.packed);

    EXPECT_EQ(2U, dictEntryLine.indent);
    EXPECT_EQ(128U, dictEntryLine.first);
    EXPECT_EQ(146U, dictEntryLine.last);
    EXPECT_EQ(NEO::Yaml::Line::LineType::dictionaryEntry, dictEntryLine.lineType);
    EXPECT_EQ(dictionaryEntryTraits.packed, dictEntryLine.traits.packed);
}

TEST(YamlConstructParseError, WhenConstructingErrorThenTakesLineNumberAndContextIntoAccount) {
    ConstStringRef line{"Not really a yaml"};
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [146] : [Not r] <-- parser position on error\n", NEO::Yaml::constructYamlError(146, line.begin(), line.begin() + 4).c_str());
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [146] : [Not r] <-- parser position on error. Reason : Yup, not a yaml\n", NEO::Yaml::constructYamlError(146, line.begin(), line.begin() + 4, "Yup, not a yaml").c_str());
}

TEST(YamlTokenize, GivenEmptyInputStringThenEmitsWarning) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize("", lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(lines.empty());
    EXPECT_TRUE(tokens.empty());
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("NEO::Yaml : input text is empty\n", warnings.c_str());

    warnings.clear();
    success = NEO::Yaml::tokenize(" ", lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(lines.empty());
    EXPECT_TRUE(tokens.empty());
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("NEO::Yaml : text tokenized to 0 tokens\n", warnings.c_str());

    warnings.clear();
    success = NEO::Yaml::tokenize("\r", lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(lines.empty());
    EXPECT_TRUE(tokens.empty());
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("NEO::Yaml : text tokenized to 0 tokens\n", warnings.c_str());

    warnings.clear();
    success = NEO::Yaml::tokenize(ConstStringRef::fromArray("\0"), lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(lines.empty());
    EXPECT_TRUE(tokens.empty());
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("NEO::Yaml : text tokenized to 0 tokens\n", warnings.c_str());
}

TEST(YamlTokenize, GivenTabsAsIndentThenEmitsWarning) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize("\t", lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(lines.empty());
    EXPECT_TRUE(tokens.empty());
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("NEO::Yaml : Tabs used as indent at line : 0\nNEO::Yaml : text tokenized to 0 tokens\n", warnings.c_str());
}

TEST(YamlTokenize, WhenTextDoesNotEndWithNewlineThenEmitsWarning) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize("aaaaa", lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_EQ(1U, lines.size());
    EXPECT_EQ(2U, tokens.size());
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("NEO::Yaml : text does not end with newline\n", warnings.c_str());
}

TEST(YamlTokenize, WhenTextIsNotPartOfACollectionThenEmitsError) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize("7\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [7\n] <-- parser position on error. Reason : Internal error - undefined line type\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();
    success = NEO::Yaml::tokenize(": a\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [: a\n] <-- parser position on error. Reason : Unhandled keyword character : :\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(YamlTokenize, GivenInlineDictionariesThenEmitsError) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;

    bool success = NEO::Yaml::tokenize("{\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [{] <-- parser position on error. Reason : NEO::Yaml : Inline dictionaries are not supported\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();

    success = NEO::Yaml::tokenize("}\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [}] <-- parser position on error. Reason : NEO::Yaml : Inline dictionaries are not supported\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(YamlTokenize, GivenInvalidInlineCollectionThenEmitsError) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;

    bool success = NEO::Yaml::tokenize("]\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : []] <-- parser position on error. Reason : NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();

    success = NEO::Yaml::tokenize(",\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [,] <-- parser position on error. Reason : NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();

    success = NEO::Yaml::tokenize("[123,32,,]\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [[] <-- parser position on error. Reason : NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();

    success = NEO::Yaml::tokenize("[1,2,3,4]]\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [[] <-- parser position on error. Reason : NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();

    success = NEO::Yaml::tokenize("[[1,2,3,4]\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [[] <-- parser position on error. Reason : NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    success = NEO::Yaml::tokenize("[1 2 3 4]\n", lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [[] <-- parser position on error. Reason : NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

namespace NEO {
namespace Yaml {

bool operator==(const NEO::Yaml::Line &lhs, const NEO::Yaml::Line &rhs) {
    bool matched = (lhs.first == rhs.first);
    matched &= (lhs.last == rhs.last);
    matched &= (lhs.indent == rhs.indent);
    matched &= (lhs.lineType == rhs.lineType);
    matched &= (lhs.traits.packed == rhs.traits.packed);
    return matched;
}

bool operator==(const NEO::Yaml::Token &lhs, const NEO::Yaml::Token &rhs) {
    bool matched = (lhs.cstrref() == rhs);
    matched &= (lhs.traits.type == rhs.traits.type);
    return matched;
}

} // namespace Yaml
} // namespace NEO

TEST(YamlTokenize, GivenMultilineDictionaryThenTokenizeAllEntries) {
    ConstStringRef yaml =
        R"===(
    apple : red
    banana : yellow
    orange : orange
)===";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 0

        // line 1
        Token{"apple", NEO::Yaml::Token::identifier},   // token 1
        Token{":", NEO::Yaml::Token::singleCharacter},  // token 2
        Token{"red", NEO::Yaml::Token::literalString},  // token 3
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 4

        // line 2
        Token{"banana", NEO::Yaml::Token::identifier},    // token 5
        Token{":", NEO::Yaml::Token::singleCharacter},    // token 6
        Token{"yellow", NEO::Yaml::Token::literalString}, // token 7
        Token{"\n", NEO::Yaml::Token::singleCharacter},   // token 8

        // line 3
        Token{"orange", NEO::Yaml::Token::identifier},    // token 9
        Token{":", NEO::Yaml::Token::singleCharacter},    // token 10
        Token{"orange", NEO::Yaml::Token::literalString}, // token 11
        Token{"\n", NEO::Yaml::Token::singleCharacter},   // token 12
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::empty, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::dictionaryEntry, 4, 1, 4, {}},
        Line{NEO::Yaml::Line::LineType::dictionaryEntry, 4, 5, 8, {}},
        Line{NEO::Yaml::Line::LineType::dictionaryEntry, 4, 9, 12, {}},
    };
    expectedLines[1].traits.hasDictionaryEntry = true;
    expectedLines[2].traits.hasDictionaryEntry = true;
    expectedLines[3].traits.hasDictionaryEntry = true;

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenMultilineListThenTokenizeAllEntries) {
    ConstStringRef yaml =
        R"===(
   - apple
   - banana
   - orange
)===";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 0

        // line 1
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 1
        Token{"apple", NEO::Yaml::Token::identifier},   // token 2
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 3

        // line 2
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 4
        Token{"banana", NEO::Yaml::Token::identifier},  // token 5
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 6

        // line 3
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 7
        Token{"orange", NEO::Yaml::Token::identifier},  // token 8
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 9
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::empty, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 3, 1, 3, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 3, 4, 6, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 3, 7, 9, {}},
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenCommentThenTokenizeTillEndOfLineAsOneToken) {
    ConstStringRef yaml = "orange : green # needs more sun\n";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"orange", NEO::Yaml::Token::identifier},       // token 0
        Token{":", NEO::Yaml::Token::singleCharacter},       // token 1
        Token{"green", NEO::Yaml::Token::literalString},     // token 2
        Token{"#", NEO::Yaml::Token::singleCharacter},       // token 3
        Token{" needs more sun", NEO::Yaml::Token::comment}, // token 4
        Token{"\n", NEO::Yaml::Token::singleCharacter},      // token 5
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::dictionaryEntry, 0, 0, 5, {}},
    };

    expectedLines[0].traits.hasDictionaryEntry = true;

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }

    ConstStringRef yamlCommentTillEnd = ConstStringRef(yaml.data(), 25);
    tokens.clear();
    lines.clear();
    success = NEO::Yaml::tokenize(yamlCommentTillEnd, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_FALSE(warnings.empty());

    expectedTokens[4].len -= static_cast<uint32_t>(yaml.length() - yamlCommentTillEnd.length() - expectedTokens[5].len);
    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenEmptyCommentMarkerThenDontCreateEmptyComment) {
    ConstStringRef yaml = "orange : green #\n";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"orange", NEO::Yaml::Token::identifier},   // token 0
        Token{":", NEO::Yaml::Token::singleCharacter},   // token 1
        Token{"green", NEO::Yaml::Token::literalString}, // token 2
        Token{"#", NEO::Yaml::Token::singleCharacter},   // token 3
        Token{"\n", NEO::Yaml::Token::singleCharacter},  // token 4
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::dictionaryEntry, 0, 0, 4, {}},
    };

    expectedLines[0].traits.hasDictionaryEntry = true;

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenCommentAtTheBeginningOfTheLineThenMarkWholeLineAsComment) {
    ConstStringRef yaml = "#orange : green\n";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"#", NEO::Yaml::Token::singleCharacter},      // token 0
        Token{"orange : green", NEO::Yaml::Token::comment}, // token 1
        Token{"\n", NEO::Yaml::Token::singleCharacter},     // token 2
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::comment, 0, 0, 2, {}},
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenFileSectionMarkersThenTokenizesThemProperly) {
    ConstStringRef yaml =
        R"===(
---
 - banana
...
)===";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 0

        // line 1
        Token{"---", NEO::Yaml::Token::fileSectionBeg}, // token 1
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 2

        // line 2
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 3
        Token{"banana", NEO::Yaml::Token::identifier},  // token 4
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 5

        // line 3
        Token{"...", NEO::Yaml::Token::fileSectionEnd}, // token 6
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 7
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::empty, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::fileSection, 0, 1, 2, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 1, 3, 5, {}},
        Line{NEO::Yaml::Line::LineType::fileSection, 0, 6, 7, {}},
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenInvalidFileSectionMarkersThenTreatThemAsText) {
    ConstStringRef yaml = "- .\n";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 1
        Token{".", NEO::Yaml::Token::singleCharacter},  // token 2
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 3
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 0, 2, {}},
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();
    yaml = "..\n";
    success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [..\n] <-- parser position on error. Reason : Unhandled keyword character : .\n", errors.c_str()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(YamlTokenize, GivenStringLiteralsThenTokenizesThemProperly) {
    ConstStringRef yaml =
        R"===(
- "abc defg ijk"
- '23 57'
- 'needs to "match"'
- "match 'needs to'"
)===";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 0

        // line 1
        Token{"-", NEO::Yaml::Token::singleCharacter},              // token 1
        Token{"\"abc defg ijk\"", NEO::Yaml::Token::literalString}, // token 2
        Token{"\n", NEO::Yaml::Token::singleCharacter},             // token 3

        // line 2
        Token{"-", NEO::Yaml::Token::singleCharacter},       // token 4
        Token{"\'23 57\'", NEO::Yaml::Token::literalString}, // token 5
        Token{"\n", NEO::Yaml::Token::singleCharacter},      // token 6

        // line 3
        Token{"-", NEO::Yaml::Token::singleCharacter},                    // token 7
        Token{"\'needs to \"match\"\'", NEO::Yaml::Token::literalString}, // token 8
        Token{"\n", NEO::Yaml::Token::singleCharacter},                   // token 9

        // line 4
        Token{"-", NEO::Yaml::Token::singleCharacter},                    // token 10
        Token{"\"match \'needs to\'\"", NEO::Yaml::Token::literalString}, // token 11
        Token{"\n", NEO::Yaml::Token::singleCharacter},                   // token 12
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::empty, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 1, 3, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 4, 6, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 7, 9, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 10, 12, {}},
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenUnterminatedStringLiteralsThenReturnsError) {
    ConstStringRef yaml = "\"aaaa";
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [\"] <-- parser position on error. Reason : Unterminated string\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(YamlTokenize, GivenNumericLiteralsThenTokenizesThemProperly) {
    ConstStringRef yaml =
        R"===(
- -5
- +7
- 136
- 95.8
- +14.7
- -63.1
- 0
)===";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 0

        // line 1
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 1
        Token{"-5", NEO::Yaml::Token::literalNumber},   // token 2
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 3

        // line 2
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 4
        Token{"+7", NEO::Yaml::Token::literalNumber},   // token 5
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 6

        // line 3
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 7
        Token{"136", NEO::Yaml::Token::literalNumber},  // token 8
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 9

        // line 4
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 10
        Token{"95.8", NEO::Yaml::Token::literalNumber}, // token 11
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 12

        // line 5
        Token{"-", NEO::Yaml::Token::singleCharacter},   // token 13
        Token{"+14.7", NEO::Yaml::Token::literalNumber}, // token 14
        Token{"\n", NEO::Yaml::Token::singleCharacter},  // token 15

        // line 6
        Token{"-", NEO::Yaml::Token::singleCharacter},   // token 16
        Token{"-63.1", NEO::Yaml::Token::literalNumber}, // token 17
        Token{"\n", NEO::Yaml::Token::singleCharacter},  // token 18

        // line 7
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 19
        Token{"0", NEO::Yaml::Token::literalNumber},    // token 20
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 21
    };

    NEO::Yaml::Line expectedLines[] = {
        Line{NEO::Yaml::Line::LineType::empty, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 1, 3, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 4, 6, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 7, 9, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 10, 12, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 13, 15, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 16, 18, {}},
        Line{NEO::Yaml::Line::LineType::listEntry, 0, 19, 21, {}},
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }

    ASSERT_EQ(sizeof(expectedLines) / sizeof(expectedLines[0]), lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(expectedLines[i], lines[i]) << i;
    }
}

TEST(YamlTokenize, GivenInvalidNumericLiteralThenReturnError) {
    ConstStringRef yaml = "@5\n";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [0] : [@] <-- parser position on error. Reason : Invalid numeric literal\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(YamlTokenize, GivenSpaceSeparatedStringAsValueThenReadItCorrectly) {
    ConstStringRef yaml = "\nbanana: space separated string\napple: space separated with spaces at the end   \n";

    NEO::Yaml::Token expectedTokens[] = {
        Token{"\n", NEO::Yaml::Token::singleCharacter},
        Token{"banana", NEO::Yaml::Token::identifier},
        Token{":", NEO::Yaml::Token::singleCharacter},
        Token{"space separated string", NEO::Yaml::Token::literalString},
        Token{"\n", NEO::Yaml::Token::singleCharacter},
        Token{"apple", NEO::Yaml::Token::identifier},
        Token{":", NEO::Yaml::Token::singleCharacter},
        Token{"space separated with spaces at the end", NEO::Yaml::Token::literalString},
        Token{"\n", NEO::Yaml::Token::singleCharacter}};
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }
}

TEST(YamlTokenize, GivenHexadecimalNumbersThenTokenizesThemProperly) {
    ConstStringRef yaml =
        R"===(
    - 0x1A
    - 0xFFFF
    - 0x1a2b3c4d
    - 0X7F
    - 0xabcdef
)===";

    NEO::Yaml::Token expectedTokens[] = {
        // line 0
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 0

        // line 1
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 1
        Token{"0x1A", NEO::Yaml::Token::literalNumber}, // token 2
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 3

        // line 2
        Token{"-", NEO::Yaml::Token::singleCharacter},    // token 4
        Token{"0xFFFF", NEO::Yaml::Token::literalNumber}, // token 5
        Token{"\n", NEO::Yaml::Token::singleCharacter},   // token 6

        // line 3
        Token{"-", NEO::Yaml::Token::singleCharacter},        // token 7
        Token{"0x1a2b3c4d", NEO::Yaml::Token::literalNumber}, // token 8
        Token{"\n", NEO::Yaml::Token::singleCharacter},       // token 9

        // line 4
        Token{"-", NEO::Yaml::Token::singleCharacter},  // token 10
        Token{"0X7F", NEO::Yaml::Token::literalNumber}, // token 11
        Token{"\n", NEO::Yaml::Token::singleCharacter}, // token 12

        // line 5
        Token{"-", NEO::Yaml::Token::singleCharacter},      // token 13
        Token{"0xabcdef", NEO::Yaml::Token::literalNumber}, // token 14
        Token{"\n", NEO::Yaml::Token::singleCharacter},     // token 15
    };

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(sizeof(expectedTokens) / sizeof(expectedTokens[0]), tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(expectedTokens[i], tokens[i]) << i;
    }
}

TEST(YamlParserReadValueCheckedInt64, GivenHexadecimalIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = "hex_value : 0x123456789ABCDEF";
    int64_t expectedInt64 = 0x123456789ABCDEF;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto hexNode = parser.getChild(*parser.getRoot(), "hex_value");
    ASSERT_NE(hexNode, nullptr);
    int64_t readValue = 0;
    auto readSuccess = parser.readValueChecked<int64_t>(*hexNode, readValue);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedInt64, readValue);
}

TEST(YamlParserReadValueCheckedUint64, GivenHexadecimalIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = "hex_value : 0X123456789ABCDEF";
    uint64_t expectedUint64 = 0X123456789ABCDEF;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto hexNode = parser.getChild(*parser.getRoot(), "hex_value");
    ASSERT_NE(hexNode, nullptr);
    uint64_t readValue = 0;
    auto readSuccess = parser.readValueChecked<uint64_t>(*hexNode, readValue);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedUint64, readValue);
}

TEST(YamlParserReadValueCheckedInt32, GivenHexadecimalIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===(
hex_value_32 : 0x1234ABCD
hex_value_64 : 0x123456789ABCDEF
)===";
    int32_t expectedInt32 = 0x1234ABCD;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto hex32Node = parser.getChild(*parser.getRoot(), "hex_value_32");
    ASSERT_NE(hex32Node, nullptr);
    int32_t readValue32 = 0;
    auto readSuccess32 = parser.readValueChecked<int32_t>(*hex32Node, readValue32);
    EXPECT_TRUE(readSuccess32);
    EXPECT_EQ(expectedInt32, readValue32);

    auto hex64Node = parser.getChild(*parser.getRoot(), "hex_value_64");
    ASSERT_NE(hex64Node, nullptr);
    int32_t readValue64 = 0;
    auto readSuccess64 = parser.readValueChecked<int32_t>(*hex64Node, readValue64);
    EXPECT_FALSE(readSuccess64);
}

TEST(YamlParserReadValueCheckedUint32, GivenHexadecimalIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===(
hex_value_32 : 0xABCDEF12
hex_value_64 : 0x123456789ABCDEF
)===";
    uint32_t expectedUint32 = 0xABCDEF12;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto hex32Node = parser.getChild(*parser.getRoot(), "hex_value_32");
    ASSERT_NE(hex32Node, nullptr);
    uint32_t readValue32 = 0;
    auto readSuccess32 = parser.readValueChecked<uint32_t>(*hex32Node, readValue32);
    EXPECT_TRUE(readSuccess32);
    EXPECT_EQ(expectedUint32, readValue32);

    auto hex64Node = parser.getChild(*parser.getRoot(), "hex_value_64");
    ASSERT_NE(hex64Node, nullptr);
    uint32_t readValue64 = 0;
    auto readSuccess64 = parser.readValueChecked<uint32_t>(*hex64Node, readValue64);
    EXPECT_FALSE(readSuccess64);
}

TEST(YamlNode, WhenConstructedThenSetsUpProperDefaults) {
    {
        NEO::Yaml::Node node;
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.firstChildId);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.id);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.lastChildId);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.nextSiblingId);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.parentId);
        EXPECT_EQ(0U, node.indent);
        EXPECT_EQ(NEO::Yaml::invalidTokenId, node.key);
        EXPECT_EQ(NEO::Yaml::invalidTokenId, node.value);
        EXPECT_EQ(0U, node.numChildren);
    }

    {
        NEO::Yaml::Node node(7U);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.firstChildId);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.id);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.lastChildId);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.nextSiblingId);
        EXPECT_EQ(NEO::Yaml::invalidNodeID, node.parentId);
        EXPECT_EQ(7U, node.indent);
        EXPECT_EQ(NEO::Yaml::invalidTokenId, node.key);
        EXPECT_EQ(NEO::Yaml::invalidTokenId, node.value);
        EXPECT_EQ(0U, node.numChildren);
    }
}

TEST(YamlLineTypeIsUnused, WhenLineContaintsNoDataThenReturnTrue) {
    EXPECT_TRUE(NEO::Yaml::isUnused(NEO::Yaml::Line::LineType::empty));
    EXPECT_TRUE(NEO::Yaml::isUnused(NEO::Yaml::Line::LineType::comment));
    EXPECT_TRUE(NEO::Yaml::isUnused(NEO::Yaml::Line::LineType::fileSection));
}

TEST(YamlLineTypeIsUnused, WhenLineContaintsDataThenReturnFalse) {
    EXPECT_FALSE(NEO::Yaml::isUnused(NEO::Yaml::Line::LineType::dictionaryEntry));
    EXPECT_FALSE(NEO::Yaml::isUnused(NEO::Yaml::Line::LineType::listEntry));
}

TEST(YamlBuildTree, GivenEmptyInputThenEmitsWarning) {
    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    NEO::Yaml::NodesCache tree;
    std::string errors, warnings;
    bool success = NEO::Yaml::buildTree(lines, tokens, tree, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(tree.empty());

    EXPECT_STREQ("NEO::Yaml : Text has no data\n", warnings.c_str());
}

TEST(YamlBuildTree, GivenEmptyLinesThenSkipsThem) {
    NEO::Yaml::LinesCache lines = {
        Line{NEO::Yaml::Line::LineType::empty, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::comment, 0, 0, 0, {}},
        Line{NEO::Yaml::Line::LineType::fileSection, 0, 0, 0, {}},
    };
    NEO::Yaml::TokensCache tokens;
    NEO::Yaml::NodesCache tree;
    std::string errors, warnings;
    bool success = NEO::Yaml::buildTree(lines, tokens, tree, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(tree.empty());

    EXPECT_STREQ("NEO::Yaml : Text has no data\n", warnings.c_str());
}

template <typename ContainerT, typename IndexT>
auto at(ContainerT &container, IndexT index) -> decltype(std::declval<ContainerT>()[0]) & {
    if (index >= container.size()) {
        throw std::out_of_range(std::to_string(index) + " >= " + std::to_string(container.size()));
    }
    return container[index];
}

TEST(YamlBuildTree, GivenListThenProperlyCreatesChildNodes) {
    ConstStringRef yaml =
        R"===(
   - apple
   - banana
   - orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    ASSERT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(4U, treeNodes.size());

    auto &list = *treeNodes.begin();
    EXPECT_GT(treeNodes.size(), list.id);
    EXPECT_EQ(0U, list.indent);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, list.key);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, list.value);
    EXPECT_EQ(3U, list.numChildren);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, list.parentId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, list.nextSiblingId);

    ASSERT_GT(treeNodes.size(), list.firstChildId);
    ASSERT_GT(treeNodes.size(), list.lastChildId);

    auto &apple = treeNodes[list.firstChildId];
    ASSERT_GT(treeNodes.size(), apple.nextSiblingId);
    auto &banana = treeNodes[apple.nextSiblingId];
    auto &orange = treeNodes[list.lastChildId];

    EXPECT_EQ(3U, apple.indent);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, apple.key);
    EXPECT_GT(tokens.size(), apple.value);
    EXPECT_EQ("apple", tokens[apple.value].cstrref());
    EXPECT_EQ(0U, apple.numChildren);
    EXPECT_EQ(list.id, apple.parentId);
    EXPECT_EQ(banana.id, apple.nextSiblingId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, apple.firstChildId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, apple.lastChildId);

    EXPECT_EQ(3U, banana.indent);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, banana.key);
    EXPECT_GT(tokens.size(), banana.value);
    EXPECT_EQ("banana", tokens[banana.value].cstrref());
    EXPECT_EQ(0U, banana.numChildren);
    EXPECT_EQ(list.id, banana.parentId);
    EXPECT_EQ(orange.id, banana.nextSiblingId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.firstChildId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.lastChildId);

    EXPECT_EQ(3U, orange.indent);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, orange.key);
    EXPECT_GT(tokens.size(), orange.value);
    EXPECT_EQ("orange", tokens[orange.value].cstrref());
    EXPECT_EQ(0U, orange.numChildren);
    EXPECT_EQ(list.id, orange.parentId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, orange.nextSiblingId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.firstChildId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.lastChildId);
}

TEST(YamlBuildTree, GivenDictionaryThenProperlyCreatesChildNodes) {
    ConstStringRef yaml =
        R"===(
    apple : red
    banana : yellow
    orange : orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    ASSERT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(4U, treeNodes.size());

    auto &dictionary = *treeNodes.begin();
    EXPECT_GT(treeNodes.size(), dictionary.id);
    EXPECT_EQ(0U, dictionary.indent);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, dictionary.key);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, dictionary.value);
    EXPECT_EQ(3U, dictionary.numChildren);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, dictionary.parentId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, dictionary.nextSiblingId);

    ASSERT_GT(treeNodes.size(), dictionary.firstChildId);
    ASSERT_GT(treeNodes.size(), dictionary.lastChildId);

    auto &apple = treeNodes[dictionary.firstChildId];
    ASSERT_GT(treeNodes.size(), apple.nextSiblingId);
    auto &banana = treeNodes[apple.nextSiblingId];
    auto &orange = treeNodes[dictionary.lastChildId];

    EXPECT_EQ(4U, apple.indent);
    EXPECT_GT(tokens.size(), apple.key);
    EXPECT_EQ("apple", tokens[apple.key].cstrref());
    EXPECT_GT(tokens.size(), apple.value);
    EXPECT_EQ("red", tokens[apple.value].cstrref());
    EXPECT_EQ(0U, apple.numChildren);
    EXPECT_EQ(dictionary.id, apple.parentId);
    EXPECT_EQ(banana.id, apple.nextSiblingId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, apple.firstChildId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, apple.lastChildId);

    EXPECT_EQ(4U, banana.indent);
    EXPECT_GT(tokens.size(), banana.key);
    EXPECT_EQ("banana", tokens[banana.key].cstrref());
    EXPECT_GT(tokens.size(), banana.value);
    EXPECT_EQ("yellow", tokens[banana.value].cstrref());
    EXPECT_EQ(0U, banana.numChildren);
    EXPECT_EQ(dictionary.id, banana.parentId);
    EXPECT_EQ(orange.id, banana.nextSiblingId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.firstChildId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.lastChildId);

    EXPECT_EQ(4U, orange.indent);
    EXPECT_GT(tokens.size(), orange.key);
    EXPECT_EQ("orange", tokens[orange.key].cstrref());
    EXPECT_GT(tokens.size(), orange.value);
    EXPECT_EQ("orange", tokens[orange.value].cstrref());
    EXPECT_EQ(0U, orange.numChildren);
    EXPECT_EQ(dictionary.id, orange.parentId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, orange.nextSiblingId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.firstChildId);
    EXPECT_EQ(NEO::Yaml::invalidNodeID, banana.lastChildId);
}

TEST(YamlBuildTree, GivenNestedCollectionsThenProperlyCreatesChildNodes) {
    ConstStringRef yaml =
        R"===(
    banana : yellow
    apple : 
        - red
        - green
        - 
          types :
             - rome
             - cameo
          flavors :
             - sweet
             - bitter
    orange : orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    ASSERT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(13U, treeNodes.size());

    auto &rootNode = *treeNodes.begin();
    ASSERT_EQ(3U, rootNode.numChildren);
    auto &banana = at(treeNodes, rootNode.firstChildId);
    auto &apple = at(treeNodes, banana.nextSiblingId);
    auto &orange = at(treeNodes, apple.nextSiblingId);
    auto &appleRed = at(treeNodes, apple.firstChildId);
    auto &appleGreen = at(treeNodes, appleRed.nextSiblingId);
    auto &appleDict = at(treeNodes, appleGreen.nextSiblingId);
    auto &appleTypes = at(treeNodes, appleDict.firstChildId);
    auto &appleTypesRome = at(treeNodes, appleTypes.firstChildId);
    auto &appleTypesCameo = at(treeNodes, appleTypesRome.nextSiblingId);
    auto &appleFlavors = at(treeNodes, appleTypes.nextSiblingId);
    auto &appleFlavorsSweet = at(treeNodes, appleFlavors.firstChildId);
    auto &appleFlavorsBitter = at(treeNodes, appleFlavorsSweet.nextSiblingId);

    EXPECT_STREQ("banana", at(tokens, banana.key).cstrref().str().c_str());
    EXPECT_STREQ("yellow", at(tokens, banana.value).cstrref().str().c_str());

    EXPECT_STREQ("orange", at(tokens, orange.key).cstrref().str().c_str());
    EXPECT_STREQ("orange", at(tokens, orange.value).cstrref().str().c_str());

    EXPECT_STREQ("apple", at(tokens, apple.key).cstrref().str().c_str());
    EXPECT_STREQ("red", at(tokens, appleRed.value).cstrref().str().c_str());
    EXPECT_STREQ("green", at(tokens, appleGreen.value).cstrref().str().c_str());
    EXPECT_EQ(NEO::Yaml::invalidTokenId, appleDict.key);
    EXPECT_STREQ("types", at(tokens, appleTypes.key).cstrref().str().c_str());
    EXPECT_STREQ("rome", at(tokens, appleTypesRome.value).cstrref().str().c_str());
    EXPECT_STREQ("cameo", at(tokens, appleTypesCameo.value).cstrref().str().c_str());
    EXPECT_STREQ("flavors", at(tokens, appleFlavors.key).cstrref().str().c_str());
    EXPECT_STREQ("sweet", at(tokens, appleFlavorsSweet.value).cstrref().str().c_str());
    EXPECT_STREQ("bitter", at(tokens, appleFlavorsBitter.value).cstrref().str().c_str());
}

TEST(YamlBuildTree, GivenInlineCollectionThenProperlyCreatesChildNodes) {
    ConstStringRef yaml =
        R"===(
      banana : yellow
      kiwi : green
      apple : [ red, green, blue ]
      pear : pearish
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    ASSERT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(8U, treeNodes.size());
    auto &rootNode = *treeNodes.begin();
    ASSERT_EQ(4U, rootNode.numChildren);
    auto &banana = at(treeNodes, rootNode.firstChildId);
    auto &kiwi = at(treeNodes, banana.nextSiblingId);
    auto &apple = at(treeNodes, kiwi.nextSiblingId);
    auto &appleRed = at(treeNodes, apple.firstChildId);
    auto &appleGreen = at(treeNodes, appleRed.nextSiblingId);
    auto &appleBlue = at(treeNodes, appleGreen.nextSiblingId);
    auto &pear = at(treeNodes, apple.nextSiblingId);

    EXPECT_STREQ("banana", at(tokens, banana.key).cstrref().str().c_str());
    EXPECT_STREQ("yellow", at(tokens, banana.value).cstrref().str().c_str());

    EXPECT_STREQ("kiwi", at(tokens, kiwi.key).cstrref().str().c_str());
    EXPECT_STREQ("green", at(tokens, kiwi.value).cstrref().str().c_str());

    EXPECT_STREQ("apple", at(tokens, apple.key).cstrref().str().c_str());
    EXPECT_EQ(NEO::Yaml::invalidTokenId, apple.value);
    EXPECT_STREQ("red", at(tokens, appleRed.value).cstrref().str().c_str());
    EXPECT_EQ(NEO::Yaml::invalidTokenId, appleRed.key);
    EXPECT_STREQ("green", at(tokens, appleGreen.value).cstrref().str().c_str());
    EXPECT_EQ(NEO::Yaml::invalidTokenId, appleGreen.key);
    EXPECT_STREQ("blue", at(tokens, appleBlue.value).cstrref().str().c_str());
    EXPECT_EQ(NEO::Yaml::invalidTokenId, appleBlue.key);

    EXPECT_STREQ("pear", at(tokens, pear.key).cstrref().str().c_str());
    EXPECT_STREQ("pearish", at(tokens, pear.value).cstrref().str().c_str());
}

TEST(YamlBuildTree, WhenTabsAreUsedAfterIndentWasParsedThenTreatThemAsSeparators) {
    ConstStringRef yaml =
        R"===(
    banana	:	yellow
    apple	:	
        -	red
        -	green
        -	
          types	:	
             -	rome
             -	cameo
          flavors	:	
             -	sweet
             -	bitter
    orange :	orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    ASSERT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(13U, treeNodes.size());

    auto &rootNode = *treeNodes.begin();
    ASSERT_EQ(3U, rootNode.numChildren);
    auto &banana = at(treeNodes, rootNode.firstChildId);
    auto &apple = at(treeNodes, banana.nextSiblingId);
    auto &orange = at(treeNodes, apple.nextSiblingId);
    auto &appleRed = at(treeNodes, apple.firstChildId);
    auto &appleGreen = at(treeNodes, appleRed.nextSiblingId);
    auto &appleDict = at(treeNodes, appleGreen.nextSiblingId);
    auto &appleTypes = at(treeNodes, appleDict.firstChildId);
    auto &appleTypesRome = at(treeNodes, appleTypes.firstChildId);
    auto &appleTypesCameo = at(treeNodes, appleTypesRome.nextSiblingId);
    auto &appleFlavors = at(treeNodes, appleTypes.nextSiblingId);
    auto &appleFlavorsSweet = at(treeNodes, appleFlavors.firstChildId);
    auto &appleFlavorsBitter = at(treeNodes, appleFlavorsSweet.nextSiblingId);

    EXPECT_STREQ("banana", at(tokens, banana.key).cstrref().str().c_str());
    EXPECT_STREQ("yellow", at(tokens, banana.value).cstrref().str().c_str());

    EXPECT_STREQ("orange", at(tokens, orange.key).cstrref().str().c_str());
    EXPECT_STREQ("orange", at(tokens, orange.value).cstrref().str().c_str());

    EXPECT_STREQ("apple", at(tokens, apple.key).cstrref().str().c_str());
    EXPECT_STREQ("red", at(tokens, appleRed.value).cstrref().str().c_str());
    EXPECT_STREQ("green", at(tokens, appleGreen.value).cstrref().str().c_str());
    EXPECT_EQ(NEO::Yaml::invalidTokenId, appleDict.key);
    EXPECT_STREQ("types", at(tokens, appleTypes.key).cstrref().str().c_str());
    EXPECT_STREQ("rome", at(tokens, appleTypesRome.value).cstrref().str().c_str());
    EXPECT_STREQ("cameo", at(tokens, appleTypesCameo.value).cstrref().str().c_str());
    EXPECT_STREQ("flavors", at(tokens, appleFlavors.key).cstrref().str().c_str());
    EXPECT_STREQ("sweet", at(tokens, appleFlavorsSweet.value).cstrref().str().c_str());
    EXPECT_STREQ("bitter", at(tokens, appleFlavorsBitter.value).cstrref().str().c_str());
}

TEST(YamlBuildTree, GivenInvalidIndentationThenBuldingOfTreeFails) {
    ConstStringRef invalidListIndentYaml =
        R"===(
    - red
   - green
  - blue
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(invalidListIndentYaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [2] : [- ] <-- parser position on error. Reason : Invalid indentation\n", errors.c_str());
    EXPECT_TRUE(warnings.empty());

    ConstStringRef invalidDictIndentYaml =
        R"===(
    colors:
        - red
      flavors : sweet
)===";

    lines.clear();
    tokens.clear();
    warnings.clear();
    errors.clear();
    success = NEO::Yaml::tokenize(invalidDictIndentYaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    treeNodes.clear();
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [3] : [fl] <-- parser position on error. Reason : Invalid indentation\n", errors.c_str());
    EXPECT_TRUE(warnings.empty());
}

TEST(YamlTreeFindChildByKey, WhenChildWithKeyDoesNotExistThenReturnsNull) {
    ConstStringRef yaml =
        R"===(
    banana : yellow
    apple : 
        - red
        - green
        - 
          types :
             - rome
             - cameo
          flavors :
             - sweet
             - bitter
    orange : orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    auto success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);

    auto grapes = NEO::Yaml::findChildByKey(*treeNodes.begin(), treeNodes, tokens, "grapes");
    auto apple = NEO::Yaml::findChildByKey(*treeNodes.begin(), treeNodes, tokens, "apple");
    EXPECT_STREQ("apple", at(tokens, apple->key).cstrref().str().c_str());
    EXPECT_EQ(nullptr, grapes);
    ASSERT_NE(nullptr, apple);
    auto appleTraits = NEO::Yaml::findChildByKey(*apple, treeNodes, tokens, "traits");
    EXPECT_EQ(nullptr, appleTraits);
}

TEST(YamlTreeFindChildByKey, WhenChildWithKeyDoesExistThenReturnsIt) {
    ConstStringRef yaml =
        R"===(
    banana : yellow
    apple : 
        - red
        - green
        - 
          types :
             - rome
             - cameo
          flavors :
             - sweet
             - bitter
    orange : orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    auto success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);

    auto apple = NEO::Yaml::findChildByKey(*treeNodes.begin(), treeNodes, tokens, "apple");
    ASSERT_NE(nullptr, apple);
    EXPECT_STREQ("apple", at(tokens, apple->key).cstrref().str().c_str());
}

TEST(YamlBuildTree, GivenCommentInDictionaryEntryThenDonTreatItAsValue) {
    ConstStringRef yaml =
        R"===(
   apple : # red
      - green
      - yellow
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    ASSERT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    auto apple = NEO::Yaml::findChildByKey(*treeNodes.begin(), treeNodes, tokens, "apple");
    ASSERT_NE(nullptr, apple);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, apple->value);
    ASSERT_EQ(2U, apple->numChildren);
}

TEST(YamlBuildTree, GivenCommentInListEntryThenDonTreatItAsValue) {
    ConstStringRef yaml =
        R"===(
      - #green
      - yellow
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    bool success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    ASSERT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    auto root = &*treeNodes.begin();
    ASSERT_NE(nullptr, root);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, root->value);
    ASSERT_EQ(2U, root->numChildren);

    auto appleFirstEntry = NEO::Yaml::getFirstChild(*root, treeNodes);
    EXPECT_EQ(NEO::Yaml::invalidTokenId, appleFirstEntry->value);
}

TEST(YamlTreeGetFirstChild, WhenChildExistsThenReturnsIt) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : 
        - yellow
        - green
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    auto success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);

    auto apple = NEO::Yaml::getFirstChild(*treeNodes.begin(), treeNodes);
    ASSERT_NE(nullptr, apple);
    auto appleRed = NEO::Yaml::getFirstChild(*apple, treeNodes);
    ASSERT_NE(nullptr, appleRed);
    auto appleRedChild = NEO::Yaml::getFirstChild(*appleRed, treeNodes);
    EXPECT_EQ(nullptr, appleRedChild);

    EXPECT_STREQ("apple", at(tokens, apple->key).cstrref().str().c_str());
    EXPECT_STREQ("red", at(tokens, appleRed->value).cstrref().str().c_str());
    EXPECT_EQ(apple->id, appleRed->parentId);
}

TEST(YamlTreeGetLastChild, WhenChildExistsThenReturnsIt) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : 
        - yellow
        - green
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    auto success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);

    auto banana = NEO::Yaml::getLastChild(*treeNodes.begin(), treeNodes);
    ASSERT_NE(nullptr, banana);
    auto bananaGreen = NEO::Yaml::getLastChild(*banana, treeNodes);
    ASSERT_NE(nullptr, bananaGreen);
    auto bananaGreenChild = NEO::Yaml::getLastChild(*bananaGreen, treeNodes);
    EXPECT_EQ(nullptr, bananaGreenChild);

    EXPECT_STREQ("banana", at(tokens, banana->key).cstrref().str().c_str());
    EXPECT_STREQ("green", at(tokens, bananaGreen->value).cstrref().str().c_str());
    EXPECT_EQ(banana->id, bananaGreen->parentId);
}

TEST(YamlTreeConstSiblingsFwdIterator, WhenConstructedThenSetsUpProperMembers) {
    struct WhiteBoxConstSiblingsFwdIterator : ConstSiblingsFwdIterator {
        using ConstSiblingsFwdIterator::allNodes;
        using ConstSiblingsFwdIterator::ConstSiblingsFwdIterator;
        using ConstSiblingsFwdIterator::currId;
    };

    NEO::Yaml::NodesCache nodesCache;
    WhiteBoxConstSiblingsFwdIterator it(1U, &nodesCache);
    EXPECT_EQ(&nodesCache, it.allNodes);
    EXPECT_EQ(1U, it.currId);

    WhiteBoxConstSiblingsFwdIterator it2{it};
    EXPECT_EQ(&nodesCache, it2.allNodes);
    EXPECT_EQ(1U, it2.currId);

    WhiteBoxConstSiblingsFwdIterator it3(7U, &nodesCache + 1);
    it3 = it;
    EXPECT_EQ(&nodesCache, it2.allNodes);
    EXPECT_EQ(1U, it2.currId);
}

TEST(YamlTreeConstSiblingsFwdIterator, WhenComparisonOperatorsAreUsedThenComparesMembers) {
    NEO::Yaml::NodesCache nodesCache;
    nodesCache.resize(2);
    ConstSiblingsFwdIterator first{0U, &nodesCache};
    ConstSiblingsFwdIterator second{1U, &nodesCache};
    ConstSiblingsFwdIterator firstAgain{0U, &nodesCache};
    EXPECT_TRUE(first == first);
    EXPECT_TRUE(second == second);
    EXPECT_TRUE(firstAgain == firstAgain);
    EXPECT_TRUE(first == firstAgain);
    EXPECT_TRUE(firstAgain == first);
    EXPECT_FALSE(first == second);
    EXPECT_FALSE(second == first);
    EXPECT_FALSE(firstAgain == second);
    EXPECT_FALSE(second == firstAgain);

    EXPECT_FALSE(first != first);
    EXPECT_FALSE(second != second);
    EXPECT_FALSE(firstAgain != firstAgain);
    EXPECT_FALSE(first != firstAgain);
    EXPECT_FALSE(firstAgain != first);
    EXPECT_TRUE(first != second);
    EXPECT_TRUE(second != first);
    EXPECT_TRUE(firstAgain != second);
    EXPECT_TRUE(second != firstAgain);
}

TEST(YamlTreeConstSiblingsFwdIterator, WhenDereferenceOperatorsAreUsedThenReturnsReferenceToProperNode) {
    NEO::Yaml::NodesCache nodesCache;
    nodesCache.resize(2);
    ConstSiblingsFwdIterator first{0U, &nodesCache};
    ConstSiblingsFwdIterator second{1U, &nodesCache};
    EXPECT_EQ(nodesCache.begin(), &*first);
    EXPECT_EQ(nodesCache.begin() + 1, &*second);

    nodesCache.begin()->id = 7U;
    (nodesCache.begin() + 1)->id = 13U;
    EXPECT_EQ(7U, first->id);
    EXPECT_EQ(13U, second->id);
}

TEST(YamlTreeConstSiblingsFwdIterator, WhenIncrementOperatorsAreUsedThenAdvancesInTheCollection) {
    NEO::Yaml::NodesCache nodesCache;
    nodesCache.resize(2);
    nodesCache[0].key = 3;
    nodesCache[1].key = 7;

    ConstSiblingsFwdIterator end{NEO::Yaml::invalidNodeID, &nodesCache};
    {
        ConstSiblingsFwdIterator it{0U, &nodesCache};
        auto it2 = it++;
        ASSERT_NE(end, it2);
        EXPECT_EQ(3U, it2->key);
        EXPECT_EQ(end, it);
        auto it3 = it++;
        EXPECT_EQ(end, it);
        EXPECT_EQ(end, it3);
    }

    {
        ConstSiblingsFwdIterator it{0U, &nodesCache};
        auto next = ++it;
        EXPECT_EQ(end, next);
        EXPECT_EQ(end, it);
        auto further = ++it;
        EXPECT_EQ(end, further);
        EXPECT_EQ(end, it);
    }

    nodesCache[0].nextSiblingId = 1U;
    {
        ConstSiblingsFwdIterator it{0U, &nodesCache};
        auto it2 = it++;
        ASSERT_NE(end, it);
        ASSERT_NE(end, it2);
        EXPECT_EQ(3U, it2->key);
        EXPECT_EQ(7U, it->key);
        auto it3 = it++;
        ASSERT_NE(end, it2);
        ASSERT_NE(end, it3);
        EXPECT_EQ(3U, it2->key);
        EXPECT_EQ(7U, it3->key);
        EXPECT_EQ(end, it);
    }

    {
        ConstSiblingsFwdIterator it{0U, &nodesCache};
        auto next = ++it;
        ASSERT_NE(end, next);
        ASSERT_NE(end, it);
        EXPECT_EQ(7U, it->key);
        EXPECT_EQ(7U, next->key);
        auto third = ++next;
        ASSERT_NE(end, it);
        EXPECT_EQ(7U, it->key);
        EXPECT_EQ(end, next);
        EXPECT_EQ(end, third);
    }
}

TEST(YamlTreeConstChildrenRange, WhenConstructedThenSetsUpProperMembers) {
    NEO::Yaml::NodesCache nodesCache;
    nodesCache.resize(2);
    nodesCache[0].id = 0U;
    nodesCache[0].key = 7U;
    ConstChildrenRange rangeByNode{*nodesCache.begin(), nodesCache};
    ConstChildrenRange rangeByNodeId{0U, nodesCache};

    {
        auto beg = rangeByNode.begin();
        auto end = rangeByNode.end();
        EXPECT_NE(beg, end);
        EXPECT_EQ(7U, beg->key);
        EXPECT_EQ(end, ++beg);
    }

    {
        auto beg = rangeByNodeId.begin();
        auto end = rangeByNodeId.end();
        EXPECT_NE(beg, end);
        EXPECT_EQ(7U, beg->key);
        EXPECT_EQ(end, ++beg);
    }
}

TEST(YamlTreeDebugNode, WhenConstructedThenSetsUpProperMembers) {
    NEO::Yaml::DebugNode debugNode;
    EXPECT_TRUE(debugNode.key.empty());
    EXPECT_TRUE(debugNode.children.empty());
    EXPECT_TRUE(debugNode.value.empty());
    EXPECT_EQ(nullptr, debugNode.parent);
    EXPECT_EQ(nullptr, debugNode.src);
}

TEST(YamlTreeBuildDebugNodes, GivenTreeNodesThenBuildsDebugFriendlyRepresentation) {
    ConstStringRef yaml =
        R"===(
    banana : yellow
    apple : 
        - red
        - green
        - 
          types :
             - rome
             - cameo
          flavors :
             - sweet
             - bitter
    orange : orange
)===";

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string warnings;
    std::string errors;
    auto success = NEO::Yaml::tokenize(yaml, lines, tokens, errors, warnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, errors, warnings);
    EXPECT_TRUE(success);

    auto rootDebugNode = NEO::Yaml::buildDebugNodes(treeNodes.begin()->id, treeNodes, tokens);
    EXPECT_TRUE(rootDebugNode->key.empty());
    EXPECT_TRUE(rootDebugNode->value.empty());
    EXPECT_EQ(nullptr, rootDebugNode->parent);
    ASSERT_EQ(3U, rootDebugNode->children.size());

    EXPECT_STREQ("banana", rootDebugNode->children[0]->key.str().c_str());
    EXPECT_STREQ("yellow", rootDebugNode->children[0]->value.str().c_str());
    EXPECT_STREQ("apple", rootDebugNode->children[1]->key.str().c_str());
    EXPECT_EQ(3U, rootDebugNode->children[1]->children.size());
    EXPECT_STREQ("orange", rootDebugNode->children[2]->key.str().c_str());
    EXPECT_STREQ("orange", rootDebugNode->children[2]->value.str().c_str());

    delete rootDebugNode;
}

TEST(YamlParser, WhenConstructedThenSetsUpProperDefaults) {
    NEO::Yaml::YamlParser parser;
    EXPECT_TRUE(parser.empty());
    EXPECT_EQ(nullptr, parser.buildDebugNodes());
}

TEST(YamlParserParse, WhenTokenizerFailsThenParserPropagatesTheError) {
    ConstStringRef yaml = "\"aaaa";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_FALSE(success);
    EXPECT_TRUE(parser.empty());

    std::string tokenizerErrors;
    std::string tokenizerWarnings;
    NEO::Yaml::TokensCache tokens;
    NEO::Yaml::LinesCache lines;
    success = NEO::Yaml::tokenize(yaml, lines, tokens, tokenizerErrors, tokenizerWarnings);
    EXPECT_FALSE(success);

    EXPECT_STREQ(tokenizerErrors.c_str(), parserErrors.c_str());
    EXPECT_STREQ(tokenizerWarnings.c_str(), parserWarnings.c_str());
}

TEST(YamlParserParse, WhenTreeBuildingFailsThenParserPropagatesTheError) {
    ConstStringRef yaml =
        R"===(
    - red
   - green
  - blue
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_FALSE(success);
    EXPECT_TRUE(parser.empty());

    NEO::Yaml::LinesCache lines;
    NEO::Yaml::TokensCache tokens;
    std::string tokenizerWarnings;
    std::string tokenizerErrors;
    success = NEO::Yaml::tokenize(yaml, lines, tokens, tokenizerErrors, tokenizerWarnings);
    EXPECT_TRUE(success);

    NEO::Yaml::NodesCache treeNodes;
    std::string treeBuildingWarnings;
    std::string treeBuildingErrors;
    success = NEO::Yaml::buildTree(lines, tokens, treeNodes, treeBuildingErrors, treeBuildingWarnings);
    EXPECT_FALSE(success);
    EXPECT_STREQ(treeBuildingErrors.c_str(), parserErrors.c_str());
    EXPECT_STREQ(treeBuildingWarnings.c_str(), parserWarnings.c_str());
}

TEST(YamlParserParse, GivenValidYamlThenParsesItCorrectly) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : 
        - yellow
        - green
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parserErrors.empty());
    EXPECT_TRUE(parserWarnings.empty());
}

TEST(YamlParser, WhenGetRootIsCalledThenReturnsFirstNode) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : 
        - yellow
        - green
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    ASSERT_NE(nullptr, parser.getRoot());
    EXPECT_EQ(0U, parser.getRoot()->id);
}

TEST(YamlParser, WhenGetChildIsCalledThenSearchesForChildNodeByKey) {
    ConstStringRef yaml =
        R"===(
    banana : yellow
    apple : 
        - red
        - green
        - 
          types :
             - rome
             - cameo
          flavors :
             - sweet
             - bitter
    orange : orange
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    ASSERT_NE(nullptr, parser.getRoot());
    EXPECT_EQ(0U, parser.getRoot()->id);

    auto apple = parser.getChild(*parser.getRoot(), "apple");
    ASSERT_NE(nullptr, apple);
    EXPECT_EQ(3U, apple->numChildren);
    EXPECT_NE(parser.getRoot(), apple);
}

TEST(YamlParser, GivenNodeWhenReadKeyIsCalledThenReturnsStringRepresentationOfKey) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : 
        - yellow
        - green
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto banana = parser.getChild(*parser.getRoot(), "banana");
    ASSERT_NE(nullptr, banana);
    EXPECT_STREQ("banana", parser.readKey(*banana).str().c_str());
}

TEST(YamlParser, GivenNodeWhenReadValueIsCalledThenReturnsStringRepresentationOfKey) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : yellow
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto banana = parser.getChild(*parser.getRoot(), "banana");
    ASSERT_NE(nullptr, banana);
    EXPECT_STREQ("yellow", parser.readValue(*banana).str().c_str());

    auto apple = parser.getChild(*parser.getRoot(), "apple");
    EXPECT_STREQ("", parser.readValue(*apple).str().c_str());
}

TEST(YamlParser, GivenNodeWhenGetValueTokenIsCalledThenReturnsTokenRepResettingTheValue) {
    ConstStringRef yaml =
        R"===(
    apple : 
        - red
        - green
    banana : yellow
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto apple = parser.getChild(*parser.getRoot(), "apple");
    ASSERT_NE(nullptr, apple);
    EXPECT_EQ(nullptr, parser.getValueToken(*apple));

    auto banana = parser.getChild(*parser.getRoot(), "banana");
    ASSERT_NE(nullptr, banana);
    auto bananaValueToken = parser.getValueToken(*banana);
    ASSERT_NE(nullptr, bananaValueToken);
    EXPECT_EQ('y', bananaValueToken->traits.character0);
}

TEST(YamlParser, WhenFindNodeWithKeyDfsIsCalledThenSearchesForFirstMatchUsingDfs) {
    ConstStringRef yaml =
        R"===(
    apple : 
        color : red
        taste : bitter
    banana :
        color : yellow
        taste : sweet
    color : green
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto appleColor = parser.findNodeWithKeyDfs("color");
    ASSERT_NE(nullptr, appleColor);
    EXPECT_STREQ("color", parser.readKey(*appleColor).str().c_str());
    EXPECT_STREQ("red", parser.readValue(*appleColor).str().c_str());

    EXPECT_EQ(nullptr, parser.findNodeWithKeyDfs("colors"));
}

TEST(YamlParser, GivenNodeWhenCreateChildrenRangeIsCalledThenCreatesProperRange) {
    ConstStringRef yaml =
        R"===(
    apple : 
        color : red
        taste : bitter
    banana :
        color : yellow
        taste : sweet
    color : green
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto root = parser.getRoot();
    auto childrenRange = parser.createChildrenRange(*root);
    auto it = childrenRange.begin();
    ASSERT_NE(childrenRange.end(), it);
    EXPECT_STREQ("apple", parser.readKey(*it).str().c_str());
    ++it;
    ASSERT_NE(childrenRange.end(), it);
    EXPECT_STREQ("banana", parser.readKey(*it).str().c_str());
    ++it;
    ASSERT_NE(childrenRange.end(), it);
    EXPECT_STREQ("color", parser.readKey(*it).str().c_str());
    ++it;
    EXPECT_EQ(childrenRange.end(), it);
}

TEST(YamlParser, GivenNodeWithoutChildrenWhenCreateChildrenRangeIsCalledThenCreatesEmptyRange) {
    ConstStringRef yaml = "- a";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto root = parser.getRoot();
    ASSERT_EQ(1U, root->numChildren);
    auto first = parser.createChildrenRange(*root).begin();
    EXPECT_EQ(0U, first->numChildren);
    auto range = parser.createChildrenRange(*first);
    EXPECT_EQ(range.end(), range.begin());
}

TEST(YamlParser, WhenBuildDebugNodesIsCalledThenBuildsDebugFriendlyRepresentation) {
    ConstStringRef yaml =
        R"===(
    banana : yellow
    apple : 
        - red
        - green
        - 
          types :
             - rome
             - cameo
          flavors :
             - sweet
             - bitter
    orange : orange
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto rootDebugNode = parser.buildDebugNodes();
    EXPECT_TRUE(rootDebugNode->key.empty());
    EXPECT_TRUE(rootDebugNode->value.empty());
    EXPECT_EQ(nullptr, rootDebugNode->parent);
    ASSERT_EQ(3U, rootDebugNode->children.size());

    EXPECT_STREQ("banana", rootDebugNode->children[0]->key.str().c_str());
    EXPECT_STREQ("yellow", rootDebugNode->children[0]->value.str().c_str());
    EXPECT_STREQ("apple", rootDebugNode->children[1]->key.str().c_str());
    EXPECT_EQ(3U, rootDebugNode->children[1]->children.size());
    EXPECT_STREQ("orange", rootDebugNode->children[2]->key.str().c_str());
    EXPECT_STREQ("orange", rootDebugNode->children[2]->value.str().c_str());

    delete rootDebugNode;

    auto flavors = parser.findNodeWithKeyDfs("flavors");
    ASSERT_NE(nullptr, flavors);
    auto flavorsDebugNode = parser.buildDebugNodes(*flavors);
    ASSERT_NE(nullptr, flavorsDebugNode);
    ASSERT_EQ(2U, flavorsDebugNode->children.size());
    EXPECT_STREQ("flavors", flavorsDebugNode->key.str().c_str());
    EXPECT_TRUE(flavorsDebugNode->value.empty());
    EXPECT_STREQ("sweet", flavorsDebugNode->children[0]->value.str().c_str());
    EXPECT_STREQ("bitter", flavorsDebugNode->children[1]->value.str().c_str());

    delete flavorsDebugNode;
}

TEST(YamlParserReadValueNoQuotes, GivenStringWithQuotesThenReturnSubstring) {
    ConstStringRef yaml = R"===( 
banana : "yellow color" 
apple : 'red color'
)===";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto banana = parser.findNodeWithKeyDfs("banana");
    ASSERT_NE(nullptr, banana);
    EXPECT_STREQ("\"yellow color\"", parser.readValue(*banana).str().c_str());
    EXPECT_STREQ("yellow color", parser.readValueNoQuotes(*banana).str().c_str());

    auto apple = parser.findNodeWithKeyDfs("apple");
    ASSERT_NE(nullptr, apple);
    EXPECT_STREQ("\'red color\'", parser.readValue(*apple).str().c_str());
    EXPECT_STREQ("red color", parser.readValueNoQuotes(*apple).str().c_str());
}

TEST(YamlParserReadValueNoQuotes, GivenStringWithoutQuotesThenReturnStringAsIs) {
    ConstStringRef yaml = R"===( 
banana : yellow
apple :
    - red
)===";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto banana = parser.findNodeWithKeyDfs("banana");
    ASSERT_NE(nullptr, banana);
    EXPECT_STREQ("yellow", parser.readValue(*banana).str().c_str());
    EXPECT_STREQ("yellow", parser.readValueNoQuotes(*banana).str().c_str());

    auto apple = parser.findNodeWithKeyDfs("apple");
    ASSERT_NE(nullptr, apple);
    EXPECT_STREQ("", parser.readValueNoQuotes(*apple).str().c_str());
}

TEST(YamlParserReadValueNoQuotes, GivenNonStringValueThenReturnTextRepresentationAsIs) {
    ConstStringRef yaml = R"===( 
banana : 5
)===";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto banana = parser.findNodeWithKeyDfs("banana");
    ASSERT_NE(nullptr, banana);
    EXPECT_STREQ("5", parser.readValueNoQuotes(*banana).str().c_str());
}

TEST(YamlParserReadValueCheckedInt64, GivenNodeWithoutASingleValueThenReturnFalse) {
    ConstStringRef yaml = R"===( 
list : 
   - a
   - b
)===";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto notIntNode = parser.getChild(*parser.getRoot(), "list");
    ASSERT_NE(notIntNode, nullptr);
    int64_t readInt64 = 0;
    auto readSuccess = parser.readValueChecked<int64_t>(*notIntNode, readInt64);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedint64, GivenNonIntegerValueThenReturnFalse) {
    ConstStringRef yaml = "not_integer : five";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto notIntNode = parser.getChild(*parser.getRoot(), "not_integer");
    ASSERT_NE(notIntNode, nullptr);
    int64_t readInt64 = 0;
    auto readSuccess = parser.readValueChecked<int64_t>(*notIntNode, readInt64);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedInt64, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = "positive : 9223372036854775807\nnegative : -9223372036854775807";
    int64_t expectedInt64 = 9223372036854775807;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto positiveNode = parser.getChild(*parser.getRoot(), "positive");
    auto negativeNode = parser.getChild(*parser.getRoot(), "negative");
    ASSERT_NE(positiveNode, nullptr);
    ASSERT_NE(negativeNode, nullptr);
    int64_t readPositive = 0;
    int64_t readNegative = 0;
    auto readSuccess = parser.readValueChecked<int64_t>(*positiveNode, readPositive);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedInt64, readPositive);
    readSuccess = parser.readValueChecked<int64_t>(*negativeNode, readNegative);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(-expectedInt64, readNegative);
}

TEST(YamlParserReadValueCheckedInt32, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===( 
positive64 : 9223372036854775807
negative64 : -9223372036854775807
positive32 : 2147483647
negative32 : -2147483647
)===";
    int32_t expectedInt32 = 2147483647;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto positive32Node = parser.getChild(*parser.getRoot(), "positive32");
    auto negative32Node = parser.getChild(*parser.getRoot(), "negative32");
    ASSERT_NE(positive32Node, nullptr);
    ASSERT_NE(negative32Node, nullptr);
    int32_t readPositive = 0;
    int32_t readNegative = 0;
    auto readSuccess = parser.readValueChecked<int32_t>(*positive32Node, readPositive);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedInt32, readPositive);
    readSuccess = parser.readValueChecked<int32_t>(*negative32Node, readNegative);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(-expectedInt32, readNegative);

    auto positive64Node = parser.getChild(*parser.getRoot(), "positive64");
    auto negative64Node = parser.getChild(*parser.getRoot(), "negative64");
    readSuccess = parser.readValueChecked<int32_t>(*positive64Node, readPositive);
    EXPECT_FALSE(readSuccess);
    readSuccess = parser.readValueChecked<int32_t>(*negative64Node, readNegative);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedInt16, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===( 
positive32 : 2147483647
negative32 : -2147483647
positive16 : 32767
negative16 : -32767
)===";
    int16_t expectedInt16 = 32767;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto positive16Node = parser.getChild(*parser.getRoot(), "positive16");
    auto negative16Node = parser.getChild(*parser.getRoot(), "negative16");
    ASSERT_NE(positive16Node, nullptr);
    ASSERT_NE(negative16Node, nullptr);
    int16_t readPositive = 0;
    int16_t readNegative = 0;
    auto readSuccess = parser.readValueChecked<int16_t>(*positive16Node, readPositive);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedInt16, readPositive);
    readSuccess = parser.readValueChecked<int16_t>(*negative16Node, readNegative);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(-expectedInt16, readNegative);

    auto positive32Node = parser.getChild(*parser.getRoot(), "positive32");
    auto negative32Node = parser.getChild(*parser.getRoot(), "negative32");
    readSuccess = parser.readValueChecked<int16_t>(*positive32Node, readPositive);
    EXPECT_FALSE(readSuccess);
    readSuccess = parser.readValueChecked<int16_t>(*negative32Node, readNegative);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedInt8, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===( 
positive16 : 65530
negative16 : -65530
positive8 : 127
negative8 : -127
)===";
    int8_t expectedInt8 = 127;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto positive8Node = parser.getChild(*parser.getRoot(), "positive8");
    auto negative8Node = parser.getChild(*parser.getRoot(), "negative8");
    ASSERT_NE(positive8Node, nullptr);
    ASSERT_NE(negative8Node, nullptr);
    int8_t readPositive = 0;
    int8_t readNegative = 0;
    auto readSuccess = parser.readValueChecked<int8_t>(*positive8Node, readPositive);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedInt8, readPositive);
    readSuccess = parser.readValueChecked<int8_t>(*negative8Node, readNegative);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(-expectedInt8, readNegative);

    auto positive16Node = parser.getChild(*parser.getRoot(), "positive16");
    auto negative16Node = parser.getChild(*parser.getRoot(), "negative16");
    readSuccess = parser.readValueChecked<int8_t>(*positive16Node, readPositive);
    EXPECT_FALSE(readSuccess);
    readSuccess = parser.readValueChecked<int8_t>(*negative16Node, readNegative);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedUint64, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = "positive : 6294967295";
    uint64_t expectedUint64 = 6294967295;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto positiveNode = parser.getChild(*parser.getRoot(), "positive");
    ASSERT_NE(positiveNode, nullptr);
    uint64_t readPositive = 0;
    auto readSuccess = parser.readValueChecked<uint64_t>(*positiveNode, readPositive);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedUint64, readPositive);
}

TEST(YamlParserReadValueCheckedUint64, GivenNegativeIntegerThenFail) {
    ConstStringRef yaml = "integer64 : -10";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto int64Node = parser.getChild(*parser.getRoot(), "integer64");
    ASSERT_NE(int64Node, nullptr);
    uint64_t readUint64 = 0;
    auto readSuccess = parser.readValueChecked<uint64_t>(*int64Node, readUint64);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedUint32, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===( 
integer64 : 6294967295
integer32 : 294967295
)===";
    uint32_t expectedUint32 = 294967295U;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto int64Node = parser.getChild(*parser.getRoot(), "integer64");
    ASSERT_NE(int64Node, nullptr);
    uint32_t readUint32 = 0;
    auto readSuccess = parser.readValueChecked<uint32_t>(*int64Node, readUint32);
    EXPECT_FALSE(readSuccess);

    auto int32Node = parser.getChild(*parser.getRoot(), "integer32");
    ASSERT_NE(int32Node, nullptr);
    readUint32 = 0;
    readSuccess = parser.readValueChecked<uint32_t>(*int32Node, readUint32);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedUint32, readUint32);
}

TEST(YamlParserReadValueCheckedUint32, GivenNegativeIntegerThenFail) {
    ConstStringRef yaml = "integer32 : -10";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto int32Node = parser.getChild(*parser.getRoot(), "integer32");
    ASSERT_NE(int32Node, nullptr);
    uint32_t readUint32 = 0;
    auto readSuccess = parser.readValueChecked<uint32_t>(*int32Node, readUint32);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedUint16, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===( 
integer32 : 294967295
integer16 : 65530
)===";

    uint16_t expectedUint16 = 65530U;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto int32Node = parser.getChild(*parser.getRoot(), "integer32");
    ASSERT_NE(int32Node, nullptr);
    uint16_t readUint16 = 0;
    auto readSuccess = parser.readValueChecked<uint16_t>(*int32Node, readUint16);
    EXPECT_FALSE(readSuccess);

    auto int16Node = parser.getChild(*parser.getRoot(), "integer16");
    ASSERT_NE(int16Node, nullptr);
    readSuccess = parser.readValueChecked<uint16_t>(*int16Node, readUint16);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedUint16, readUint16);
}

TEST(YamlParserReadValueCheckedUint16, GivenNegativeIntegerThenFail) {
    ConstStringRef yaml = "integer16 : -10";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto int16Node = parser.getChild(*parser.getRoot(), "integer16");
    ASSERT_NE(int16Node, nullptr);
    uint16_t readUint16 = 0;
    auto readSuccess = parser.readValueChecked<uint16_t>(*int16Node, readUint16);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedUint8, GivenIntegerThenParsesItCorrectly) {
    ConstStringRef yaml = R"===( 
integer16 : 65530
integer8 : 250
)===";

    uint8_t expectedUint8 = 250U;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    auto int16Node = parser.getChild(*parser.getRoot(), "integer16");
    ASSERT_NE(int16Node, nullptr);
    uint8_t readUint8 = 0;
    auto readSuccess = parser.readValueChecked<uint8_t>(*int16Node, readUint8);
    EXPECT_FALSE(readSuccess);

    auto int8Node = parser.getChild(*parser.getRoot(), "integer8");
    ASSERT_NE(int8Node, nullptr);
    readSuccess = parser.readValueChecked<uint8_t>(*int8Node, readUint8);
    EXPECT_TRUE(readSuccess);
    EXPECT_EQ(expectedUint8, readUint8);
}

TEST(YamlParserReadValueCheckedUint8, GivenNegativeIntegerThenFail) {
    ConstStringRef yaml = "integer8 : -10";
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    auto int8Node = parser.getChild(*parser.getRoot(), "integer8");
    ASSERT_NE(int8Node, nullptr);
    uint8_t readUint8 = 0;
    auto readSuccess = parser.readValueChecked<uint8_t>(*int8Node, readUint8);
    EXPECT_FALSE(readSuccess);
}

TEST(YamlParserReadValueCheckedBool, GivenFlagsThenParsesThemCorrectly) {
    ConstStringRef yaml = R"===( 
ones_that_are_true :
    y : y
    Y : y

    yes : yes
    Yes : Yes
    YES : YES

    true : true
    True : True
    TRUE : TRUE

    on : on
    ON : ON

ones_that_are_false :
    n : n
    N : N

    no : no
    No : No
    NO : NO

    false : false
    False : False
    FALSE : FALSE

    off : off
    Off : Off
    OFF : OFF
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    uint32_t expectedNumberOfPositiveCases = 10;
    uint32_t expectedNumberOfNegativeCases = 11;

    auto onesThatAreTrue = parser.getChild(*parser.getRoot(), "ones_that_are_true");
    auto onesThatAreFalse = parser.getChild(*parser.getRoot(), "ones_that_are_false");
    ASSERT_NE(nullptr, onesThatAreTrue);
    ASSERT_NE(nullptr, onesThatAreFalse);

    EXPECT_EQ(expectedNumberOfPositiveCases, onesThatAreTrue->numChildren);
    EXPECT_EQ(expectedNumberOfNegativeCases, onesThatAreFalse->numChildren);

    for (auto &node : parser.createChildrenRange(*onesThatAreTrue)) {
        bool value = false;
        bool readSuccess = false;
        readSuccess = parser.readValueChecked<bool>(node, value);
        EXPECT_TRUE(readSuccess);
        EXPECT_TRUE(value) << parser.readValue(node).str();
    }

    for (auto &node : parser.createChildrenRange(*onesThatAreFalse)) {
        bool value = false;
        bool readSuccess = false;
        readSuccess = parser.readValueChecked<bool>(node, value);
        EXPECT_TRUE(readSuccess);
        EXPECT_FALSE(value) << parser.readValue(node).str();
    }
}

TEST(YamlParserReadValueCheckedBool, GivenNonFlagEntriesThenFailParsing) {
    ConstStringRef yaml = R"===( 
ones_that_look_like_true :
    one : 1

    long_yesssss : long_yesssss

    yess : yess
    Yos  : Yos

    tru  : tru
    truA : truA

    o : o
    og : og

ones_that_look_like_false :
    zero : 0

    None : None
    ni   : ni

    fals  : fals
    falsA : falsA

    of  : of 
    ofo : ofo

    alse : alse

not_really_a_bool:
    - imalist
    - true
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);

    uint32_t expectedNumberOfPositiveCases = 8;
    uint32_t expectedNumberOfNegativeCases = 8;

    auto onesThatLookLikeTrue = parser.getChild(*parser.getRoot(), "ones_that_look_like_true");
    auto onesThatLookLikeFalse = parser.getChild(*parser.getRoot(), "ones_that_look_like_false");
    auto notReallyABool = parser.getChild(*parser.getRoot(), "not_really_a_bool");
    ASSERT_NE(nullptr, onesThatLookLikeTrue);
    ASSERT_NE(nullptr, onesThatLookLikeFalse);
    ASSERT_NE(nullptr, notReallyABool);

    EXPECT_EQ(expectedNumberOfPositiveCases, onesThatLookLikeTrue->numChildren);
    EXPECT_EQ(expectedNumberOfNegativeCases, onesThatLookLikeFalse->numChildren);

    for (auto &node : parser.createChildrenRange(*onesThatLookLikeTrue)) {
        bool value = false;
        bool readSuccess = false;
        readSuccess = parser.readValueChecked<bool>(node, value);
        EXPECT_FALSE(readSuccess) << parser.readValue(node).str();
    }

    for (auto &node : parser.createChildrenRange(*onesThatLookLikeFalse)) {
        bool value = false;
        bool readSuccess = false;
        readSuccess = parser.readValueChecked<bool>(node, value);
        EXPECT_FALSE(readSuccess) << parser.readValue(node).str();
    }

    {
        bool value = false;
        bool readSuccess = false;
        readSuccess = parser.readValueChecked<bool>(*notReallyABool, value);
        EXPECT_FALSE(readSuccess) << parser.readValue(*notReallyABool).str();
    }
}

TEST(YamlParser, GivenSimpleZebinThenParsesItCorrectly) {
    ConstStringRef yaml = R"===(---
kernels:         
  - name:            k
    execution_env:   
      grf_count:       128
      has_no_stateless_write: true
      simd_size:       32
      subgroup_independent_forward_progress: false
    payload_arguments: 
      - arg_type:        global_id_offset
        offset:          0
        size:            12
      - arg_type:        local_size
        offset:          12
        size:            12
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       0
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
      - arg_type:        arg_bypointer
        offset:          32
        size:            8
        arg_index:       0
        addrmode:        stateless
        addrspace:       global
        access_type:     readwrite
    per_thread_payload_arguments: 
      - arg_type:        local_id
        offset:          0
        size:            192
    binding_table_indexes: 
      - bti_value:       0
        arg_index:       0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(success);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;

    auto debugNodes = parser.buildDebugNodes();
    ASSERT_EQ(1U, debugNodes->children.size());
    delete debugNodes;

    const NEO::Yaml::Node *ndKernels = parser.findNodeWithKeyDfs("kernels");
    ASSERT_NE(nullptr, ndKernels);
    ASSERT_EQ(1U, ndKernels->numChildren);
    const NEO::Yaml::Node *ndKernel0 = &*parser.createChildrenRange(*ndKernels).begin();
    ASSERT_NE(nullptr, ndKernel0);
    const NEO::Yaml::Node *ndName = parser.getChild(*ndKernel0, "name");
    const NEO::Yaml::Node *ndExecutionEnv = parser.getChild(*ndKernel0, "execution_env");
    const NEO::Yaml::Node *ndPayloadArgs = parser.getChild(*ndKernel0, "payload_arguments");
    const NEO::Yaml::Node *ndPerThreadPayloadArgs = parser.getChild(*ndKernel0, "per_thread_payload_arguments");
    const NEO::Yaml::Node *ndBtis = parser.getChild(*ndKernel0, "binding_table_indexes");
    ASSERT_NE(nullptr, ndName);
    ASSERT_NE(nullptr, ndExecutionEnv);
    ASSERT_NE(nullptr, ndPayloadArgs);
    ASSERT_NE(nullptr, ndPerThreadPayloadArgs);
    ASSERT_NE(nullptr, ndBtis);
    EXPECT_STREQ("k", parser.readValue(*ndName).str().c_str());
    { // exec env
        auto ndGrfCount = parser.getChild(*ndExecutionEnv, "grf_count");
        auto ndHasNoStatelessWrite = parser.getChild(*ndExecutionEnv, "has_no_stateless_write");
        auto ndSimdSize = parser.getChild(*ndExecutionEnv, "simd_size");
        auto ndSubgroupIfp = parser.getChild(*ndExecutionEnv, "subgroup_independent_forward_progress");
        ASSERT_NE(nullptr, ndGrfCount);
        ASSERT_NE(nullptr, ndHasNoStatelessWrite);
        ASSERT_NE(nullptr, ndSimdSize);
        ASSERT_NE(nullptr, ndSubgroupIfp);
        uint32_t grfCount;
        bool hasNoStatelessWrite;
        uint32_t simdSize;
        bool subgroupIfp;
        EXPECT_TRUE(parser.readValueChecked(*ndGrfCount, grfCount));
        EXPECT_TRUE(parser.readValueChecked(*ndHasNoStatelessWrite, hasNoStatelessWrite));
        EXPECT_TRUE(parser.readValueChecked(*ndSimdSize, simdSize));
        EXPECT_TRUE(parser.readValueChecked(*ndSubgroupIfp, subgroupIfp));
        EXPECT_EQ(128U, grfCount);
        EXPECT_TRUE(hasNoStatelessWrite);
        EXPECT_EQ(32U, simdSize);
        EXPECT_FALSE(subgroupIfp);
    }

    { // payload arguments
        ASSERT_EQ(4U, ndPayloadArgs->numChildren);
        auto argIt = parser.createChildrenRange(*ndPayloadArgs).begin();
        auto *ndArg0 = &*(argIt++);
        auto *ndArg1 = &*(argIt++);
        auto *ndArg2 = &*(argIt++);
        auto *ndArg3 = &*argIt;
        ASSERT_NE(nullptr, ndArg0);
        ASSERT_NE(nullptr, ndArg1);
        ASSERT_NE(nullptr, ndArg2);
        ASSERT_NE(nullptr, ndArg3);

        { // arg0
            auto ndArgType = parser.getChild(*ndArg0, "arg_type");
            auto ndArgOffset = parser.getChild(*ndArg0, "offset");
            auto ndArgSize = parser.getChild(*ndArg0, "size");
            ASSERT_NE(nullptr, ndArgType);
            ASSERT_NE(nullptr, ndArgOffset);
            ASSERT_NE(nullptr, ndArgSize);
            uint32_t argOffset = 0U;
            uint32_t argSize = 0U;
            EXPECT_TRUE(parser.readValueChecked(*ndArgOffset, argOffset));
            EXPECT_TRUE(parser.readValueChecked(*ndArgSize, argSize));
            EXPECT_STREQ("global_id_offset", parser.readValue(*ndArgType).str().c_str());
            EXPECT_EQ(0U, argOffset);
            EXPECT_EQ(12U, argSize);
        }

        { // arg1
            auto ndArgType = parser.getChild(*ndArg1, "arg_type");
            auto ndArgOffset = parser.getChild(*ndArg1, "offset");
            auto ndArgSize = parser.getChild(*ndArg1, "size");
            ASSERT_NE(nullptr, ndArgType);
            ASSERT_NE(nullptr, ndArgOffset);
            ASSERT_NE(nullptr, ndArgSize);
            uint32_t argOffset = 0U;
            uint32_t argSize = 0U;
            EXPECT_TRUE(parser.readValueChecked(*ndArgOffset, argOffset));
            EXPECT_TRUE(parser.readValueChecked(*ndArgSize, argSize));
            EXPECT_STREQ("local_size", parser.readValue(*ndArgType).str().c_str());
            EXPECT_EQ(12U, argOffset);
            EXPECT_EQ(12U, argSize);
        }

        { // arg2
            auto ndArgType = parser.getChild(*ndArg2, "arg_type");
            auto ndArgOffset = parser.getChild(*ndArg2, "offset");
            auto ndArgSize = parser.getChild(*ndArg2, "size");
            auto ndArgIndex = parser.getChild(*ndArg2, "arg_index");
            auto ndAddrMode = parser.getChild(*ndArg2, "addrmode");
            auto ndAddrSpace = parser.getChild(*ndArg2, "addrspace");
            auto ndAccessType = parser.getChild(*ndArg2, "access_type");
            ASSERT_NE(nullptr, ndArgType);
            ASSERT_NE(nullptr, ndArgOffset);
            ASSERT_NE(nullptr, ndArgSize);
            ASSERT_NE(nullptr, ndArgIndex);
            ASSERT_NE(nullptr, ndAddrMode);
            ASSERT_NE(nullptr, ndAddrSpace);
            ASSERT_NE(nullptr, ndAccessType);
            uint32_t argOffset = 0U;
            uint32_t argSize = 0U;
            uint32_t argIndex = 0U;
            EXPECT_TRUE(parser.readValueChecked(*ndArgOffset, argOffset));
            EXPECT_TRUE(parser.readValueChecked(*ndArgSize, argSize));
            EXPECT_TRUE(parser.readValueChecked(*ndArgIndex, argIndex));
            EXPECT_STREQ("arg_bypointer", parser.readValue(*ndArgType).str().c_str());
            EXPECT_STREQ("stateful", parser.readValue(*ndAddrMode).str().c_str());
            EXPECT_STREQ("global", parser.readValue(*ndAddrSpace).str().c_str());
            EXPECT_STREQ("readwrite", parser.readValue(*ndAccessType).str().c_str());
            EXPECT_EQ(0U, argOffset);
            EXPECT_EQ(0U, argSize);
            EXPECT_EQ(0U, argIndex);
        }

        { // arg3
            auto ndArgType = parser.getChild(*ndArg3, "arg_type");
            auto ndArgOffset = parser.getChild(*ndArg3, "offset");
            auto ndArgSize = parser.getChild(*ndArg3, "size");
            auto ndArgIndex = parser.getChild(*ndArg3, "arg_index");
            auto ndAddrMode = parser.getChild(*ndArg3, "addrmode");
            auto ndAddrSpace = parser.getChild(*ndArg3, "addrspace");
            auto ndAccessType = parser.getChild(*ndArg3, "access_type");
            ASSERT_NE(nullptr, ndArgType);
            ASSERT_NE(nullptr, ndArgOffset);
            ASSERT_NE(nullptr, ndArgSize);
            ASSERT_NE(nullptr, ndArgIndex);
            ASSERT_NE(nullptr, ndAddrMode);
            ASSERT_NE(nullptr, ndAddrSpace);
            ASSERT_NE(nullptr, ndAccessType);
            uint32_t argOffset = 0U;
            uint32_t argSize = 0U;
            uint32_t argIndex = 0U;
            EXPECT_TRUE(parser.readValueChecked(*ndArgOffset, argOffset));
            EXPECT_TRUE(parser.readValueChecked(*ndArgSize, argSize));
            EXPECT_TRUE(parser.readValueChecked(*ndArgIndex, argIndex));
            EXPECT_STREQ("arg_bypointer", parser.readValue(*ndArgType).str().c_str());
            EXPECT_STREQ("stateless", parser.readValue(*ndAddrMode).str().c_str());
            EXPECT_STREQ("global", parser.readValue(*ndAddrSpace).str().c_str());
            EXPECT_STREQ("readwrite", parser.readValue(*ndAccessType).str().c_str());
            EXPECT_EQ(32U, argOffset);
            EXPECT_EQ(8U, argSize);
            EXPECT_EQ(0U, argIndex);
        }
    }

    { // per-thread payload arguments
        ASSERT_EQ(1U, ndPerThreadPayloadArgs->numChildren);
        auto argIt = parser.createChildrenRange(*ndPerThreadPayloadArgs).begin();
        auto *ndArg0 = &*argIt;
        ASSERT_NE(nullptr, ndArg0);

        { // arg0
            auto ndArgType = parser.getChild(*ndArg0, "arg_type");
            auto ndArgOffset = parser.getChild(*ndArg0, "offset");
            auto ndArgSize = parser.getChild(*ndArg0, "size");
            ASSERT_NE(nullptr, ndArgType);
            ASSERT_NE(nullptr, ndArgOffset);
            ASSERT_NE(nullptr, ndArgSize);
            uint32_t argOffset = 0U;
            uint32_t argSize = 0U;
            EXPECT_TRUE(parser.readValueChecked(*ndArgOffset, argOffset));
            EXPECT_TRUE(parser.readValueChecked(*ndArgSize, argSize));
            EXPECT_STREQ("local_id", parser.readValue(*ndArgType).str().c_str());
            EXPECT_EQ(0U, argOffset);
            EXPECT_EQ(192U, argSize);
        }
    }

    { // binding_table_indexes
        ASSERT_EQ(1U, ndBtis->numChildren);
        auto argIt = parser.createChildrenRange(*ndBtis).begin();
        auto *ndBti0 = &*argIt;
        ASSERT_NE(nullptr, ndBti0);

        { // bti0
            auto ndBtiValue = parser.getChild(*ndBti0, "bti_value");
            auto ndArgIndex = parser.getChild(*ndBti0, "arg_index");
            ASSERT_NE(nullptr, ndBtiValue);
            ASSERT_NE(nullptr, ndArgIndex);
            uint32_t btiValue = 0U;
            uint32_t argIndex = 0U;
            EXPECT_TRUE(parser.readValueChecked(*ndBtiValue, btiValue));
            EXPECT_TRUE(parser.readValueChecked(*ndArgIndex, argIndex));
            EXPECT_EQ(0U, btiValue);
            EXPECT_EQ(0U, argIndex);
        }
    }
}

TEST(ReserveBasedOnEstimates, WhenContainerNotFullThenDontGrow) {
    StackVec<int, 7> container;
    size_t beg = 0U;
    size_t pos = 10U;
    size_t end = 1000U;
    bool reservedAdditionalMem = reserveBasedOnEstimates(container, beg, end, pos);
    EXPECT_FALSE(reservedAdditionalMem);
    EXPECT_EQ(7U, container.capacity());
}

TEST(ReserveBasedOnEstimates, WhenContainerFullButPosIsBegThenDontGrow) {
    StackVec<int, 7> container;
    container.resize(7U);
    ASSERT_EQ(container.capacity(), container.size());
    size_t beg = 0U;
    size_t pos = 0U;
    size_t end = 1000U;
    bool reservedAdditionalMem = reserveBasedOnEstimates(container, beg, end, pos);
    EXPECT_FALSE(reservedAdditionalMem);
    EXPECT_EQ(7U, container.capacity());
}

TEST(ReserveBasedOnEstimates, WhenContainerFullThenGrowBasedOnPos) {
    StackVec<int, 7> container;
    container.resize(7U);
    ASSERT_EQ(container.capacity(), container.size());
    size_t beg = 0U;
    size_t pos = 25U;
    size_t end = 1000U;
    bool reservedAdditionalMem = reserveBasedOnEstimates(container, beg, end, pos);
    EXPECT_TRUE(reservedAdditionalMem);
    EXPECT_EQ(280U, container.capacity());
}
