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
#include <iostream>
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
try {
	if (argc < 2) {
		printf("Usage: %s [screen name]\n", argv[0]);
		return 0;
	}

	std::string screen_name = argv[1];

	std::string url = "https://api.twitter.com/1/statuses/user_timeline.json?screen_name=" + screen_name;

	char error[CURL_ERROR_SIZE];
	std::string result;
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
	int res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res) {
		throw std::runtime_error(error);
	}

	/* We use lazy loading here to allow very large inputs */
	json::Value doc;
	std::istringstream parser(result);
	doc.load(parser, true);

	bool end = false;
	for (json::Value i = doc.load_next(&end); !end; i = doc.load_next(&end)) {
		json::Value user = i.get("user");
		std::string from = user.get("screen_name").as_string();
		std::string created = i.get("created_at").as_string();
		std::string text = i.get("text").as_string();
		printf("<%s> %s: %s\n", created.c_str(), from.c_str(), text.c_str());
	}
	return 0;

} catch (const std::runtime_error &e) {
	fprintf(stderr, "Load error: %s\n", e.what());
	return 1;
}
