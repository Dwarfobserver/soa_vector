
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../soa_vector.hpp"


// Get iterator on component I/N of values from std::vector<T>.
template <size_t I, size_t N, class T>
auto make_component_view(std::vector<T> const& vec) {
    struct iterator {
        T const* ptr;

        bool operator==(iterator const& it) const { return ptr == it.ptr; }
        bool operator!=(iterator const& it) const { return ptr != it.ptr; }

        iterator& operator++() { ++ptr; return *this; }

        auto const& operator*() const {
            auto const tuple = soa::detail::as_tuple<N>(*ptr);
            return std::get<I>(tuple);
        }
        auto const* operator->() const {
            auto const tuple = soa::detail::as_tuple<N>(*ptr);
            return &std::get<I>(tuple);
        }
    };
    struct range {
        iterator begin_;
        iterator end_;

        int      size()  const { return end_.ptr - begin_.ptr; }
        iterator begin() const { return begin_; }
        iterator end()   const { return end_; }
    };
    return range{ iterator{ vec.data() }, iterator{ vec.data() + vec.size() } };
}

// Compare size and values of a specific component from both vectors.
template <class Span, class Span2>
void check_vector_integrity(Span const& span, Span2 const& span_copy) {
    REQUIRE(span.size() == span_copy.size());
    auto it      = span.begin();
    auto it_copy = span_copy.begin();
    for (; it != span.end(); ++it, ++it_copy) {
        REQUIRE(*it == *it_copy);
    }
    REQUIRE(it_copy == span_copy.end());
}

// Compare size, capacity and values from both vectors.
template <class T, size_t...Is>
void check_vector_integrity(soa::vector<T> const& vec, std::vector<T> const& vec_copy, std::index_sequence<Is...>) {
    REQUIRE(vec.empty()    == vec_copy.empty());
    REQUIRE(vec.size()     == static_cast<int>(vec_copy.size()));
    REQUIRE(vec.capacity() == static_cast<int>(vec_copy.capacity()));
    (check_vector_integrity(
        vec.template get_span<Is>(),
        make_component_view<Is, sizeof...(Is)>(vec_copy)
    ), ...);
}

template <class T>
struct vector_interface {
    soa::vector<T>  v1;
    std::vector<T> v2;

    void check_integrity() const {
        constexpr auto size = soa::vector<T>::components_count;
        using seq = std::make_index_sequence<size>;
        check_vector_integrity(v1, v2, seq{});
    }
};

template <class T>
void test_vector(T const& value) {
    #define SOA_TEST(n, ...) n.v1.__VA_ARGS__; n.v2.__VA_ARGS__; n.check_integrity()
    #define SOA_CHECK(n) n.check_integrity()

    auto i1 = vector_interface<T>{};
    SOA_CHECK(i1);
    SOA_TEST(i1, reserve(4));
    SOA_TEST(i1, resize(1));
    SOA_TEST(i1, push_back(value));

    auto const j = i1;
    SOA_CHECK(j);

    auto i2 = std::move(i1);
    SOA_CHECK(i2);
    SOA_TEST(i2, emplace_back());
    SOA_TEST(i2, pop_back());
    SOA_TEST(i2, clear());

    #undef SOA_TEST
    #undef SOA_CHECK
}


namespace user {
    struct physics {
        float pos;
        float speed;
        float acc;
        int   id;
    };
}
SOA_DEFINE_TYPE(user::physics, pos, speed, acc, id);

struct person {
    std::string name;
    int age;
    bool likes_cpp;
};
SOA_DEFINE_TYPE(person, name, age, likes_cpp);

struct movable {
    std::unique_ptr<int> ptr;
};
SOA_DEFINE_TYPE(movable, ptr);

TEST_CASE("generic comparisons against std::vector") {
    test_vector(user::physics{ true, 2.0, 3.f, 42 });
    test_vector(person{ "Sid", 22, true });
}

TEST_CASE("move-only types") {
    auto v2 = soa::vector<movable>{};
    auto v1 = std::move(v2);
    v1.emplace_back(std::make_unique<int>());
    REQUIRE(v1.capacity() == 1);
    v1.emplace_back(std::make_unique<int>());
    REQUIRE(v1.capacity() > 1);
    v2 = std::move(v1);
    REQUIRE(v2.size() == 2);
}

TEST_CASE("conditions not covered previously") {
    auto v1 = soa::vector<person>{};
    auto v2 = v1;
    auto v3 = std::move(v1);
    v2 = std::move(v3);
    v3 = v2;
    REQUIRE(v1.empty());
    REQUIRE(v2.empty());
    REQUIRE(v3.empty());
    REQUIRE(v1.capacity() == 0);
    REQUIRE(v2.capacity() == 0);
    REQUIRE(v3.capacity() == 0);

    v1.emplace_back();
    REQUIRE(v1.size() == 1);
    v1 = std::move(v2);
    REQUIRE(v1.empty());

    v1.reserve(2);
    REQUIRE(v1.empty());
    REQUIRE(v1.capacity() == 2);

    v1.resize(3);
    REQUIRE(v1.size() == 3);
    REQUIRE(v1.capacity() == 3);

    v1.resize(2);
    REQUIRE(v1.size() == 2);
    REQUIRE(v1.capacity() == 3);
}
