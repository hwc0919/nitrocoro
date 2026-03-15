#include <nitrocoro/http/Form.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;
using namespace nitrocoro::http;

NITRO_TEST(form_parse_basic_data)
{
    std::string formData = "name=Alice&age=25&city=New+York";
    auto result = parseFormData(formData);

    NITRO_CHECK_EQ(result.size(), 3);
    NITRO_CHECK_EQ(result.at("name"), "Alice");
    NITRO_CHECK_EQ(result.at("age"), "25");
    NITRO_CHECK_EQ(result.at("city"), "New York");
    co_return;
}

NITRO_TEST(form_parse_empty_values)
{
    std::string formData = "name=&age=25&empty";
    auto result = parseFormData(formData);

    NITRO_CHECK_EQ(result.size(), 3);
    NITRO_CHECK(result.at("name").empty());
    NITRO_CHECK_EQ(result.at("age"), "25");
    NITRO_CHECK(result.at("empty").empty());
    co_return;
}

NITRO_TEST(form_parse_url_encoded)
{
    std::string formData = "message=Hello%20World%21&special=%2B%26%3D";
    auto result = parseFormData(formData);

    NITRO_CHECK_EQ(result.size(), 2);
    NITRO_CHECK_EQ(result.at("message"), "Hello World!");
    NITRO_CHECK_EQ(result.at("special"), "+&=");
    co_return;
}

NITRO_TEST(form_parse_multi_data)
{
    std::string formData = "color=red&color=blue&name=Alice";
    auto result = parseMultiFormData(formData);

    NITRO_CHECK_EQ(result.size(), 2);
    NITRO_CHECK_EQ(result.at("color").size(), 2);
    NITRO_CHECK_EQ(result.at("color")[0], "red");
    NITRO_CHECK_EQ(result.at("color")[1], "blue");
    NITRO_CHECK_EQ(result.at("name").size(), 1);
    NITRO_CHECK_EQ(result.at("name")[0], "Alice");
    co_return;
}

NITRO_TEST(form_parse_empty_string)
{
    auto result = parseFormData("");
    NITRO_CHECK_EQ(result.size(), 0);

    auto multiResult = parseMultiFormData("");
    NITRO_CHECK_EQ(multiResult.size(), 0);
    co_return;
}

NITRO_TEST(form_encode_basic)
{
    NITRO_CHECK_EQ(formEncode("hello world"), "hello+world"); // space -> +
    NITRO_CHECK_EQ(formEncode("foo/bar"), "foo%2Fbar");       // / encoded
    NITRO_CHECK_EQ(formEncode("a+b"), "a%2Bb");               // + -> %2B
    NITRO_CHECK_EQ(formEncode("foo=bar&baz"), "foo%3Dbar%26baz");
    NITRO_CHECK_EQ(formEncode("abc-_.~"), "abc-_.~");
    NITRO_CHECK_EQ(formEncode(""), "");
    co_return;
}

NITRO_TEST(form_decode_basic)
{
    NITRO_CHECK_EQ(formDecode("hello+world"), "hello world");
    NITRO_CHECK_EQ(formDecode("hello%20world"), "hello world");
    NITRO_CHECK_EQ(formDecode("a%2Bb"), "a+b");
    NITRO_CHECK_EQ(formDecode("foo%2Fbar"), "foo/bar");
    NITRO_CHECK_EQ(formDecode(""), "");
    co_return;
}

NITRO_TEST(form_decode_invalid)
{
    NITRO_CHECK_EQ(formDecode("foo%2"), "foo%2");
    NITRO_CHECK_EQ(formDecode("foo%GG"), "foo%GG");
    co_return;
}

NITRO_TEST(form_encode_decode_round_trip)
{
    std::string original = "Hello World! +&=@#$%";
    std::string encoded = formEncode(original);
    std::string decoded = formDecode(encoded);
    NITRO_CHECK_EQ(decoded, original);
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
