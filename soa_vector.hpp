
/*
    soa_vector.hpp
    MIT license (2018)
    Header repository : https://github.com/Dwarfobserver/soa_vector
    You can join me at sidney.congard@gmail.com
 */

#pragma once

#include <cstring>
#include <cstddef>
#include <utility>
#include <memory>
#include <tuple>
#include <string_view>
#include <typeinfo>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

namespace soa {

// Holds arrays for each T component in a single allocation.
// The allocator will be rebound to 'std::byte'.
template <class T, class Allocator = std::allocator<T>>
class vector;

// Iterable object accessed in soa::vector<Aggregate> through soa::member<Aggregate>.
template <size_t Pos, class Aggregate, class T>
class vector_span;

// Specialized for aggregates so soa::vector<T> can be istanciated.
// Specialization of non-template types can be done with the macro
// 'SOA_DEFINE_TYPE(type, members...);' in the global namespace.
template <class Aggregate>
struct members {};

// These proxy types are defined with the macro. They are created when iterating on a
// soa::vector and mimic the given aggregate members as references.
template <class Aggregate>
struct ref_proxy {};
template <class Aggregate>
struct cref_proxy {};

// A trait allows to check if the three class above have been defined for the given type.
template <class Aggregate>
constexpr bool is_defined_v =
    !std::is_empty_v<members   <Aggregate>> &&
    !std::is_empty_v<ref_proxy <Aggregate>> &&
    !std::is_empty_v<cref_proxy<Aggregate>>;

// Exemple of specialization (for std::pair) :
template <class T1, class T2>
struct members<std::pair<T1, T2>> {
    vector_span<0, std::pair<T1, T2>, T1> first; 
    vector_span<1, std::pair<T1, T2>, T2> second;
};
template <class T1, class T2>
struct ref_proxy<std::pair<T1, T2>> {
    T1 & first;
    T2 & second;
    
    // Use of template class to allow SFINAE.
    // This operator is optional.
    template <class T_ = T1, class = std::enable_if_t<
        std::is_copy_constructible_v<std::pair<T_, T2>>
    >>
    constexpr operator std::pair<T1, T2>() const {
        return { first, second };
    }
};
template <class T1, class T2>
struct cref_proxy<std::pair<T1, T2>> {
    T1 const& first;
    T2 const& second;
    
    template <class T_ = T1, class = std::enable_if_t<
        std::is_copy_constructible_v<std::pair<T_, T2>>
    >>
    constexpr operator std::pair<T1, T2>() const {
        return { first, second };
    }
};

namespace detail {

    namespace impl {
        template <class T, size_t I>
        using indexed_alias = T;

        template <class T, class Seq>
        struct repeat_tuple {};
        
        template <class T, size_t...Is>
        struct repeat_tuple<T, std::index_sequence<Is...>> {
            using type = std::tuple<indexed_alias<T, Is>...>;
        };
    }
    // Equivalent of std::tuple<T, T, T...N times>.
    template <class T, size_t N>
    using repeat_tuple_t = typename impl::repeat_tuple<T, std::make_index_sequence<N>>::type;

    namespace impl {
        template <size_t I, class...Ts>
        struct get {};
        template <size_t I, class T, class...Ts>
        struct get<I, T, Ts...> {
            using type = typename get<I - 1, Ts...>::type;
        };
        template <class T, class...Ts>
        struct get<0, T, Ts...> {
            using type = T;
        };
    }
    // An empty type used to pass types.
    template <class...Ts>
    struct type_tag {
        template <size_t I>
        using get = typename impl::get<I, Ts...>::type;
        using type = get<0>;
    };

    namespace impl {
        template <class Tuple>
        struct tuple_tag {};
        template <class...Ts>
        struct tuple_tag<std::tuple<Ts...>> {
            using type = type_tag<Ts...>;
        };
    }
    // std::tuple<Ts...> gives type_tag<Ts...>.
    template <class Tuple>
    using tuple_tag = typename impl::tuple_tag<Tuple>::type;

    // Base class of soa::vector<T>.
    // Used to retrieve the size by soa::vector_span<Offset, T, MemberT> from members<T>.
    template <class T>
    class members_with_size : public members<T> {
        template <size_t, class, class>
        friend class ::soa::vector_span;
    protected:
        int size_;
    };

    template <class T>
    auto type_name() {
        auto const name = typeid(T).name();
    #if __has_include(<cxxabi.h>)
        int errc;
        unsigned long size;
        auto const free_ptr = std::free;
        auto const demangled_name = std::unique_ptr<char, decltype(free_ptr)>{
            abi::__cxa_demangle(name, nullptr, &size, &errc),
            free_ptr
        };
        return errc
            ? std::string{ name }
            : std::string(demangled_name.get(), size);
    #else
        return std::string_view{ name };
    #endif
    }
    template <class...Strings>
    std::string concatene(Strings const&...str) {
        auto message = std::string{};
        message.reserve((0 + ... + str.size()));
        (message.append(str), ...);
        return message;
    }
    template <class T>
    [[noreturn]]
    void throw_out_of_range(int index, int size) {
        using namespace std::literals;
        
        throw std::out_of_range{detail::concatene(
            "Out of bounds access when calling "sv, detail::type_name<T>(), "::at("sv,
            std::to_string(index), ") while size = "sv, std::to_string(size)
        )};
    }

} // ::detail

template <size_t Pos, class Aggregate, class T>
class vector_span {
    template <class, class>
    friend class vector;
    template <class>
    friend struct members;
public:
    using value_type = T;

    // Informations
    T *      data()       noexcept { return ptr_; }
    T const* data() const noexcept { return ptr_; }
    int size() const noexcept;

    // Accessors
    T &      operator[](int i)       noexcept { return ptr_[i]; }
    T const& operator[](int i) const noexcept { return ptr_[i]; }
    T &      at(int i)       { check_at(i); return ptr_[i]; }
    T const& at(int i) const { check_at(i); return ptr_[i]; }

    T &      front()       noexcept { return ptr_[0]; }
    T const& front() const noexcept { return ptr_[0]; }
    T &      back()       noexcept { return ptr_[size() - 1]; }
    T const& back() const noexcept { return ptr_[size() - 1]; }

    // Iterators
    T *      begin()       noexcept { return ptr_; }
    T const* begin() const noexcept { return ptr_; }
    T *      end()       noexcept { return ptr_ + size(); }
    T const* end() const noexcept { return ptr_ + size(); }
private:
    void check_at(int i) const {
        if (i >= size()) detail::throw_out_of_range<vector_span<Pos, Aggregate, T>>(i, size());
    }

    vector_span() = default;
    vector_span(vector_span const&) = default;
    vector_span& operator=(vector_span const&) = default;
    
    vector_span(std::byte * ptr) noexcept :
        ptr_{ reinterpret_cast<T *>(ptr) }
    {}

    T * ptr_;
};

// Template arguments are used to retrieve the size from detail::members_with_size<Aggregate>.
template <size_t Pos, class Aggregate, class T>
int vector_span<Pos, Aggregate, T>::size() const noexcept {
    auto const mem_ptr = reinterpret_cast<members<Aggregate> const*>(this - Pos);
    auto const mws_ptr = static_cast<detail::members_with_size<Aggregate> const*>(mem_ptr);
    return mws_ptr->size_;
}

namespace detail {
    // Aggregate to tuple implementation, only for soa::member<T>.

    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 1>) {
        auto & [v1] = agg;
        return std::forward_as_tuple(v1);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 2>) {
        auto & [v1, v2] = agg;
        return std::forward_as_tuple(v1, v2);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 3>) {
        auto & [v1, v2, v3] = agg;
        return std::forward_as_tuple(v1, v2, v3);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 4>) {
        auto & [v1, v2, v3, v4] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 5>) {
        auto & [v1, v2, v3, v4, v5] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4, v5);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 6>) {
        auto & [v1, v2, v3, v4, v5, v6] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4, v5, v6);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 7>) {
        auto & [v1, v2, v3, v4, v5, v6, v7] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4, v5, v6, v7);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 8>) {
        auto & [v1, v2, v3, v4, v5, v6, v7, v8] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4, v5, v6, v7, v8);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 9>) {
        auto & [v1, v2, v3, v4, v5, v6, v7, v8, v9] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4, v5, v6, v7, v8, v9);
    }
    template <class T>
    auto as_tuple(T & agg, std::integral_constant<int, 10>) {
        auto & [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10] = agg;
        return std::forward_as_tuple(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10);
    }

    // The arity is the number of members of a well-formed soa::member<T>.
    template <class Members>
    constexpr int arity_v = sizeof(Members) / sizeof(vector_span<0, vector<char>, char>);

    // Continue the overloads above to increase the max_arity.
    constexpr int max_arity = 10;

    // Converts a well-formed soa::member<T> to a tuple with references on each member of the class.
    template <class T>
    auto as_tuple(members<T> const& agg) {
        return as_tuple(agg, std::integral_constant<int, arity_v<members<T>>>{});
    }
    template <class T>
    auto as_tuple(members<T> & agg) {
        return as_tuple(agg, std::integral_constant<int, arity_v<members<T>>>{});
    }

    // Allows to converts any aggregate to a tuple given it's arity.
    template <size_t Arity, class T>
    auto as_tuple(T && agg) {
        return as_tuple(agg, std::integral_constant<int, Arity>{});
    }

    // for_each loops takes a function object to operate on one or two tuples of references.
    // Note : C++20 template lambdas would be cleaner to retrieve the type.

    template <class F, size_t...Is, class...Ts>
    constexpr void for_each(std::tuple<Ts &...> const& tuple, F && f, std::index_sequence<Is...>) {
        (f(std::get<Is>(tuple), type_tag<typename Ts::value_type>{}), ...);
    }
    template <class F, class...Ts>
    constexpr void for_each(std::tuple<Ts &...> const& tuple, F && f) {
        using seq = std::make_index_sequence<sizeof...(Ts)>;
        detail::for_each(tuple, f, seq{});
    }

    template <class F, size_t...Is, class...Ts1, class...Ts2>
    constexpr void for_each(std::tuple<Ts1 &...> const& t1, std::tuple<Ts2 &...> const& t2, F && f, std::index_sequence<Is...>) {
        (f(std::get<Is>(t1), std::get<Is>(t2), type_tag<typename Ts1::value_type>{}), ...);
    }
    template <class F, class...Ts1, class...Ts2>
    constexpr void for_each(std::tuple<Ts1 &...> const& t1, std::tuple<Ts2 &...> const& t2, F && f) {
        static_assert(sizeof...(Ts1) == sizeof...(Ts2));
        using seq = std::make_index_sequence<sizeof...(Ts1)>;
        detail::for_each(t1, t2, f, seq{});
    }

    // Apply a function for every object in two ranges.
    // Used for soa::vector copy/move assignments and constructors.
    template <class T, class SizeT, class F>
    constexpr void apply_two_arrays(T const* __restrict src, T * dst, SizeT size, F&& f) {
        for (SizeT i = 0; i < size; ++i) {
            f(src[i], dst[i]);
        }
    }
    template <class T, class SizeT, class F>
    constexpr void apply_two_arrays(T * __restrict src, T * dst, SizeT size, F&& f) {
        for (SizeT i = 0; i < size; ++i) {
            f(src[i], dst[i]);
        }
    }

    // Iterator used by soa::vector to return new proxies with references to the elements.
    template <class Vector, bool IsConst>
    class proxy_iterator {
        friend Vector;

        using vector_pointer_type = std::conditional_t<IsConst,
            Vector const*,
            Vector *>;

        vector_pointer_type vec_;
        int index_;

        proxy_iterator(vector_pointer_type vec, int index) noexcept :
            vec_{vec}, index_{index} {}
    public:
        // Note : replace with std::continuous_iteartor_rag when it is available.
        using iterator_category = std::random_access_iterator_tag;

        using value_type = std::conditional_t<IsConst,
            typename Vector::const_reference_type,
            typename Vector::reference_type>;
    private:
        template <size_t...Is>
        value_type make_proxy(std::index_sequence<Is...>) const noexcept {
            return { vec_->template get_span<Is>()[index_] ... };
        }
    public:
        value_type operator*() const noexcept { return make_proxy(typename Vector::sequence_type{}); }

        bool operator==(proxy_iterator const& rhs) const noexcept { return index_ == rhs.index_; }
        bool operator!=(proxy_iterator const& rhs) const noexcept { return !(*this == rhs); }

        bool operator<(proxy_iterator const& rhs) const noexcept { return index_ < rhs.index_; }
        bool operator>(proxy_iterator const& rhs) const noexcept { return rhs < *this; }
        bool operator<=(proxy_iterator const& rhs) const noexcept { return !(rhs < *this); }
        bool operator>=(proxy_iterator const& rhs) const noexcept { return !(*this < rhs); }

        proxy_iterator & operator++() noexcept { return ++index_, *this; }
        proxy_iterator & operator--() noexcept { return --index_, *this; }
        proxy_iterator & operator++(int) noexcept { const auto old = *this; return ++index_, old; }
        proxy_iterator & operator--(int) noexcept { const auto old = *this; return --index_, old; }

        proxy_iterator & operator+=(int shift) noexcept { return index_ += shift, *this; }
        proxy_iterator & operator-=(int shift) noexcept { return index_ -= shift, *this; }

        proxy_iterator operator+(int shift) const noexcept { return { vec_, index_ + shift }; }
        proxy_iterator operator-(int shift) const noexcept { return { vec_, index_ - shift }; }
        
        int operator-(proxy_iterator const& rhs) const noexcept { return index_ - rhs.index_; }
    };

} // ::detail

// Stores components of the aggregate T (given by the specialization soa::member<T>)
// in successives arrays from an unique continuous allocation.
// It increases the performance when the access patterns are differents for the
// aggregate's members.
// Over-aligned types are not supported.
template <class T, class Allocator>
class vector : public detail::members_with_size<T> {
public:
    static_assert(is_defined_v<T>,
        "soa::vector<T> can't be instancied because the required types 'soa::members<T>', "
        "'soa::ref_proxy<T>' or 'soa::cref_proxy<T>' haven't been defined. "
        "Did you forget to call the macro SOA_DEFINE_TYPE(T, members...) ?");

    // The given allocator is reboud to std::byte to store the different member types.
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;

    using value_type           = T;
    using reference_type       = ref_proxy<T>;
    using const_reference_type = cref_proxy<T>;

    using iterator       = detail::proxy_iterator<vector, false>;
    using const_iterator = detail::proxy_iterator<vector, true>;

    // The number of T members.
    static constexpr int components_count = detail::arity_v<members<T>>;

    // Constructors.
    vector(Allocator allocator = Allocator{}) noexcept;
    vector(vector && rhs) noexcept;
    vector(vector const& rhs);

    // Assignments.
    vector& operator=(vector && rhs) noexcept;
    vector& operator=(vector const& rhs);

    // Destructor.
    ~vector();

    // Size or capacity modifiers.
    void clear() noexcept;
    void reserve(int capacity);
    void resize(int size);
    void resize(int size, T const& value);
    void shrink_to_fit();

    // Add and remove an element.
    template <class...Ts>
    void emplace_back(Ts &&...components);
    void push_back(T const& value);
    void push_back(T && value);
    void pop_back() noexcept;

    // Informations.
    int  size()     const noexcept { return this->size_; }
    int  capacity() const noexcept { return capacity_; }
    bool empty()    const noexcept { return size() == 0; }

    // Accessors.
    reference_type       operator[](int i)       noexcept { return *(begin() + i); } 
    const_reference_type operator[](int i) const noexcept { return *(begin() + i); }
    reference_type       at(int i)       { check_at(i); return *(begin() + i); }
    const_reference_type at(int i) const { check_at(i); return *(begin() + i); }

    reference_type       front()       noexcept { return *begin(); }
    const_reference_type front() const noexcept { return *begin(); }
    reference_type       back()       noexcept { return *(end() - 1); }
    const_reference_type back() const noexcept { return *(end() - 1); }

    // Iterators.
    iterator       begin()        noexcept { return { this, 0 }; }
    const_iterator begin()  const noexcept { return { this, 0 }; }
    const_iterator cbegin() const noexcept { return begin(); }
    iterator       end()        noexcept { return { this, size() }; }
    const_iterator end()  const noexcept { return { this, size() }; }
    const_iterator cend() const noexcept { return end(); }

    // Components accessors.
    template <size_t I>
    auto & get_span() noexcept;
    template <size_t I>
    auto const & get_span() const noexcept;
private:
    friend iterator;
    friend const_iterator;

    // Some static asserts on the soa::member<T> type.
    // Workaround MSVC : must returns a value to be constexpr.
    static constexpr int check_members();
    static constexpr int check_members_trigger = check_members();

    // Explicit cast to base class.
    members<T>&      base()       noexcept { return *this; }
    members<T>const& base() const noexcept { return *this; }

    detail::members_with_size<T>&       base_with_size()       noexcept { return *this; }
    detail::members_with_size<T> const& base_with_size() const noexcept { return *this; }

    using sequence_type = std::make_index_sequence<components_count>;

    // components_tag = detail::type_tag<Ts...>.
    template <class Tuple>
    struct components_tag_impl;
    template <class...Members>
    struct components_tag_impl<std::tuple<Members&...>> {
        using type = detail::type_tag<typename Members::value_type...>;
    };
    using components_tag = typename components_tag_impl<decltype(
        detail::as_tuple(std::declval<members<T>>())
    )>::type;

    using allocator_traits = std::allocator_traits<allocator_type>;

    // Functions implementations.

    void check_at(int index) const;

    template <class Tuple, size_t...Is>
    void push_back_copy(Tuple const& tuple, std::index_sequence<Is...>);
    template <class Tuple, size_t...Is>
    void push_back_move(Tuple& tuple, std::index_sequence<Is...>);

    template <size_t I, class...Members, class T1, class...Ts>
    void emplace_back_impl(std::tuple<Members&...> const& members, T1&& component, Ts&&...nexts);
    template <size_t I, class...Members>
    void emplace_back_impl(std::tuple<Members&...> const& members);

    // Computes the bytes padding for each component,
    // assuming we start with a 8-bytes aligned address.
    template <size_t I, class...Ints>
    static void update_shift(std::tuple<Ints...>& shifts, int nb, int acc);

    // Creates vector_spans based on the data allocated
    // and the computed shift for each component.
    template <class Tuple, size_t...Is>
    static members<T> create_members(std::byte* ptr, Tuple const& shift, std::index_sequence<Is...>);

    struct alloc_result {
        members<T> new_members;
        int nb_bytes;
    };
    // Allocates unitialized array of 'nb' elements.
    alloc_result allocate(int nb);

    static void construct_copy_array(members<T> const& src, members<T>& dst, int nb);
    static void construct_move_array(members<T> &      src, members<T>& dst, int nb);

    template <class F>
    static void apply_on_arrays(members<T> const& mem_src, members<T> & mem_dst, int nb, F && f);
    template <class F>
    static void apply_on_arrays(members<T> & mem_src, members<T> & mem_dst, int nb, F && f);

    void destroy() noexcept;
    void destroy(int begin, int end) noexcept;
    void deallocate() noexcept;

    // Sets the vector fields (size, capacity, ...) according to an empty vector.
    void to_zero() noexcept;

    int capacity_;
    allocator_type allocator_;
    int nb_bytes_;
};

// soa::vector implementation.

// The check function returns an arbitrary value to be executed at compile-time :
// The msvc version used don't support constexpr void functions.
template <class T, class Allocator>
constexpr int vector<T, Allocator>::check_members() {

    static_assert(!std::is_empty_v<members<T>>,
        "soa::members<T> must be specialized to hold "
        "an soa::vector_span for each member of T");
    
    static_assert(detail::arity_v<members<T>> <= detail::max_arity,
        "soa::members<T> must have less than 'max_arity' members. "
        "This limit can be increased by writing more overloads of 'as_tuple'.");
    
    return 0;
}

// Constructors.

template <class T, class Allocator>
vector<T, Allocator>::vector(Allocator allocator) noexcept :
    detail::members_with_size<T>{},
    capacity_ { 0 },
    allocator_{ allocator },
    nb_bytes_ { 0 }
{}

template <class T, class Allocator>
vector<T, Allocator>::vector(vector&& rhs) noexcept :
    detail::members_with_size<T>{ rhs.base_with_size() },
    capacity_ { rhs.capacity() },
    allocator_{ rhs.allocator_ },
    nb_bytes_ { rhs.nb_bytes_ }
{
    rhs.to_zero();
}

template <class T, class Allocator>
vector<T, Allocator>::vector(vector const& rhs) :
    detail::members_with_size<T>{ rhs.base_with_size() },
    capacity_ { rhs.size() },
    allocator_{ rhs.allocator_ },
    nb_bytes_ { rhs.nb_bytes_ }
{
    if (rhs.empty()) return;

    auto [new_members, nb_bytes] = allocate(size());
    construct_copy_array(rhs.base(), new_members, size());
    base()    = new_members;
    nb_bytes_ = nb_bytes;
}

// Assignments.

template <class T, class Allocator>
vector<T, Allocator>& vector<T, Allocator>::operator=(vector&& rhs) noexcept {
    destroy();
    deallocate();
    base_with_size() = rhs.base_with_size();
    capacity_  = rhs.capacity();
    allocator_ = rhs.allocator_;
    nb_bytes_  = rhs.nb_bytes_;
    rhs.to_zero();
    return *this;
}
    
template <class T, class Allocator>
vector<T, Allocator>& vector<T, Allocator>::operator=(vector const& rhs) {
    destroy();
    this->size_ = rhs.size();
    if (capacity() < size()) {
        deallocate();
        auto [new_members, nb_bytes] = allocate(size());
        base()    = new_members;
        nb_bytes_ = nb_bytes;
        capacity_ = size();
    }
    construct_copy_array(rhs.base(), base(), size());
    return *this;
}

// Destructor.
template <class T, class Allocator>
vector<T, Allocator>::~vector() {
    destroy();
    deallocate();
}

// Size & capacity modifiers.

template <class T, class Allocator>
void vector<T, Allocator>::clear() noexcept {
    destroy();
    this->size_ = 0;
}

template <class T, class Allocator>
void vector<T, Allocator>::reserve(int capacity) {
    if (capacity <= this->capacity()) return;
    
    auto [new_members, nb_bytes] = allocate(capacity);
    construct_move_array(base(), new_members, size());
    base()    = new_members;
    nb_bytes_ = nb_bytes;
    capacity_ = capacity;
}

template <class T, class Allocator>
void vector<T, Allocator>::resize(int size) {
    if (size <= this->size()) {
        destroy(size, this->size());
        this->size_ = size;
        return;
    }
    reserve(size);
    detail::for_each(detail::as_tuple(base()), [this, size] (auto& span, auto tag) {
        using type = typename decltype(tag)::type;
        auto it = span.begin() + this->size();
        auto const end = span.begin() + size;
        for (; it < end; ++it) {
            new (it) type();
        }
    });
    this->size_ = size;
}

template <class T, class Allocator>
void vector<T, Allocator>::resize(int size, T const& value) {
    if (size <= this->size()) {
        destroy(size, this->size());
        this->size_ = size;
        return;
    }
    reserve(size);
    auto const tuple = detail::as_tuple<components_count>(value);
    detail::for_each(detail::as_tuple(base()), tuple, [this, size] (auto& span, auto& val, auto tag) {
        using type = typename decltype(tag)::type;
        auto it = span.begin() + this->size();
        auto const end = span.begin() + size;
        while (it < end) {
            new (it) type(val); ++it;
        }
    });
    this->size_ = size;
}

template <class T, class Allocator>
void vector<T, Allocator>::shrink_to_fit() {
    if (size() == capacity()) return;
    reallocate(size());
}

// Add and remove an element.

template <class T, class Allocator>
void vector<T, Allocator>::push_back(T const& value) {
    auto const tuple = detail::as_tuple<components_count>(value);
    push_back_copy(tuple, sequence_type{});
}

template <class T, class Allocator>
void vector<T, Allocator>::push_back(T&& value) {
    auto tuple = detail::as_tuple<components_count>(value);
    push_back_move(tuple, sequence_type{});
}

template <class T, class Allocator>
template <class...Ts>
void vector<T, Allocator>::emplace_back(Ts&&...components) {
    if (size() == capacity()) {
        auto const new_capacity = size() == 0 ? 1 : capacity() * 2;
        reserve(new_capacity);
    }
    emplace_back_impl<0>(detail::as_tuple(base()), std::forward<Ts>(components)...);
    ++this->size_;
}

template <class T, class Allocator>
void vector<T, Allocator>::pop_back() noexcept {
    --this->size_;
    detail::for_each(detail::as_tuple(base()), [this] (auto& span, auto tag) {
        using type = typename decltype(tag)::type;
        span[size()].~type();
    });
}

// Components accessors.

template <class T, class Allocator>
template <size_t I>
auto& vector<T, Allocator>::get_span() noexcept {
    static_assert(I < components_count);
    return std::get<I>(detail::as_tuple(base()));
}

template <class T, class Allocator>
template <size_t I>
auto const& vector<T, Allocator>::get_span() const noexcept {
    static_assert(I < components_count);
    return std::get<I>(detail::as_tuple(base()));
}

// Private functions.

template <class T, class Allocator>
void vector<T, Allocator>::check_at(int i) const {
    if (i >= size()) detail::throw_out_of_range<vector<T, Allocator>>(i, size());
}

template <class T, class Allocator>
void vector<T, Allocator>::construct_copy_array(members<T> const& mem_src, members<T>& mem_dst, int nb) {
    apply_on_arrays(mem_src, mem_dst, nb, [] (auto src, auto & dst) {
        using type = decltype(src);
        new (&dst) type(src);
    });
}
template <class T, class Allocator>
void vector<T, Allocator>::construct_move_array(members<T> & mem_src, members<T> & mem_dst, int nb) {
    apply_on_arrays(mem_src, mem_dst, nb, [] (auto & src, auto & dst) {
        using type = std::remove_reference_t<decltype(src)>;
        new (&dst) type(std::move(src));
    });
}

template <class T, class Allocator>
template <class F>
void vector<T, Allocator>::apply_on_arrays(members<T> const& mem_src, members<T> & mem_dst, int nb, F && f) {
    auto const t1 = detail::as_tuple(mem_src);
    auto const t2 = detail::as_tuple(mem_dst);
    detail::for_each(t1, t2, [f, nb] (auto const& span_src, auto & span_dst, auto) {
        detail::apply_two_arrays(span_src.data(), span_dst.data(), nb, f);
    });
}
template <class T, class Allocator>
template <class F>
void vector<T, Allocator>::apply_on_arrays(members<T> & mem_src, members<T> & mem_dst, int nb, F && f) {
    auto const t1 = detail::as_tuple(mem_src);
    auto const t2 = detail::as_tuple(mem_dst);
    detail::for_each(t1, t2, [f, nb] (auto & span_src, auto & span_dst, auto) {
        detail::apply_two_arrays(span_src.data(), span_dst.data(), nb, f);
    });
}

template <class T, class Allocator>
template <class Tuple, size_t...Is>
void vector<T, Allocator>::push_back_copy(Tuple const& tuple, std::index_sequence<Is...>) {
    emplace_back(std::get<Is>(tuple)...);
}
template <class T, class Allocator>
template <class Tuple, size_t...Is>
void vector<T, Allocator>::push_back_move(Tuple& tuple, std::index_sequence<Is...>) {
    emplace_back(std::move(std::get<Is>(tuple))...);
}

template <class T, class Allocator>
template <size_t I, class...Members, class T1, class...Ts>
void vector<T, Allocator>::emplace_back_impl(std::tuple<Members&...> const& tuple, T1&& component, Ts&&...nexts) {
    if constexpr (I < sizeof...(Members)) {
        using type = typename components_tag::template get<I>;
        auto const it = std::get<I>(tuple).ptr_ + size();
        new (it) type(std::forward<T1>(component));
        emplace_back_impl<I + 1>(tuple, std::forward<Ts>(nexts)...);
    }
}
template <class T, class Allocator>
template <size_t I, class...Members>
void vector<T, Allocator>::emplace_back_impl(std::tuple<Members&...> const& tuple) {
    if constexpr (I < sizeof...(Members)) {
        using type = typename components_tag::template get<I>;
        auto const it = std::get<I>(tuple).ptr_ + size();
        new (it) type();
        emplace_back_impl<I + 1>(tuple);
    }
}

template <class T, class Allocator>
template <size_t I, class...Ints>
void vector<T, Allocator>::update_shift(std::tuple<Ints...>& tuple, int nb, int shift) {
    using prev = typename components_tag::template get<I - 1>;
    if constexpr (I == sizeof...(Ints) - 1) {
        std::get<I>(tuple) = shift + nb * sizeof(prev);
    }
    else {
        using type = typename components_tag::template get<I>;
        constexpr auto align = alignof(type) - 1;
        shift += (nb * sizeof(prev) + align) & ~align;
        std::get<I>(tuple) = shift;
        update_shift<I + 1>(tuple, nb, shift);
    }
}

template <class T, class Allocator>
template <class Tuple, size_t...Is>
members<T> vector<T, Allocator>::create_members(std::byte* ptr, Tuple const& shift, std::index_sequence<Is...>) {
    return { (ptr + std::get<Is>(shift))... };
}

template <class T, class Allocator>
typename vector<T, Allocator>::alloc_result
vector<T, Allocator>::allocate(int nb) {
    constexpr int arity = detail::arity_v<members<T>>;
    auto shift = detail::repeat_tuple_t<int, arity + 1>{};
    update_shift<1>(shift, nb, 0);

    auto const nb_bytes = std::get<arity>(shift);
    auto const ptr = allocator_traits::allocate(allocator_, nb_bytes);
    return { create_members(ptr, shift, sequence_type{}), nb_bytes };
}

template <class T, class Allocator>
void vector<T, Allocator>::destroy() noexcept {
    detail::for_each(detail::as_tuple(base()), [] (auto& span, auto tag) {
        using type = typename decltype(tag)::type;
        for (auto& val : span) val.~type();
    });
}

template <class T, class Allocator>
void vector<T, Allocator>::destroy(int begin, int end) noexcept {
    detail::for_each(detail::as_tuple(base()), [min = begin, max = end] (auto& span, auto tag) {
        using type = typename decltype(tag)::type;
        auto it = span.begin() + min;
        auto const end = span.begin() + max;
        for (; it < end; ++it) it->~type();
    });
}

template <class T, class Allocator>
void vector<T, Allocator>::deallocate() noexcept {
    if (capacity() == 0) return;
    auto const data = reinterpret_cast<std::byte*>(get_span<0>().ptr_);
    allocator_traits::deallocate(allocator_, data, nb_bytes_);
}

template <class T, class Allocator>
void vector<T, Allocator>::to_zero() noexcept {
    base_with_size() = {};
    capacity_ = 0;
    nb_bytes_ = 0;
}

} // namespace soa

// Private macros.

#define SOA_PP_EMPTY
#define SOA_PP_EMPTY_ARGS(...)

#define SOA_PP_EVAL0(...) __VA_ARGS__
#define SOA_PP_EVAL1(...) SOA_PP_EVAL0 (SOA_PP_EVAL0 (SOA_PP_EVAL0 (__VA_ARGS__)))
#define SOA_PP_EVAL2(...) SOA_PP_EVAL1 (SOA_PP_EVAL1 (SOA_PP_EVAL1 (__VA_ARGS__)))
#define SOA_PP_EVAL3(...) SOA_PP_EVAL2 (SOA_PP_EVAL2 (SOA_PP_EVAL2 (__VA_ARGS__)))
#define SOA_PP_EVAL4(...) SOA_PP_EVAL3 (SOA_PP_EVAL3 (SOA_PP_EVAL3 (__VA_ARGS__)))
#define SOA_PP_EVAL(...)  SOA_PP_EVAL4 (SOA_PP_EVAL4 (SOA_PP_EVAL4 (__VA_ARGS__)))

#define SOA_PP_MAP_GET_END() 0, SOA_PP_EMPTY_ARGS

#define SOA_PP_MAP_NEXT0(item, next, ...) next SOA_PP_EMPTY
#if defined(_MSC_VER)
#define SOA_PP_MAP_NEXT1(item, next) SOA_PP_EVAL0(SOA_PP_MAP_NEXT0 (item, next, 0))
#else
#define SOA_PP_MAP_NEXT1(item, next) SOA_PP_MAP_NEXT0 (item, next, 0)
#endif
#define SOA_PP_MAP_NEXT(item, next)  SOA_PP_MAP_NEXT1 (SOA_PP_MAP_GET_END item, next)

#define SOA_PP_MAP0(f, n, t, x, peek, ...) f(n, t, x) SOA_PP_MAP_NEXT (peek, SOA_PP_MAP1) (f, n+1, t, peek, __VA_ARGS__)
#define SOA_PP_MAP1(f, n, t, x, peek, ...) f(n, t, x) SOA_PP_MAP_NEXT (peek, SOA_PP_MAP0) (f, n+1, t, peek, __VA_ARGS__)
#define SOA_PP_MAP(f, t, ...) SOA_PP_EVAL (SOA_PP_MAP1 (f, 0, t, __VA_ARGS__, (), 0))

#define SOA_PP_MEMBER(nb, type, name) \
    vector_span<nb, type, decltype(std::declval<type>().name)> name;
    
#define SOA_PP_REF(nb, type, name) \
    decltype(std::declval<type>().name) & name;

#define SOA_PP_CREF(nb, type, name) \
    decltype(std::declval<type>().name) const& name;

#define SOA_PP_COPY(nb, type, name) \
    name = rhs.name;

#define SOA_PP_MOVE(nb, type, name) \
    name = std::move(rhs.name);

#define SOA_PP_ENABLE_FOR_COPYABLE(type, alias) \
    template <class alias, class = std::enable_if_t< \
        std::is_same_v<alias, type> && \
        std::is_copy_constructible_v<type> \
    >>

// Shortcut to specialize soa::member<my_type>, by listing all the members
// in their declaration order. It must be used in the global namespace.
// Usage exemple :
// 
// namespace user {
//     struct person {
//         std::string name;
//         int age;
//     };
// } 
//
// SOA_DEFINE_TYPE(user::person, name, age);
//
// This is equivalent to typing :
//
// namespace soa {
//     template <>
//     struct members<user::person> {
//         vector_span<0, user::person, std::string> name;
//         vector_span<1, user::person, int> age;
//     };
//     template <>
//     struct ref_proxy<user::person> {
//         std::string & name;
//         int & age;
//          
//         operator user::person() const {
//             return { name, age };
//         }
//
//         ref_proxy& operator=(user::person const& rhs) {
//             name = rhs.name;
//             age = rhs.age;
//             return *this;
//         }
//         ref_proxy& operator=(user::person && rhs) noexcept {
//             name = rhs.name;
//             age = rhs.age;
//             return *this;
//         }
//     };
//     template <>
//     struct cref_proxy<user::person> {
//         std::string const& name;
//         int const& age;
//          
//         operator user::person() const {
//             return { name, age };
//         }
//     };
// }
#define SOA_DEFINE_TYPE(type, ...) \
namespace soa { \
    template <> \
    struct members<::type> { \
        SOA_PP_MAP(SOA_PP_MEMBER, ::type, __VA_ARGS__) \
    }; \
    template <> \
    struct ref_proxy<::type> { \
        SOA_PP_MAP(SOA_PP_REF, ::type, __VA_ARGS__) \
        \
        ref_proxy& operator=(::type && rhs) noexcept { \
            SOA_PP_MAP(SOA_PP_MOVE, ::type, __VA_ARGS__)  \
            return *this; \
        } \
        SOA_PP_ENABLE_FOR_COPYABLE(::type, _type) \
        ref_proxy& operator=(_type const& rhs) { \
            SOA_PP_MAP(SOA_PP_COPY, ::type, __VA_ARGS__)  \
            return *this; \
        } \
        SOA_PP_ENABLE_FOR_COPYABLE(::type, _type) \
        operator _type() const { \
            return { __VA_ARGS__ }; \
        } \
        \
    }; \
    template <> \
    struct cref_proxy<::type> { \
        SOA_PP_MAP(SOA_PP_CREF, ::type, __VA_ARGS__) \
        \
        SOA_PP_ENABLE_FOR_COPYABLE(::type, _type) \
        operator _type() const { \
            return { __VA_ARGS__ }; \
        } \
    }; \
} \
struct _soa__force_semicolon_
