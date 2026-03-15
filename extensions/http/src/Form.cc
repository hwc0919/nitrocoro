#include <nitrocoro/http/Form.h>

namespace nitrocoro::http
{

static constexpr char kHex[] = "0123456789ABCDEF";

static bool isUnreserved(char c)
{
    return (c >= 'A' && c <= 'Z')
           || (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9')
           || c == '-' || c == '_' || c == '.' || c == '~';
}

static int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static void percentEncode(std::string & result, char c)
{
    auto uc = static_cast<unsigned char>(c);
    result += '%';
    result += kHex[uc >> 4];
    result += kHex[uc & 0xf];
}

std::string formEncode(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input)
    {
        if (isUnreserved(c))
            result += c;
        else if (c == ' ')
            result += '+';
        else if (c == '+')
            result += "%2B";
        else
            percentEncode(result, c);
    }
    return result;
}

std::string formDecode(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        if (c == '+')
        {
            result += ' ';
        }
        else if (c == '%')
        {
            if (i + 2 < input.size())
            {
                int hi = hexVal(input[i + 1]);
                int lo = hexVal(input[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    char decoded = static_cast<char>((hi << 4) | lo);
                    result += decoded;
                    i += 2;
                    continue;
                }
            }
            result += '%'; // invalid sequence, keep as-is
        }
        else
        {
            result += c;
        }
    }
    return result;
}

HttpFormMap parseFormData(std::string_view formBody)
{
    HttpFormMap result;

    size_t start = 0;
    while (start < formBody.size())
    {
        size_t ampPos = formBody.find('&', start);
        size_t end = (ampPos == std::string_view::npos) ? formBody.size() : ampPos;
        std::string_view pair = formBody.substr(start, end - start);

        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos)
        {
            auto key = formDecode(pair.substr(0, eqPos));
            auto value = formDecode(pair.substr(eqPos + 1));
            result[std::move(key)] = std::move(value);
        }
        else if (!pair.empty())
        {
            result[formDecode(pair)] = "";
        }

        if (ampPos == std::string_view::npos)
            break;
        start = ampPos + 1;
    }

    return result;
}

HttpMultiFormMap parseMultiFormData(std::string_view formBody)
{
    HttpMultiFormMap result;

    size_t start = 0;
    while (start < formBody.size())
    {
        size_t ampPos = formBody.find('&', start);
        size_t end = (ampPos == std::string_view::npos) ? formBody.size() : ampPos;
        std::string_view pair = formBody.substr(start, end - start);

        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos)
        {
            auto key = formDecode(pair.substr(0, eqPos));
            auto value = formDecode(pair.substr(eqPos + 1));
            result[key].push_back(std::move(value));
        }
        else if (!pair.empty())
        {
            auto key = formDecode(pair);
            result[std::move(key)] = {};
        }

        if (ampPos == std::string_view::npos)
            break;
        start = ampPos + 1;
    }

    return result;
}

} // namespace nitrocoro::http
