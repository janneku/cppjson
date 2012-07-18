#include "cppjson.h"
#include <stdio.h>
#include <sstream>

void verify(const json::Value &value, const char *encoded)
{
	/* try to encode the value and verify that it decodes to the same */
	std::ostringstream ss;
	value.write(ss);
	json::Value value2;
	std::istringstream parser(ss.str());
	value2.load_all(parser);
	assert(value == value2);

	/* try to load the JSON string, and check that it is equal */
	parser.str(encoded);
	parser.clear();
	value2.load_all(parser);
	assert(value == value2);

	/* Verify lazy loading */
	parser.str(encoded);
	parser.clear();
	value2.load_all(parser, true);
}

void verify_error(const char *s, const char *error)
{
	json::Value val;
	std::istringstream ss(s);
	try {
		val.load_all(ss);
		/* should always raise an exception */
		assert(0);
	} catch (const json::decode_error &e) {
		assert(e.what() == std::string(error));
	}
}

void test_lazy_array()
{
	std::istringstream parser("{\"a\": [1, \"foo\"], \"b\": [2, \"bar\"]}");
	json::Value value;
	value.load_all(parser, true);
	json::Value &a = value.get("a");
	json::Value &b = value.get("b");
	assert(a.load_next().as_integer() == 1);
	assert(b.load_next().as_integer() == 2);
	assert(a.load_next().as_string() == "foo");
	bool end = false;
	a.load_next(&end);
	assert(end);
	assert(b.load_next().as_string() == "bar");
	b.load_next(&end);
	assert(end);
}

int main()
{
	const uint8_t snowman[] = {0xE2, 0x98, 0x83, 0};

	verify(1234, "1234");
	verify(-1234, "-1234");
	verify(1234, "1234.");
	verify(1234, "1234 // a comment\n\n");
	verify(1234, "//a comment\n\n1234");
	verify(1234, "//\n1234");
	verify(1234.0, "1234");
	verify(1234.56, "1234.56");
	verify(-1234.56, "-1234.56");
	verify(1234, "\t1234 \n");
	verify("foobar", "\"foobar\"");
	verify("snow" + std::string((char *) snowman) + "man", "\"snow\\u2603man\"");
	verify("", "\"\"");
	verify(" /\r\n \"\\", "\" \\/\\r\\n \\\"\\\\\" ");
	verify(true, "true");
	verify(false, "false");

	std::vector<json::Value> arr;
	verify(arr, "[] ");

	arr.push_back("foo");
	arr.push_back(1234);
	arr.push_back(-1234.56);
	arr.push_back(true);
	verify(arr, "[\"foo\",\n 1234,\t-1234.56\n, true] ");

	json::object_map_t obj;
	verify(obj, "{}");
	obj["bar"] = arr;
	obj["foo"] = "test";
	verify(obj, "{\"bar\" :[ \"foo\" ,1234,-1234.56, true, ], \"foo\": \"test\"}\n");

	std::vector<json::Value> arr2;
	arr2.push_back(obj);
	arr2.push_back(123);
	verify(arr2, "[{\"bar\":[\"foo\",1234 ,-1234.56,true],\"foo\":\"test\"} ,123\n]");

	verify_error("foobar", "Unknown keyword in input");
	verify_error("-foo", "Expected a digit");
	verify_error("trueorfalse", "Unknown keyword in input");
	verify_error("\"foobar", "Unexpected end of input");
	verify_error("[,] ", "Unknown character in input");
	verify_error("[1234, ", "Unexpected end of input");
	verify_error(" [1 2]", "Expected ',' or ']'");
	verify_error("{\"foo\": ,} ", "Unknown character in input");
	verify_error("{ \"foo\": 1234, ", "Unexpected end of input");
	verify_error("{1234.56}", "Expected a string");
	verify_error("{\"a\": [] ", "Expected ',' or '}'");
	verify_error("{\"a\" 5 ", "Expected ':'");
	verify_error("11111111111111111111", "Invalid integer");
	verify_error(" /", "Expected '/'");

	test_lazy_array();

	printf("ok\n");
	return 0;
}
