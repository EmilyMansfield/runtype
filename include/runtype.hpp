#ifndef RUNTYPE_HPP
#define RUNTYPE_HPP

#include <functional>
#include <algorithm>
#include <istream>
#include <ostream>
#include <variant>
#include <memory>
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

    // All type variants (Basic, Compound etc) should derive this
    class TypeInstance {
    public:
        virtual std::ostream& write(std::ostream&) const = 0;
        virtual std::istream& read(std::istream&) = 0;
        virtual ~TypeInstance() {}
        virtual const TypeInstance& operator()(const std::string&) const {
            throw std::runtime_error("Not a compound type");
        }
    };

    template <typename Key, typename T, typename Hash,
             typename KeyEqual, typename Allocator>
    class OrderPreservingMap;

    // Bidirectional iterator class for the OrderPreservingMap.
    // Templated over a boolean to give const and nonconst iterators in
    // one class.
    template <bool IsConst, typename Key, typename T, typename Hash,
             typename KeyEqual, typename Allocator>
    class OrderPreservingMapIteratorImpl {
        friend class OrderPreservingMap<Key, T, Hash, KeyEqual, Allocator>;

        using map_type = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;
        using vec_type = std::vector<typename map_type::iterator>;
        using vec_iterator = typename std::conditional<IsConst,
              typename vec_type::const_iterator,
              typename vec_type::iterator>::type;
        // This should really be const& all the time, since an iterator
        // cannot modify the order. However, std::begin(order_)
        // will always return a const_iterator if order_ is const&.
        // This makes the O(n) constructor awkward, requiring
        // const_casts or preventing the use of std::find (no conversion
        // from the returned const_iterator to an iterator even with
        // const_cast; const_iterator != const iterator.
        // PRs welcome.
        using order_type = typename std::conditional<IsConst,
              const vec_type&, vec_type&>::type;
        // The ordering is kept by the underlying map not the iterator;
        // each iterator needs to be bound to a map which provides the
        // ordering used -- and ++ etc
        order_type order_;
        vec_iterator it_;

        // O(1) creation at the given point in the ordering.
        OrderPreservingMapIteratorImpl(order_type order, vec_iterator it)
            : order_(order), it_(it) {}

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename std::conditional<IsConst,
              const typename map_type::value_type,
              typename map_type::value_type>::type;
        using pointer = typename std::conditional<IsConst,
              typename map_type::const_pointer,
              typename map_type::pointer>::type;
        using reference = typename std::conditional<IsConst,
              typename map_type::const_reference,
              typename map_type::reference>::type;
        using iterator_category = std::bidirectional_iterator_tag;

        // O(n) creation using a map iterator, e.g. if the key is known
        OrderPreservingMapIteratorImpl(order_type order,
                const typename map_type::iterator& it)
            : order_(order) {
            it_ = std::find(std::begin(order_), std::end(order_), it);
        }

        reference operator*() {
            // it_ is an iterator to an iterator
            return **it_;
        }

        reference operator*() const {
            return **it_;
        }

        typename map_type::iterator operator->() {
            // Pass through to the map iterator's -> operator to get
            // member access for free
            return *it_;
        }

        typename map_type::iterator operator->() const {
            return *it_;
        }

        OrderPreservingMapIteratorImpl& operator++() {
            ++it_;
            return *this;
        }

        OrderPreservingMapIteratorImpl operator++(int) {
            auto tmp(*this);
            operator++();
            return tmp;
        }

        OrderPreservingMapIteratorImpl& operator--() {
            --it_;
            return *this;
        }

        OrderPreservingMapIteratorImpl operator--(int) {
            auto tmp(*this);
            operator--();
            return tmp;
        }

        bool operator==(const OrderPreservingMapIteratorImpl& rhs) {
            return it_ == rhs.it_;
        }

        bool operator!=(const OrderPreservingMapIteratorImpl& rhs) {
            return !operator==(rhs);
        }
    };

    template <typename Key, typename T, typename Hash, typename KeyEqual, typename Allocator>
    using OrderPreservingMapIterator = OrderPreservingMapIteratorImpl<false, Key, T, Hash, KeyEqual, Allocator>;

    template <typename Key, typename T, typename Hash, typename KeyEqual, typename Allocator>
    using OrderPreservingMapConstIterator = OrderPreservingMapIteratorImpl<true, Key, T, Hash, KeyEqual, Allocator>;

    // std::map sorts with < on the key by default, and
    // std::unordered_map is well, unordered, but we need to iterate
    // over the keys in insertion order. With std::map this necessitates
    // including order information in the keys, which leaves the
    // handling to the user(!). This abstracts it away by
    // providing an interface matching std::unordered_map with (average)
    // O(1) lookup, but with iteration in insertion order.
    template <typename Key,
             typename T,
             typename Hash = std::hash<Key>,
             typename KeyEqual = std::equal_to<Key>,
             typename Allocator = std::allocator<std::pair<const Key, T>>>
    class OrderPreservingMap {
        using map_type = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;
        using vec_type = std::vector<typename map_type::iterator>;
        using map_iterator = typename map_type::iterator;
        using const_map_iterator = typename map_type::const_iterator;

    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<const Key, T>;
        using size_type = typename map_type::size_type;
        using difference_type = typename map_type::difference_type;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using allocator_type = Allocator;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename std::allocator_traits<Allocator>::pointer;
        using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
        using iterator = OrderPreservingMapIterator<Key, T, Hash, KeyEqual, Allocator>;
        using const_iterator = OrderPreservingMapConstIterator<Key, T, Hash, KeyEqual, Allocator>;

    private:
        map_type map_;
        // The iterator at index i points is the (i+1)-th element that
        // was added
        vec_type vec_;

        // Insert the iterator into the tracker if the bool is true,
        // otherwise do nothing
        inline void track_insert(const std::pair<map_iterator, bool>& p) {
            auto [it, success] = p;
            if (success) vec_.push_back(it);
        }

        // Convert an unordered_map iterator/bool pair to an ordered one
        inline std::pair<iterator, bool>
        order_iterator(const std::pair<map_iterator, bool>& p) {
            return std::make_pair(iterator(vec_, p.first), p.second);
        }

    public:

        OrderPreservingMap() = default;
        OrderPreservingMap(const OrderPreservingMap& rhs) {
            for (const auto& x : rhs) {
                emplace(x);
            }
        }

        // Construct from an initializer_list of key,value pairs
        OrderPreservingMap(std::initializer_list<value_type> init,
                size_type bucket_count = 0,
                const hasher& hash = hasher(),
                const key_equal& equal = key_equal(),
                const allocator_type& alloc = allocator_type())
            : map_(bucket_count, hash, equal, alloc) {
            for (const auto& x : init) {
                emplace(x);
            }
        }

        iterator begin() {
            return iterator(vec_, std::begin(vec_));
        }

        iterator end() {
            return iterator(vec_, std::end(vec_));
        }

        const_iterator begin() const {
            return const_iterator(vec_, std::cbegin(vec_));
        }

        const_iterator end() const {
            return const_iterator(vec_, std::cend(vec_));
        }

        bool empty() const noexcept {
            return map_.empty();
        }

        size_type size() const noexcept {
            return map_.size();
        }

        size_type max_size() const noexcept {
            return map_.max_size();
        }

        void clear() noexcept {
            map_.clear();
            vec_.clear();
        }

        std::pair<iterator, bool> insert(const value_type& value) {
            auto p = map_.insert(value);
            track_insert(p);
            return order_iterator(p);
        }

        template <typename P>
        std::pair<iterator, bool> insert(P&& value) {
            auto p = map_.insert(std::forward<P>(value));
            track_insert(p);
            return order_iterator(p);
        }

        template <typename ... Args>
        std::pair<iterator, bool> try_emplace(const key_type& k, Args&& ... args) {
            auto p = map_.try_emplace(k, std::forward<Args>(args) ...);
            track_insert(p);
            return order_iterator(p);
        }

        template <typename ... Args>
        std::pair<iterator, bool> try_emplace(key_type&& k, Args&& ... args) {
            auto p = map_.try_emplace(std::forward<key_type>(k), std::forward<Args>(args) ...);
            track_insert(p);
            return order_iterator(p);
        }

        template <typename ... Args>
        std::pair<iterator, bool> emplace(Args&& ... args) {
            auto p = map_.emplace(std::forward<Args>(args) ...);
            track_insert(p);
            return order_iterator(p);
        }

        T& at(const key_type& key) {
            return map_.at(key);
        }

        const T& at(const key_type& key) const {
            return map_.at(key);
        }

        T& operator[](const key_type& key) {
            // Thanks cppreference for the implementation
            return try_emplace(key).first->second;
        }

        T& operator[](key_type&& key) {
            return try_emplace(std::move(key)).first->second;
        }

        // Equality respects insertion order
        friend inline bool operator==(
                const OrderPreservingMap& lhs,
                const OrderPreservingMap& rhs)
        {
            if (lhs.size() != rhs.size()) return false;
            for (auto lhs_it = std::begin(lhs), rhs_it = std::begin(rhs);
                    lhs_it != std::end(lhs) && rhs_it != std::end(rhs);
                    ++lhs_it, ++rhs_it) {
                if (lhs_it->first != rhs_it->first || lhs_it->second != lhs_it->second) {
                    return false;
                }
            }
            return true;
        }

        friend inline bool operator!=(
                const OrderPreservingMap& lhs,
                const OrderPreservingMap& rhs)
        {
            return !operator==(lhs, rhs);
        }
    };

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
//   - U shall contain no duplicate types
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
class Basic : public detail::TypeInstance {
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

    Basic(const detail::TypeInstance& rhs)
        : Basic(dynamic_cast<const Basic<R, U...>&>(rhs)) {}

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
    const T& get() const {
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
        return Resolver::resolveBasic(type)(is);
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

template <typename R>
class CompoundInstance;

class CompoundType {
public:
    struct Member {
        std::string type;
    };

private:
    std::string name_;
    using container_type = detail::OrderPreservingMap<std::string, Member>;
    const container_type members_;

public:
    CompoundType(const std::string& name,
            std::initializer_list<container_type::value_type> l)
        : name_(name), members_(l) {}

    const container_type& members() const {
        return members_;
    }

    std::string name() const {
        return name_;
    }

    template <typename R>
    CompoundInstance<R> create(std::istream& is) const {
        return CompoundInstance<R>(name_, is);
    }

    friend inline bool operator==(const CompoundType& lhs, const CompoundType& rhs) {
        return lhs.name() == rhs.name() && lhs.members_ == rhs.members_;
    }

    friend inline bool operator!=(const CompoundType& lhs, const CompoundType& rhs) {
        return !operator==(lhs, rhs);
    }

    friend inline bool operator==(const Member& lhs, const Member& rhs) {
        return lhs.type == rhs.type;
    }

    friend inline bool operator!=(const Member& lhs, const Member& rhs) {
        return !operator==(lhs, rhs);
    }
};

template <typename R>
class CompoundInstance : public detail::TypeInstance {
    using member_type = std::unique_ptr<detail::TypeInstance>;
    using container_type = detail::OrderPreservingMap<std::string, member_type>;
    using Resolver = R;
    const CompoundType& type_;
    container_type members_;

public:

    CompoundInstance(const CompoundInstance<R>& rhs) : type_(rhs.type_) {
        for (const auto& [name, m] : rhs.members_) {
            if (auto mPtr = dynamic_cast<typename R::BasicType*>(m.get())) {
                members_.emplace(name, std::make_unique<typename R::BasicType>(*mPtr));
            } else if (auto mPtr = dynamic_cast<CompoundInstance<R>*>(m.get())) {
                members_.emplace(name, std::make_unique<CompoundInstance<R>>(*mPtr));
            } else {
                throw std::runtime_error("No such type");
            }
        }
    }

    CompoundInstance(const std::string& type, std::istream& is)
        : type_(Resolver::resolveCompound(type)) {
        read(is);
    }

    CompoundInstance(const detail::TypeInstance& rhs)
        : CompoundInstance(dynamic_cast<const CompoundInstance<R>&>(rhs)) {}

    std::ostream& write(std::ostream& os) const {
        for (const auto& m : members_) {
            m.second->write(os);
        }
        return os;
    }

    std::istream& read(std::istream& is) {
        for (const auto& [name, member] : type_.members()) {
            if (R::isBasicType(member.type)) {
                members_[name] = std::make_unique<typename R::BasicType>(
                        R::resolveBasic(member.type)(is));
            } else if (R::isCompoundType(member.type)) {
                members_[name] = std::make_unique<CompoundInstance<R>>(
                        R::resolveCompound(member.type).template create<R>(is));
            } else {
                throw std::runtime_error("No such type");
            }
        }
        return is;
    }

    const detail::TypeInstance& operator()(const std::string& name) const {
        return *members_.at(name);
    }
};

template <typename R>
std::ostream& operator<<(std::ostream& os, const CompoundInstance<R>& x) {
    return x.write(os);
}

template <typename R>
std::istream& operator>>(std::istream& is, CompoundInstance<R>& x) {
    return x.read(is);
}

// If B is a Basic<R, U...>, then construct a type map mapping the given
// types to the U...
template <typename B>
inline TypeMap_t<B> makeTypeMap(std::list<std::string> types) {
    return detail::makeTypeMap<typename B::Resolver>(typename B::Types(), types);
}

// Example implementation of a type resolver. Uses static variables
// that must be explicitly specialized by the user. It is
// convenient to use the BasicWithDefaultResolver alias to save
// duplicating template parameters. For example,
//
//     using B = BasicWithDefaultResolver<int, double>;
//     using BR = B::Resolver;
//     template <>
//     const BR::BasicMapType BR::basicTypes = makeTypeMap<B>({"int", "double"});
//     template <>
//     BR::CompoundMapType BR::compoundTypes = {};
//
template <typename ... U>
class BasicResolver {
public:
    using BasicType = Basic<BasicResolver<U...>, U...>;
    using BasicMapType = TypeMap_t<BasicType>;
    using CompoundMapType = std::map<std::string, CompoundType>;
private:
    const static BasicMapType basicTypes;
    static CompoundMapType compoundTypes;
public:
    static auto resolveBasic(const std::string& s) {
        return basicTypes.at(s);
    }

    static void registerCompoundType(CompoundType type) {
        compoundTypes.emplace(type.name(), type);
    }

    static const CompoundType& resolveCompound(const std::string& s) {
        return compoundTypes.at(s);
    }

    static bool isBasicType(const std::string& s) {
        return std::end(basicTypes) != std::find_if(std::begin(basicTypes),
                std::end(basicTypes), [&s](const auto& p) { return p.first == s; });
    }

    static bool isCompoundType(const std::string& s) {
        return std::end(compoundTypes) != std::find_if(std::begin(compoundTypes),
                std::end(compoundTypes), [&s](const auto& p) { return p.first == s; });
    }
};

template <typename ... U>
using BasicWithDefaultResolver = Basic<BasicResolver<U...>, U...>;

} // namespace runtype

#endif // RUNTYPE_HPP
