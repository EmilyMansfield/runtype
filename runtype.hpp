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

template <typename R, typename ... U>
class Basic {
public:
    using Variant_t = std::variant<U...>;
    using Types = Pack<U...>;
    using Resolver_t = R;
private:
    Variant_t v_;
    static Resolver_t r_;
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
        return r_.at(type)(is);
        //return Resolver::at(type)(is);
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

template <typename T>
using TypeMap_t = std::map<std::string, std::function<T(std::istream&)>>;

template <typename T, typename S>
void registerType(TypeMap_t<S>& typeMap, const std::string& name) {
    typeMap[name] = [](std::istream& is) {
        return S::template create<T>(is);
    };
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

template <typename R, typename ... U>
TypeMap_t<Basic<R, U...>> makeTypeMap(std::list<std::string> types) {
    TypeMap_t<Basic<R, U...>> m;
    detail::makeTypeMapImpl<Basic<R, U...>,  U...>(m, types);
    return m;
}

template <typename R, typename ... U>
TypeMap_t<Basic<R, U...>> makeTypeMap(Pack<U...>, std::list<std::string> types) {
    return makeTypeMap<R, U...>(types);
}

template <typename ... U>
class BasicResolver {
    using B = Basic<BasicResolver<U...>, U...>;
    TypeMap_t<B> map_;
public:
    auto at(const std::string& s) { return map_.at(s); }
    BasicResolver(std::list<std::string> s) {
        map_ = makeTypeMap<BasicResolver<U...>, U...>(s);
    }
};

} // namespace runtype

#endif // RUNTYPE_HPP
