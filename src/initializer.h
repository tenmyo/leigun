#pragma once

// ref: c - __attribute__((constructor)) equivalent in VC? - Stack Overflow
// http://stackoverflow.com/questions/1113409/attribute-constructor-equivalent-in-vc/2390626#2390626
// Rev.5

// Initializer/finalizer sample for MSVC and GCC/Clang.
// 2010-2016 Joe Lowe. Released into the public domain.
#ifdef __cplusplus
#define INITIALIZER(f)                                                         \
    static void f(void);                                                       \
    struct f##_t_ {                                                            \
        f##_t_(void) {                                                         \
            f();                                                               \
        }                                                                      \
    };                                                                         \
    static f##_t_ f##_;                                                        \
    static void f(void)
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define INITIALIZER2_(f, p)                                                    \
    static void f(void);                                                       \
    __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f;                   \
    __pragma(comment(linker, "/include:" p #f "_")) static void f(void)
#ifdef _WIN64
#define INITIALIZER(f) INITIALIZER2_(f, "")
#else
#define INITIALIZER(f) INITIALIZER2_(f, "_")
#endif
#else
#define INITIALIZER(f)                                                         \
    static void f(void) __attribute__((constructor));                          \
    static void f(void)
#endif
