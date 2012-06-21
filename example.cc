/*
 * cppjson - JSON (de)serialization library for C++ and STL
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>
 *
 * Program code is licensed with GNU LGPL 2.1. See COPYING.LGPL file.
 *
 * Fetch and display a Twitter feed.
 */
#include "cppjson.h"
#include <sstream>
#include <curl/curl.h>
#include <curl/easy.h>

#define FOR_EACH_CONST(type, i, cont)		\
	for (type::const_iterator i = (cont).begin(); i != (cont).end(); ++i)

size_t write_func(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::string *str = (std::string *) userdata;
	str->append((char *) ptr, size * nmemb);
	return size * nmemb;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s [screen name]\n", argv[0]);
		return 0;
	}

	std::string screen_name = argv[1];

	std::string url = "https://api.twitter.com/1/statuses/user_timeline.json?screen_name=" + screen_name;

	std::string result;
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
	int res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	json::Value doc;
	std::istringstream parser(result);
	doc.load(parser);

	FOR_EACH_CONST(std::vector<json::Value>, i, doc.as_array()) {
		std::string created = i->get("created_at").as_string();
		std::string text = i->get("text").as_string();
		printf("<%s> %s\n", created.c_str(), text.c_str());
	}
	return 0;
}
