/**
 * @file Form.h
 * @brief HTTP form data parsing and encoding utilities
 */
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace nitrocoro::http
{

using HttpFormMap = std::map<std::string, std::string, std::less<>>;
using HttpMultiFormMap = std::map<std::string, std::vector<std::string>, std::less<>>;

/**
 * Parse application/x-www-form-urlencoded form data.
 * @param formBody The raw form data string
 * @return Map of form field names to values (single value per key)
 */
HttpFormMap parseFormData(std::string_view formBody);

/**
 * Parse application/x-www-form-urlencoded form data with support for multiple values per key.
 * @param formBody The raw form data string
 * @return Map of form field names to vectors of values (multiple values per key)
 */
HttpMultiFormMap parseMultiFormData(std::string_view formBody);

/**
 * Encode a single form (application/x-www-form-urlencoded) key/value component.
 * Space -> '+', '/' -> '%2F', '+' -> '%2B'.
 * @param input The string to encode
 * @return The form-encoded string
 */
std::string formEncode(std::string_view input);

/**
 * Decode a single form key/value component.
 * Both '+' and '%20' are decoded to space. '%2B' is decoded to '+'.
 * Invalid percent-encoded sequences are kept as-is.
 * @param input The form-encoded string to decode
 * @return The decoded string
 */
std::string formDecode(std::string_view input);

} // namespace nitrocoro::http
