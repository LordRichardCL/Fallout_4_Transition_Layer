#pragma once

// ============================================================
// Basic F4SE integer typedefs (unsigned)
// ============================================================
typedef unsigned char      UInt8;
typedef unsigned short     UInt16;
typedef unsigned int       UInt32;
typedef unsigned long long UInt64;

// ============================================================
// Basic F4SE integer typedefs (signed)
// ============================================================
typedef signed char        SInt8;
typedef signed short       SInt16;
typedef signed int         SInt32;
typedef signed long long   SInt64;

// ============================================================
// Boolean type used by F4SE
// ============================================================
typedef bool Bool;

// ============================================================
// Plugin handle type used by F4SE
// ============================================================
typedef UInt32 PluginHandle;

// ============================================================
// STATIC_ASSERT macro used throughout F4SE headers
// ============================================================
#ifndef STATIC_ASSERT
#define STATIC_ASSERT(expr) static_assert(expr, #expr)
#endif
