
[![GCC & Clang Build Status](https://travis-ci.org/Dwarfobserver/soa_vector.svg?branch=master)](https://travis-ci.org/Dwarfobserver/soa_vector) [![MSVC Build Status](https://ci.appveyor.com/api/projects/status/github/Dwarfobserver/soa_vector?svg=true)](https://ci.appveyor.com/project/Dwarfobserver/soa_vector) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

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
SOA_DEFINE_TYPE(user::person, name, age);
// The line above is equivalent to :
// namespace soa {
//     template <> struct members<::user::person> {
//         vector_span<0, ::user::person, std::string> name;
//         vector_span<1, ::user::person, int>         age;
//     };
// }

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
    
    for (auto& name : persons.name)
        std::cout << "new person : " << name << '\n';
    
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

Project limitations :

 - The aggregate max size is limited (10 by default, it can be increased with more copy-pasta of the 'soa::detail::as_tuple' function).
 - It does not support aggregates with native arrays (eg. T[N], use std::array<T, N> instead).
 - It does not support aggregates with base classes (they are detected as aggregates but can't be destructured).
 - It does not support over-aligned types from the aggregates.

What's coming :

 - Exceptions garantees for the av::vector functions (in code and documented)
 - av::hybrid_vector<class T, size_t GroupSize, class Allocator> which holds little arrays of components to be more cache-friendly while itearting on sevveral components at the same time (the components layout can look like this :  xxxxyyyyzzzzxxxxyyyyzzzz..., for T = {x, y, z} and GroupSize = 4)
