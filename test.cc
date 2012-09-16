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
	parser.str(ss.str());
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
	/* Test basic types */
	verify(1234, "1234");
	verify(-1234, "-1234");
	verify(1234, "1234.");
	verify(1234.56, "1234.56");
	verify(-1234.56, "-1234.56");
	verify(1234e10, "1234e10");
	verify(1234e-10, "1234e-10");
	verify("", "\"\"");
	verify("foobar", "\"foobar\"");
	verify(true, "true");
	verify(false, "false");
	verify(json::Value(), "null");
	verify(json::object_map_t(), "{}");
	verify(std::vector<json::Value>(), "[]");
	verify(1234, "\t1234");
	verify(1234, "1234\n");

	/* Floating points and integers should be treated as the same */
	verify(1234.0, "1234");
	verify(1234, "1234.0");
	verify_error("1234e", "Invalid number");
	verify_error("-", "Invalid number");
	verify_error("-foo", "Invalid number");
	verify_error("1-e2", "Invalid number");
	verify_error("1-", "Invalid number");
	verify_error("11111111111111111111", "Invalid number");

	/* Test that unicode is converted to UTF-8 */
	verify("snow\xE2\x98\x83man", "\"snow\\u2603man\"");

	verify("\r\n\t\f\b", "\"\\r\\n\\t\\f\\b\"");
	verify("foo\nbar ", "\"foo\\nbar \"");

	/* Test comments */
	verify(1234, "1234// a comment");
	verify(1234, "1234// \"foobar\"");
	verify(1234, "1234\n// a comment");
	verify(1234, "//acomment {}[]\n\n1234");
	verify(1234, "//\n1234");
	verify_error(" /", "Expected '/'");
	verify_error(" /x", "Expected '/'");

	/* Test arrays */
	std::vector<json::Value> arr;
	arr.push_back("foo");
	arr.push_back(1234);
	arr.push_back(-1234.56);
	arr.push_back(true);
	verify(arr, "[\"foo\", 1234, -1234.56, true]");
	verify(arr, "[\"foo\", 1234, -1234.56, true,]");
	verify(arr, " [ \"foo\" ,1234,\n-1234.56,\ttrue ]");
	verify_error("[,] ", "Unknown character in input");
	verify_error("[1234, ", "Unexpected end of input");
	verify_error(" [1 2]", "Expected ',' or ']'");

	/* Test objects */
	json::object_map_t obj;
	obj["bar"] = arr;
	obj["foo"] = "test";
	verify(obj, "{\"bar\": [\"foo\",1234,-1234.56,true], \"foo\": \"test\"}");
	verify(obj, "{\n\"bar\" :[ \"foo\" ,1234,-1234.56, true, ], \t\"foo\"\n: \"test\"\n,}");
	verify_error("{\"foo\": ,} ", "Unknown character in input");
	verify_error("{ \"foo\": 1234, ", "Unexpected end of input");
	verify_error("{1234.56}", "Expected '}' or a string");
	verify_error("{\"a\": [] ", "Expected ',' or '}'");
	verify_error("{\"a\" 5 ", "Expected ':'");

	/* A list that contains an object */
	std::vector<json::Value> arr2;
	arr2.push_back(obj);
	arr2.push_back(123e10);
	verify(arr2, "[{\"bar\": [\"foo\", 1234, -1234.56, true], \"foo\": \"test\"}, 123e10]");
	verify(arr2, "[{\"bar\":[\"foo\",1234 ,-1234.56,true],\"foo\":\"test\"} ,123e+10\n, ]");

	verify_error("foobar", "Unknown keyword in input");
	verify_error("trueorfalse", "Unknown keyword in input");
	verify_error("\"foobar", "Unexpected end of input");
	verify_error("\"foo\\xbar\"", "Unknown character entity");
	verify_error("\"foo\\", "Unexpected end of input");
	verify_error("\"foo\\u12", "Unexpected end of input");
	verify_error("\"foo\\ubarz\"", "Invalid unicode");
	verify_error("? ", "Unknown character in input");
	verify_error("\"foo\nbar\"", "Control character in a string");
	verify_error("\"foo\nbar\"", "Control character in a string");

	try {
		json::Value val;
		std::istringstream ss("{\"bar\": 123}");
		val.load_all(ss);
		int i = val.get("foo").as_integer();
		(void) i;
	} catch (const json::type_error &e) {
		assert(e.what() == std::string("Expected type integer, but got null"));
	}

	try {
		json::Value val;
		std::istringstream ss("{\"bar\": 123, \"foo\": true}");
		val.load_all(ss);
		int i = val.get("foo").as_integer();
		(void) i;
	} catch (const json::type_error &e) {
		assert(e.what() == std::string("Expected type integer, but got boolean"));
	}

	test_lazy_array();

	printf("ok\n");
	return 0;
}
