/*
 * cppjson - JSON (de)serialization library for C++ and STL
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>
 *
 * Program code is licensed with GNU LGPL 2.1. See COPYING.LGPL file.
 */
#include "cppjson.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#define FOR_EACH_CONST(type, i, cont)		\
	for (type::const_iterator i = (cont).begin(); i != (cont).end(); ++i)

namespace json {

/* Format a string, similar to sprintf() */
const std::string strf(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	char *buf = NULL;
	vasprintf(&buf, fmt, vl);
	va_end(vl);
	std::string s(buf);
	free(buf);
	return s;
}

size_t encode_utf8(int c, uint8_t *buffer)
{
	if (c < 0x80) {
		buffer[0] = c;
		return 1;
	} else if (c < 0x800) {
		buffer[0] = 0xC0 + ((c & 0x7C0) >> 6);
		buffer[1] = 0x80 + ((c & 0x03F));
		return 2;
	} else if (c < 0x10000) {
		buffer[0] = 0xE0 + ((c & 0xF000) >> 12);
		buffer[1] = 0x80 + ((c & 0x0FC0) >> 6);
		buffer[2] = 0x80 + ((c & 0x003F));
		return 3;
	} else if (c <= 0x10FFFF) {
		buffer[0] = 0xF0 + ((c & 0x1C0000) >> 18);
		buffer[1] = 0x80 + ((c & 0x03F000) >> 12);
		buffer[2] = 0x80 + ((c & 0x000FC0) >> 6);
		buffer[3] = 0x80 + ((c & 0x00003F));
		return 4;
	}
	return 0;
}

static const char *type_names[] = {
	"null",
	"string",
	"integer",
	"floating",
	"boolean",
	"object",
	"array",
};

Value::Value(Type type) :
	m_type(type)
{
	switch (m_type) {
	case JSON_OBJECT:
		m_value.object = new object_map_t;
		break;
	case JSON_ARRAY:
		m_value.array = new std::vector<Value>;
		break;
	case JSON_NULL:
		break;
	default:
		/* other types can not be constructed */
		assert(0);
	}
}

Value::Value(const std::string &s) :
	m_type(JSON_STRING)
{
	m_value.string = new std::string(s);
}

Value::Value(const char *s) :
	m_type(JSON_STRING)
{
	m_value.string = new std::string(s);
}

Value::Value(int i) :
	m_type(JSON_INTEGER)
{
	m_value.integer = i;
}

Value::Value(double d) :
	m_type(JSON_FLOATING)
{
	m_value.floating = d;
}

Value::Value(bool b) :
	m_type(JSON_BOOLEAN)
{
	m_value.boolean = b;
}

Value::Value(const object_map_t &object) :
	m_type(JSON_OBJECT)
{
	m_value.object = new object_map_t(object);
}

Value::Value(const std::vector<Value> &array) :
	m_type(JSON_ARRAY)
{
	m_value.array = new std::vector<Value>(array);
}

Value::Value(const Value &from) :
	m_type(JSON_NULL)
{
	*this = from;
}

Value::~Value()
{
	destroy();
}

void Value::destroy()
{
	switch (m_type) {
	case JSON_STRING:
		delete m_value.string;
		break;
	case JSON_OBJECT:
		delete m_value.object;
		break;
	case JSON_ARRAY:
		delete m_value.array;
		break;
	case JSON_NULL:
	case JSON_BOOLEAN:
	case JSON_INTEGER:
	case JSON_FLOATING:
		break;
	}
	m_type = JSON_NULL;
}

void Value::verify_type(Type expected) const
{
	if (expected != m_type) {
		throw type_error(strf("Expected type %s, but got %s",
				 type_names[expected], type_names[m_type]));
	}
}

void Value::operator = (const Value &from)
{
	if (this == &from)
		return;
	destroy();
	m_type = from.m_type;
	switch (m_type) {
	case JSON_NULL:
		break;
	case JSON_STRING:
		m_value.string = new std::string(*from.m_value.string);
		break;
	case JSON_OBJECT:
		m_value.object = new object_map_t(*from.m_value.object);
		break;
	case JSON_ARRAY:
		m_value.array = new std::vector<Value>(*from.m_value.array);
		break;
	case JSON_INTEGER:
		m_value.integer = from.m_value.integer;
		break;
	case JSON_FLOATING:
		m_value.floating = from.m_value.floating;
		break;
	case JSON_BOOLEAN:
		m_value.boolean = from.m_value.boolean;
		break;
	default:
		assert(0);
	}
}

Type cmp_type(Type type)
{
	if (type == JSON_INTEGER)
		return JSON_FLOATING;
	return type;
}

bool Value::operator == (const Value &other) const
{
	if (cmp_type(m_type) != cmp_type(other.m_type))
		return false;
	switch (m_type) {
	case JSON_NULL:
		return true;
	case JSON_STRING:
		return *m_value.string == *other.m_value.string;
	case JSON_OBJECT:
		return *m_value.object == *other.m_value.object;
	case JSON_ARRAY:
		return *m_value.array == *other.m_value.array;
	case JSON_INTEGER:
		if (other.m_type == JSON_INTEGER)
			return m_value.integer == other.m_value.integer;
		else
			return m_value.integer == other.m_value.floating;
	case JSON_FLOATING:
		if (other.m_type == JSON_INTEGER)
			return m_value.floating == other.m_value.integer;
		else
			return m_value.floating == other.m_value.floating;
	case JSON_BOOLEAN:
		return m_value.boolean == other.m_value.boolean;
	default:
		assert(0);
	}
}

bool Value::operator != (const Value &other) const
{
	return !(*this == other);
}

/* Skips over all spaces in the input. The stream can end up in EOF state. */
void skip_space(std::istream &is)
{
	int c = is.peek();
	while (!is.eof()) {
		/* Skip over C++-style comments */
		if (c == '/') {
			is.get();
			if (is.get() != '/') {
				throw decode_error("Expected '/'");
			}
			while (is.peek() != '\n' && !is.eof())
				is.get();
		} else if (!isspace(c))
			break;
		else
			is.get();
		c = is.peek();
	}
}

std::string load_string(std::istream &is)
{
	int c = is.get();
	if (is.eof()) {
		throw decode_error("Unexpected end of input");
	}
	if (c != '"') {
		throw decode_error("Expected a string");
	}
	std::string str;
	while (1) {
		c = is.get();
		if (is.eof()) {
			throw decode_error("Unexpected end of input");
		}
		if (c == '"')
			break;
		if (c == '\\') {
			c = is.get();
			if (is.eof()) {
				throw decode_error("Unexpected end of input");
			}
			switch (c) {
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case '\\':
			case '/':
			case '"':
				/* pass through */
				break;
			case 'u':
				{
					char code[5];
					code[4] = 0;
					is.read(code, 4);
					if (is.eof()) {
						throw decode_error("Unexpected end of input");
					}
					std::istringstream parser(code);
					parser >> std::hex >> c;
					if (!parser) {
						throw decode_error("Invalid unicode");
					}
				}
				break;
			default:
				throw decode_error("Unknown character entity");
			}
			uint8_t buffer[4];
			size_t len = encode_utf8(c, buffer);
			str.append((char *) buffer, len);
		} else {
			/* assume input is UTF-8 and pass through unmodified */
			str += char(c);
		}
	}
	return str;
}

void write_string(std::ostream &os, const std::string &str)
{
	os.put('"');
	for (size_t i = 0; i < str.size(); ++i) {
		int c = str[i];
		switch (c) {
		case '\n':
			os.put('\\');
			c = 'n';
			break;
		case '\r':
			os.put('\\');
			c = 'r';
			break;
		case '"':
		case '\\':
			os.put('\\');
			break;
		default:
			break;
		}
		os.put(c);
	}
	os.put('"');
}

void Value::load(std::istream &is)
{
	destroy();

	/*
	 * Note, we take adventage of the fact that when EOF is reached,
	 * peek() and get() returns a special value that doesn't match
	 * anything else.
	 */
	skip_space(is);
	if (is.eof()) {
		throw decode_error("Unexpected end of input");
	}
	int c = is.peek();
	switch (c) {
	case '{':
		is.get();
		m_type = JSON_OBJECT;
		m_value.object = new object_map_t;
		while (1) {
			skip_space(is);
			if (is.peek() == '}') {
				is.get();
				break;
			}
			std::string key = load_string(is);
			skip_space(is);
			if (is.get() != ':') {
				throw decode_error("Expected ':'");
			}
			/*
			 * To avoid a copy, first insert an empty value to
			 * the container and then load it from the input.
			 */
			std::pair<object_map_t::iterator, bool> res =
				m_value.object->insert(std::make_pair(key, Value()));
			if (!res.second) {
				throw decode_error("Duplicate key in object");
			}
			res.first->second.load(is);

			skip_space(is);
			c = is.get();
			if (c == '}') {
				break;
			} else if (c != ',') {
				throw decode_error("Expected ',' or '}'");
			}
		}
		break;

	case '[':
		is.get();
		m_type = JSON_ARRAY;
		m_value.array = new std::vector<Value>;
		while (1) {
			skip_space(is);
			if (is.peek() == ']') {
				is.get();
				break;
			}
			m_value.array->push_back(Value());
			m_value.array->back().load(is);

			skip_space(is);
			c = is.get();
			if (c == ']') {
				break;
			} else if (c != ',') {
				throw decode_error("Expected ',' or ']'");
			}
		}
		break;

	case '"':
		m_value.string = new std::string(load_string(is));
		m_type = JSON_STRING;
		break;
	default:
		if ((c >= '0' && c <= '9') || c == '-') {
			/*
			 * We need first to parse the number to a buffer to
			 * decide if it's a float or an intger.
			 */
			std::string str;
			if (c == '-') {
				str += char(c);
				is.get();
				c = is.peek();
				if (c < '0' || c > '9') {
					throw decode_error("Expected a digit");
				}
			}
			while (!is.eof() && isdigit(c)) {
				str += char(c);
				is.get();
				c = is.peek();
			}
			if (c == '.') {
				str += char(c);
				is.get();
				c = is.peek();
				while (!is.eof() && isdigit(c)) {
					str += char(c);
					is.get();
					c = is.peek();
				}
				std::istringstream parser(str);
				m_type = JSON_FLOATING;
				parser >> m_value.floating;
				if (!parser) {
					throw decode_error("Invalid integer");
				}
			} else {
				std::istringstream parser(str);
				m_type = JSON_INTEGER;
				parser >> m_value.integer;
				if (!parser) {
					throw decode_error("Invalid integer");
				}
			}
		} else if (isalpha(c)) {
			std::string str;
			while (!is.eof() && isalpha(c)) {
				str += char(tolower(c));
				is.get();
				c = is.peek();
			}
			if (str == "true") {
				m_type = JSON_BOOLEAN;
				m_value.boolean = true;
			} else if (str == "false") {
				m_type = JSON_BOOLEAN;
				m_value.boolean = false;
			} else if (str == "null") {
				m_type = JSON_NULL;
			} else {
				throw decode_error("Unknown keyword in input");
			}
		} else {
			throw decode_error("Unknown character in input");
		}
	}
}

void Value::load_all(std::istream &is)
{
	load(is);
	skip_space(is);
	if (!is.eof()) {
		throw decode_error("Left over data in input");
	}
}

void Value::write(std::ostream &os, int indent) const
{
	static int depth = 0;

	switch (m_type) {
	case JSON_STRING:
		write_string(os, *m_value.string);
		break;
	case JSON_OBJECT:
		os.put('{');
		depth++;
		FOR_EACH_CONST(object_map_t, i, *m_value.object) {
			if (i != m_value.object->begin())
				os << ", ";
			if (indent) {
				os.put('\n');
				for (int n = 0; n < indent * depth; ++n)
					os.put(' ');
			}
			write_string(os, i->first);
			os << ": ";
			i->second.write(os, indent);
		}
		depth--;
		if (indent) {
			os.put('\n');
			for (int n = 0; n < indent * depth; ++n)
				os.put(' ');
		}
		os.put('}');
		break;
	case JSON_ARRAY:
		os.put('[');
		FOR_EACH_CONST(std::vector<Value>, i, *m_value.array) {
			if (i != m_value.array->begin())
				os << ", ";
			i->write(os, indent);
		}
		os.put(']');
		break;
	case JSON_INTEGER:
		os << m_value.integer;
		break;
	case JSON_FLOATING:
		os << m_value.floating;
		break;
	case JSON_BOOLEAN:
		os << (m_value.boolean ? "true" : "false");
		break;
	case JSON_NULL:
		os << "null";
		break;
	default:
		assert(0);
	}
}

}
