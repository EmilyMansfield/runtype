#ifndef RUNTYPE_HPP
#define RUNTYPE_HPP

#include <functional>
#include <variant>
#include <list>
#include <map>

namespace runtype {

template <typename T>
using TypeMap_t = std::map<std::string, std::function<T(std::istream&)>>;

template <typename R, typename ... U>
class Basic;

namespace detail {
    template <typename ... U>
    struct Pack {};

    template <typename T>
    struct always_false : std::false_type {};

    template <typename T>
    constexpr auto hasValueMember(T x) -> decltype(x.value, std::true_type{}) {
        return {};
    }
    constexpr auto hasValueMember(...) -> std::false_type {
        return {};
    }

    template <typename T, typename S>
    void registerType(TypeMap_t<S>& typeMap, const std::string& name) {
        typeMap[name] = [](std::istream& is) {
            return S::template create<T>(is);
        };
    }

    template <typename B, typename Last>
    inline void makeTypeMapImpl(TypeMap_t<B>& m, std::list<std::string> types) {
        registerType<Last>(m, *std::begin(types));
    }

    template <typename B, typename First, typename Second, typename ... Rest>
    inline void makeTypeMapImpl(TypeMap_t<B>& m, std::list<std::string> types) {
        makeTypeMapImpl<B, First>(m, types);
        types.pop_front();
        makeTypeMapImpl<B, Second, Rest...>(m, types);
    }

    template <typename R, typename ... U>
    inline TypeMap_t<Basic<R, U...>> makeTypeMap(detail::Pack<U...>, std::list<std::string> types) {
        TypeMap_t<Basic<R, U...>> m;
        detail::makeTypeMapImpl<Basic<R, U...>, U...>(m, types);
        return m;
    }
} // namespace detail

template <typename R, typename ... U>
class Basic {
public:
    using Variant_t = std::variant<U...>;
    using Types = detail::Pack<U...>;
    using Resolver = R;
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
    static Basic<R, U...> create(std::istream& is) {
        Basic<R, U...> b = Basic<R, U...>(T());
        b.read(is);
        return b;
    }

    static Basic<R, U...> create(const std::string& type, std::istream& is) {
        return Resolver::at(type)(is);
    }
};

template <typename R, typename ... U>
std::ostream& operator<<(std::ostream& os, const Basic<R, U...>& b) {
    return b.write(os);
}

template <typename R, typename ... U>
std::istream& operator>>(std::istream& is, Basic<U...>& b) {
    return b.read(is);
}

template <typename B>
inline TypeMap_t<B> makeTypeMap(std::list<std::string> types) {
    return detail::makeTypeMap<typename B::Resolver>(typename B::Types(), types);
}

template <typename ... U>
class BasicResolver {
public:
    using MapType = TypeMap_t<Basic<BasicResolver<U...>, U...>>;
private:
    const static MapType map;
public:
    static auto at(const std::string& s) { return map.at(s); }
};

template <typename ... U>
using BasicWithDefaultResolver = Basic<BasicResolver<U...>, U...>;

} // namespace runtype

#endif // RUNTYPE_HPP
