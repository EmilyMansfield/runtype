#include "catch.hpp"
#include "runtype.hpp"

using namespace runtype;
using StringIntMap = detail::OrderPreservingMap<std::string, int>;

TEST_CASE("Constructible from an initializer_list", "[OrderPreservingMap]") {
    StringIntMap opm({{"z", 1}, {"a", 4}, {"p", 3}});
    REQUIRE(opm.at("z") == 1);
    REQUIRE(opm.at("a") == 4);
    REQUIRE(opm.at("p") == 3);
}

TEST_CASE("Has queryable emptiness", "[OrderPreservingMap]") {
    StringIntMap empty;
    REQUIRE(empty.empty() == true);

    StringIntMap notEmpty({{"a", 1}});
    REQUIRE(notEmpty.empty() == false);

    notEmpty.clear();
    REQUIRE(notEmpty.empty() == true);
}

TEST_CASE("Has queryable size", "[OrderPreservingMap]") {
    StringIntMap empty;
    REQUIRE(empty.size() == 0);

    StringIntMap notEmpty({{"a", 1}});
    REQUIRE(notEmpty.size() == 1);

    notEmpty.clear();
    REQUIRE(notEmpty.size() == 0);
}

TEST_CASE("Can be inserted into", "[OrderPreservingMap]") {
    StringIntMap opm;
    opm.insert({"z", 1});
    opm.insert({"a", 4});
    opm.insert({"p", 3});
    REQUIRE(opm.at("z") == 1);
    REQUIRE(opm.at("a") == 4);
    REQUIRE(opm.at("p") == 3);
}

TEST_CASE("at(nonexistent) throws", "[OrderPreservingMap]") {
    StringIntMap opm;
    REQUIRE_THROWS_AS(opm.at("a"), std::out_of_range);
}

TEST_CASE("[] accesses values", "[OrderPreservingMap]") {
    StringIntMap opm({{"z", 1}, {"a", 4}, {"p", 3}});

    auto x = opm["a"];
    REQUIRE(x == 4);

    auto& y = opm["a"];
    y = 5;
    REQUIRE(opm.at("a") == 5);

    opm["a"] = 7;
    REQUIRE(opm.at("a") == 7);
}

TEST_CASE("Reinsertion should not modify", "[OrderPreservingMap]") {
    StringIntMap opm({{"a", 1}});

    opm.insert({"a", 2});
    REQUIRE(opm.at("a") == 1);

    auto pair = std::make_pair<std::string, int>("a", 2);
    opm.insert(pair);
    REQUIRE(opm.at("a") == 1);

    opm.emplace("a", 2);
    REQUIRE(opm.at("a") == 1);

    std::string key = "a";
    opm.emplace(key, 1);
    REQUIRE(opm.at("a") == 1);

    opm.try_emplace("a", 2);
    REQUIRE(opm.at("a") == 1);
}

TEST_CASE("Iteration should be in insertion order", "[OrderPreservingMap]") {
    using Pair = std::pair<const std::string, int>;
    std::initializer_list<Pair> init = {{"z", 1}, {"a", 4}, {"p", 3}};
    StringIntMap opm = init;
    std::vector<Pair> output;
    std::vector<Pair> expected = init;

    for (const auto& p : opm) {
        output.push_back(p);
    }
    REQUIRE(output == expected);
    output.clear();

    for (auto& p : opm) {
        output.push_back(p);
    }
    REQUIRE(output == expected);
}
