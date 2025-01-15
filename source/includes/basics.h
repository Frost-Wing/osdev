/**
 * @file basics.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief This is a basic header files with FrostWing specific short forms and basically a good for life header
 * @version 0.1
 * @date 2023-12-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>

typedef uintptr_t int_pointer;
typedef uint64_t int64;
typedef uint32_t int32;
typedef uint16_t int16;
typedef uint8_t int8;

#define null NULL

typedef const char* cstring;
typedef char* string;

#define MiB *1024*1024
#define KiB *1024

#define yes true
#define no false

#define attribute __attribute__

#define Ghz *1000000000ULL
#define Mhz *1000000ULL

#define OS_NAME "FrostWing OS"

#define EOF (-1)

#ifdef __GNUC__
#define deprecated_message(msg) attribute((deprecated(msg)))
#elif defined(_MSC_VER)
#define deprecated_message(msg) __declspec(deprecated(msg))
#else
#define deprecated_message(msg)
#endif

/**
 * @brief Assert Definition
 * @authors GAMINGNOOB (Coded Original) & Pradosh (Modified it)
 */
#define assert(expression, file, line) if(!(expression)){printf("\x1b[31mAssert Failed! at \x1b[36m%s:%d\x1b[0m => \x1b[32m%s\x1b[0m", file, line, #expression);}