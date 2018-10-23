
[![GCC & Clang Build Status](https://travis-ci.org/Dwarfobserver/soa_vector.svg?branch=master)](https://travis-ci.org/Dwarfobserver/soa_vector) [![MSVC Build Status](https://ci.appveyor.com/api/projects/status/4p7srw0qe4bmshe8/branch/master?svg=true)](https://ci.appveyor.com/project/Dwarfobserver/soa-vector) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# SoA vector

This project is an attempt to resolve the usability issues coming when working with structure of arrays rather than array of structures :

This single-header library in C++17 implements a std::vector-like data structure which separates it's aggregate components into different arrays. It improves performance when there is different access patterns for the aggregate components, or when we want to perform vectorized operations on it's components.

It works on MSVC-19.14, Clang-5.0 and GCC-7.2.

This project is in early development and prone to change. I'd be happy to receive any feedback on it's design or performance !

Simple exemple usage :

```cpp

#include <soa_vector.hpp>
#include <iostream>

// We define an aggregate type : a type with only public members, no virtual functions and no constructor.
namespace user {
    struct person {
        std::string name;
        int age;
    };
}

// We expose the aggregate so it can be used by soa::vector.
// It defines spans contained in soa::vector and proxy types.
SOA_DEFINE_TYPE(user::person, name, age);

// We can now manipulate our soa::vector.
soa::vector<user::person> make_persons() {

    // The semantics are similar to std::vector.
    // Only one allocation is performed, so the soa::vector content looks like this in memory :
    // [name1, name2, ..., age1, age2, ...]
    auto persons = soa::vector<user::person>{};
    persons.reserve(2);
    persons.push_back({ "Jack", 35 });
    // emplace_back takes components as arguments, or default-construct them.
    persons.emplace_back("New Born");

    // Components are accessed with their name, through a range structure :
    // soa::vector stores internally a pointer for each component.
    for (auto& age : persons.age)
        age += 1;
    
    // You can also access the components like a classic vector through a proxy.
    // Be careful to not use the proxy with a dangling reference.
    for (auto p : persons)
        std::cout << "new person : " << p.name << '\n';
    
    return persons;
}

```

Tests can be built and launched with CMake.

```bash

mkdir build
cd build
cmake ..
cmake --build .
ctest -V

```

Accessing components through the proxy (with vector iterators and accessors) instead of using the vector ranges (vector.xxx iterators and accessors) can be more restrictive in generic code due to the proxy, and lead to performance degradation for some compilers or use cases :

```cpp

// With -O3, GCC and Clang compiles this function with memcpy, but not MSVC with /Ox.
void copy_ages_with_proxy(soa::vector<user::person> const& persons, int* __restrict dst) {
    for (int i = 0; i < persons.size(); ++i) {
        dst[i] = persons[i].age;
    }
}
// The three compilers use memcpy.
void copy_ages_with_span(soa::vector<user::person> const& persons, int* __restrict dst) {
    for (int i = 0; i < persons.size(); ++i) {
        dst[i] = persons.age[i];
    }
}

```

Project limitations :

 - The aggregate max size is limited (20 by default, it can be increased with more copy-pasta of the 'soa::detail::as_tuple' function).
 - It does not support aggregates with native arrays (eg. T[N], use std::array<T, N> instead).
 - It does not support aggregates with base classes (they are detected as aggregates but can't be destructured).
 - It does not support over-aligned types from the aggregates.
