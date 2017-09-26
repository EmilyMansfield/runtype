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
    // Used to pass parameter packs as arguments to help type deduction
    template <typename ... U>
    struct Pack {};

    // Add a new type to a TypeMap_t unless the name already exists, in
    // which case do nothing
    template <typename T, typename S>
    void registerType(TypeMap_t<S>& typeMap, const std::string& name) {
        typeMap.try_emplace(name, [](std::istream& is) {
            return S::template create<T>(is);
        });
    }

    // Base case for makeTypeMap recursion
    // Register the remaining type
    template <typename B, typename Last>
    inline void makeTypeMapImpl(TypeMap_t<B>& m, std::list<std::string> types) {
        registerType<Last>(m, *std::begin(types));
    }

    // Induction step for makeTypeMap recursion
    // Register the next type, then register the remaining ones
    template <typename B, typename First, typename Second, typename ... Rest>
    inline void makeTypeMapImpl(TypeMap_t<B>& m, std::list<std::string> types) {
        makeTypeMapImpl<B, First>(m, types);
        types.pop_front();
        makeTypeMapImpl<B, Second, Rest...>(m, types);
    }

    // Constructs a new TypeMap_t with keys given by types and values
    // correspoing to the types in the Pack
    template <typename R, typename ... U>
    inline TypeMap_t<Basic<R, U...>> makeTypeMap(detail::Pack<U...>, std::list<std::string> types) {
        TypeMap_t<Basic<R, U...>> m;
        detail::makeTypeMapImpl<Basic<R, U...>, U...>(m, types);
        return m;
    }
} // namespace detail

// R is any `Resolver` class, namely any class with a static function
// with signature
//
//     static std::function<<Basic<R,U...>(std::istream&)> at(const std::string&);
//
// that maps type names to functions that construct a Basic<R,U...> with
// the given type when passed an input stream. See the BasicResolver
// class for a simple example using a std::map.
//
// The parameter pack U contains the possible types that can be stored.
// There are the following requirements on parameters in U:
//   - Any template parameter T used in the interface must be an
//     instance of one of the Us, although this should be caught at
//     compile time.
//   - The following functions should be defined for each T
//
//     std::ostream& operator<<(std::ostream&, const T&);
//     std::istream& operator>>(std::istream&, T&);
//
//   - Each T must be default constructible.
template <typename R, typename ... U>
class Basic {
public:
    using Variant = std::variant<U...>;
    using Types = detail::Pack<U...>;
    using Resolver = R;
private:
    Variant v_;
public:
    // Construct from one of the underlying types
    template <typename T>
    Basic(const T& rhs) : v_(rhs) {}

    // Write current value to a stream
    std::ostream& write(std::ostream& os) const {
        std::visit([&os](auto&& arg) { os << arg; }, v_);
        return os;
    }

    // Read from a stream, under the assumption that the stream contains
    // data of the same type currently stored
    std::istream& read(std::istream& is) {
        std::visit([&is](auto&& arg) { is >> arg; }, v_);
        return is;
    }

    // Get the underlying value
    // throws std::bad_variant_access if the currently stored value is
    // not of type T
    template <typename T>
    auto get() {
        return std::get<T>(v_);
    }

    // Construct a new Basic containing a T from an input stream
    template <typename T>
    static Basic<R, U...> create(std::istream& is) {
        Basic<R, U...> b = Basic<R, U...>(T());
        b.read(is);
        return b;
    }

    // Construct a new Basic containing a type from an input stream,
    // using the Resolver to resolve the type string
    static Basic<R, U...> create(const std::string& type, std::istream& is) {
        return Resolver::at(type)(is);
    }
};

template <typename R, typename ... U>
std::ostream& operator<<(std::ostream& os, const Basic<R, U...>& b) {
    return b.write(os);
}

template <typename R, typename ... U>
std::istream& operator>>(std::istream& is, Basic<R, U...>& b) {
    return b.read(is);
}

// If B is a Basic<R, U...>, then construct a type map mapping the given
// types to the U...
template <typename B>
inline TypeMap_t<B> makeTypeMap(std::list<std::string> types) {
    return detail::makeTypeMap<typename B::Resolver>(typename B::Types(), types);
}

// Example implementation of a type resolver. Uses a static variable
// that must be explicitly specialized by the user. It is
// convenient to use the BasicWithDefaultResolver alias to save
// duplicating template parameters. For example,
//
//     using B = BasicWithDefaultResolver<int, double>;
//     using BR = B::Resolver;
//     template <>
//     const B::Resolver::MapType B::Resolver::map
//      = makeTypeMap<B>({"int", "double"});
//
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
