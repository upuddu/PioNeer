#pragma once

// IWYU pragma: private

namespace fl {
    // ARM platforms (32-bit): use compiler builtins to match system stdint.h exactly.
    // This prevents typedef conflicts between FastLED's stdint.h and the system's.
    // See fl/stl/stdint.h for the full explanation of this pattern.
    typedef __INT16_TYPE__ i16;
    typedef __UINT16_TYPE__ u16;
    typedef __INT32_TYPE__ i32;
    typedef __UINT32_TYPE__ u32;
    typedef __INT64_TYPE__ i64;
    typedef __UINT64_TYPE__ u64;
    // ARM is 32-bit: pointers and size are 32-bit
    typedef __SIZE_TYPE__ size;        // size_t equivalent
    typedef __UINTPTR_TYPE__ uptr;     // uintptr_t equivalent
    typedef __INTPTR_TYPE__ iptr;      // intptr_t equivalent
    typedef __PTRDIFF_TYPE__ ptrdiff;  // ptrdiff_t equivalent
} 
