#ifndef RUNTYPE_HPP
#define RUNTYPE_HPP

#include <functional>
#include <variant>
#include <list>
#include <map>

namespace runtype {

template <typename ... U>
struct Pack {};

namespace detail {

    template <typename T>
    struct always_false : std::false_type {};

    template <typename T>
    constexpr auto hasValueMember(T x) -> decltype(x.value, std::true_type{}) {
        return {};
    }
    constexpr auto hasValueMember(...) -> std::false_type {
        return {};
    }
} // namespace detail

template <typename ... U>
class Basic {
public:
    using Variant_t = std::variant<U...>;
    using Types = Pack<U...>;
private:
    Variant_t v_;
public:
    template <typename T>
    Basic(const T& rhs) : v_(rhs) {}

    std::ostream& write(std::ostream& os) const {
        std::visit([&os](auto&& arg) { os << arg.value; }, v_);
        return os;
    }

    std::istream& read(std::istream& is) {
        std::visit([&is](auto&& arg) { is >> arg.value; }, v_);
        return is;
    }

    template <typename T>
    auto get() {
        if constexpr (detail::hasValueMember(T())) {
            return std::get<T>(v_).value;
        } else {
            static_assert(detail::always_false<T>::value, "No value member");
        }
    }

    template <typename T>
    static Basic<U...> create(std::istream& is) {
        Basic<U...> b = Basic<U...>(T());
        b.read(is);
        return b;
    }
};

template <typename ... U>
std::ostream& operator<<(std::ostream& os, const Basic<U...>& b) {
    return b.write(os);
}

template <typename ... U>
std::istream& operator>>(std::istream& is, Basic<U...>& b) {
    return b.read(is);
}

template <typename T>
using TypeMap_t = std::map<std::string, std::function<T(std::istream&)>>;

template <typename T, typename S>
void registerType(TypeMap_t<S>& typeMap, const std::string& name) {
    typeMap[name] = [](std::istream& is) {
        return S::template create<T>(is);
    };
}

template <typename ... U>
Basic<U...> newBasic(const std::string& type, std::istream& is,
        const TypeMap_t<Basic<U...>> typeMap) {
    return typeMap.at(type)(is);
}

namespace detail {
    template <typename B, typename Last>
    void makeTypeMapImpl(TypeMap_t<B>& m, std::list<std::string> types) {
        registerType<Last>(m, *std::begin(types));
    }

    template <typename B, typename First, typename Second, typename ... Rest>
    void makeTypeMapImpl(TypeMap_t<B>& m, std::list<std::string> types) {
        makeTypeMapImpl<B, First>(m, types);
        types.pop_front();
        makeTypeMapImpl<B, Second, Rest...>(m, types);
    }
} // namespace detail

template <typename ... U>
TypeMap_t<Basic<U...>> makeTypeMap(std::list<std::string> types) {
    TypeMap_t<Basic<U...>> m;
    detail::makeTypeMapImpl<Basic<U...>, U...>(m, types);
    return m;
}

template <typename ... U>
TypeMap_t<Basic<U...>> makeTypeMap(Pack<U...>, std::list<std::string> types) {
    return makeTypeMap<U...>(types);
}
} // namespace runtype

#endif // RUNTYPE_HPP
