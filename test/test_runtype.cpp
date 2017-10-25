#include "catch.hpp"
#include "runtype.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>

using namespace runtype;

// BasicResolver is templated over its type options and contains a
// static member, which will be unique up to those arguments. To get
// around that and allow duplicate Resolvers with the same types but
// different keys, we append one of these with the name "void"
template <int I> struct Blank {};
template <int I> std::ostream& operator<<(std::ostream& os, const Blank<I>&) {
    return os;
}
template <int I> std::istream& operator>>(std::istream& is, Blank<I>&) {
    return is;
}

using B = BasicWithDefaultResolver<int, double, std::string, Blank<0>>;
using BR = B::Resolver;
template <>
const BR::BasicMapType BR::basicTypes = makeTypeMap<B>(
    {"int", "double", "string", "void"});
template <> BR::CompoundMapType BR::compoundTypes = {};

using B2 = BasicWithDefaultResolver<double, int, float, Blank<1>>;
using B2R = B2::Resolver;
template <>
const B2R::BasicMapType B2R::basicTypes = makeTypeMap<B2>(
    {"a", "a", "b", "void"});

TEST_CASE("Checks basic types", "[BasicResolver]") {
    REQUIRE(BR::isBasicType("int"));
    REQUIRE(BR::isBasicType("double"));
    REQUIRE(BR::isBasicType("string"));
    REQUIRE_FALSE(BR::isBasicType("foo"));
    REQUIRE_FALSE(BR::isBasicType(""));
}

TEST_CASE("Resolves basic types", "[BasicResolver]") {
    std::stringstream intStream("10");
    std::stringstream doubleStream("3.14");
    std::stringstream stringStream("test");

    B intB = BR::resolveBasic("int")(intStream);
    REQUIRE(intB.get<int>() == 10);

    B doubleB = BR::resolveBasic("double")(doubleStream);
    REQUIRE(doubleB.get<double>() == 3.14);

    B stringB = BR::resolveBasic("string")(stringStream);
    REQUIRE(stringB.get<std::string>() == "test");

    REQUIRE_THROWS_AS(BR::resolveBasic("foo"), std::out_of_range);
    REQUIRE_THROWS_AS(BR::resolveBasic(""), std::out_of_range);
}

TEST_CASE("Duplicate basic type keys are ignored", "[BasicResolver]") {
    std::stringstream doubleStream("5.9");
    std::stringstream intStream("6");
    std::stringstream floatStream("3.5");

    REQUIRE(B2R::resolveBasic("a")(doubleStream).get<double>() == 5.9);
    REQUIRE_THROWS_AS(
        B2R::resolveBasic("a")(intStream).get<int>(), std::bad_variant_access);
    REQUIRE(B2R::resolveBasic("b")(floatStream).get<float>() == 3.5);
}

TEST_CASE("Can instantiate basics", "[Basic]") {
    std::stringstream intStream("10");

    intStream.seekg(0);
    auto intFromString = B::create("int", intStream);
    REQUIRE(intFromString.get<int>() == 10);

    intStream.seekg(0);
    auto intFromTemplate = B::create<int>(intStream);
    REQUIRE(intFromString.get<int>() == 10);

    intStream.seekg(0);
    REQUIRE_THROWS_AS(B::create("", intStream), std::out_of_range);

    B intFromT(7);
    REQUIRE(intFromT.get<int>() == 7);

    detail::TypeInstance& bPtr = intFromT;
    B bFromTypeInstance(bPtr);
    REQUIRE(bFromTypeInstance.get<int>() == intFromT.get<int>());
}

// To avoid dependencies between test cases and to prevent accidental
// attempts to redefine types, only use the compounds in this namespace.
// The disconnect between definition and usage is better than the
// alternative of requiring every test involving compounds to know about
// every other.
// If you need a new type for a test, add it to the struct and not the
// test. Any compound type used in a test must still be added in that
// test, do not assume it has already been added to a resolver.
namespace TestTypes {

const static auto emptyType = CompoundType("emptyType", {});
const static auto fakeEmptyType =
    CompoundType("fakeEmptyType", {{"i", {"int"}}});
const static auto duplicateEmptyType = CompoundType("emptyType", {});

const static auto singleIntType =
    CompoundType("singleIntType", {{"i", {"int"}}});
const static auto fakeBasicType = CompoundType("int", {});

const static auto multiType = CompoundType("multiType",
    {{"i", {"int"}},
        {"d", {"double"}},
        {"s1", {"string"}},
        {"s2", {"string"}}});

const static auto nestedType =
    CompoundType("nestedType", {{"i", {"int"}}, {"m", {"multiType"}}});

const static auto incompleteType =
    CompoundType("incompleteType", {{"m", {"NOT_IMPLEMENTED"}}});

}; // namespace TestTypes

TEST_CASE("Can make compound types", "[CompoundType]") {
    REQUIRE(TestTypes::emptyType.name() == "emptyType");
    REQUIRE(TestTypes::emptyType.members().size() == 0);

    REQUIRE(TestTypes::singleIntType.name() == "singleIntType");
    REQUIRE(TestTypes::singleIntType.members().size() == 1);

    REQUIRE(TestTypes::multiType.name() == "multiType");
    REQUIRE(TestTypes::multiType.members().size() == 4);
}

// clang-format off
TEST_CASE("Can register and lookup compound types",
        "[BasicResolver][CompoundType]") {
    // clang-format on
    BR::registerCompoundType(TestTypes::emptyType);
    REQUIRE(BR::isCompoundType("emptyType"));
    REQUIRE(BR::resolveCompound("emptyType") == TestTypes::emptyType);

    BR::registerCompoundType(TestTypes::multiType);
    REQUIRE(BR::isCompoundType("multiType"));
    REQUIRE(BR::resolveCompound("multiType") == TestTypes::multiType);

    SECTION("Re-adding a type does not modify or throw") {
        REQUIRE_NOTHROW(BR::registerCompoundType(TestTypes::emptyType));
        REQUIRE(BR::isCompoundType("emptyType"));
        REQUIRE(BR::resolveCompound("emptyType") == TestTypes::emptyType);
    }

    SECTION("Adding an identical but separately constructed type does not "
            "modify or throw") {
        REQUIRE(TestTypes::duplicateEmptyType == TestTypes::emptyType);
        REQUIRE_NOTHROW(
            BR::registerCompoundType(TestTypes::duplicateEmptyType));
        REQUIRE(BR::isCompoundType("emptyType"));
        REQUIRE(
            BR::resolveCompound("emptyType") == TestTypes::duplicateEmptyType);
    }

    SECTION("Adding a type with an existing key does not modify or throw") {
        REQUIRE_NOTHROW(BR::registerCompoundType(TestTypes::fakeEmptyType));
        REQUIRE(BR::isCompoundType("emptyType"));
        REQUIRE(BR::resolveCompound("emptyType") == TestTypes::emptyType);
    }

    SECTION("Querying nonexistent types returns false and resolving "
            "nonexistent types throws") {
        REQUIRE_FALSE(BR::isCompoundType("nonexistentType"));
        REQUIRE_FALSE(BR::isCompoundType(""));
        REQUIRE_THROWS_AS(
            BR::resolveCompound("nonexistentType"), std::out_of_range);
        REQUIRE_THROWS_AS(BR::resolveCompound(""), std::out_of_range);
    }

    SECTION("Adding a type with the same name as a Basic throws") {
        REQUIRE_THROWS(BR::registerCompoundType(TestTypes::fakeBasicType));
    }
}

TEST_CASE("Can instantiate compounds", "[CompoundInstance]") {
    std::stringstream ignoreStream("10 Hello");
    std::stringstream intStream("5");
    std::stringstream nestedStream("6 10 3.7 hello world");

    BR::registerCompoundType(TestTypes::emptyType);
    BR::registerCompoundType(TestTypes::singleIntType);
    BR::registerCompoundType(TestTypes::multiType);
    BR::registerCompoundType(TestTypes::nestedType);

    std::unique_ptr<CompoundInstance<BR>> emptyCompound;
    std::unique_ptr<CompoundInstance<BR>> intCompound;
    std::unique_ptr<CompoundInstance<BR>> nestedCompound;

    SECTION("Instantiation using a resolver") {
        emptyCompound =
            std::make_unique<CompoundInstance<BR>>("emptyType", ignoreStream);
        intCompound =
            std::make_unique<CompoundInstance<BR>>("singleIntType", intStream);
        nestedCompound =
            std::make_unique<CompoundInstance<BR>>("nestedType", nestedStream);
    }

    SECTION("Instantiation directly from type") {
        emptyCompound = std::make_unique<CompoundInstance<BR>>(
            TestTypes::emptyType.create<BR>(ignoreStream));
        intCompound = std::make_unique<CompoundInstance<BR>>(
            TestTypes::singleIntType.create<BR>(intStream));
        nestedCompound = std::make_unique<CompoundInstance<BR>>(
            TestTypes::nestedType.create<BR>(nestedStream));
    }

    REQUIRE(ignoreStream.tellg() == 0); // Do not consume
    REQUIRE(emptyCompound->type() == TestTypes::emptyType);
    REQUIRE_THROWS_AS(emptyCompound->get<int>("i"), std::out_of_range);

    REQUIRE(intCompound->type() == TestTypes::singleIntType);
    REQUIRE(intCompound->get<int>("i") == 5);
    REQUIRE_THROWS_AS(intCompound->get<int>("f"), std::out_of_range);

    REQUIRE(nestedCompound->type() == TestTypes::nestedType);
    REQUIRE(nestedCompound->get<int>("i") == 6);
    REQUIRE_THROWS_AS(nestedCompound->get<int>("m"), std::bad_cast);
    auto multi = nestedCompound->get("m");
    REQUIRE(multi.get<int>("i") == 10);
    REQUIRE(multi.get<double>("d") == 3.7);
    REQUIRE(multi.get<std::string>("s1") == "hello");
    REQUIRE(multi.get<std::string>("s2") == "world");
}

TEST_CASE("Cannot instantiate incomplete types", "[CompoundType]") {
    std::stringstream emptyStream("");
    BR::registerCompoundType(TestTypes::incompleteType);

    REQUIRE_THROWS(TestTypes::incompleteType.create<BR>(emptyStream));
    REQUIRE_THROWS(CompoundInstance<BR>("incompleteType", emptyStream));
}
