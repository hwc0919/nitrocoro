/**
 * @file ExtendableBuffer.h
 * @brief Concept for extendable buffer interface
 */
#pragma once

#include <concepts>
#include <cstddef>

namespace nitrocoro::utils
{

/**
 * @brief Concept for a buffer that can be extended and written to
 * 
 * An ExtendableBuffer must support:
 * - prepareWrite(n): Ensure at least n bytes writable, return write pointer
 * - beginWrite(): Get current write position without extending
 * - writableSize(): Get current writable size without extending
 * - commitWrite(n): Commit n bytes as written
 */
template <typename T>
concept ExtendableBuffer = requires(T buf, size_t n) {
    { buf.prepareWrite(n) } -> std::same_as<char*>;
    { buf.beginWrite() } -> std::same_as<char*>;
    { buf.writableSize() } -> std::same_as<size_t>;
    { buf.commitWrite(n) } -> std::same_as<void>;
};

} // namespace nitrocoro::utils
