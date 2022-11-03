#include "CurlClient.h"

#ifdef HTTPS_BACKEND_CURL

#include <algorithm>
#include <dlfcn.h>
#include <stdexcept>
#include <sstream>
#include <vector>

typedef struct StringReader
{
	const std::string *str;
	size_t pos;
} StringReader;

template<typename T> bool loadSymbol(void *handle, const char *name, T &out)
{
	out = (T) dlsym(handle, name);
	return out != nullptr;
}

CurlClient::Curl::Curl()
: loaded(false)
{
	void *handle = dlopen("libcurl.so", RTLD_LAZY);
	if (!handle)
		return;

	// Load symbols
	void(*global_init)(long) = nullptr;
	if (!loadSymbol(handle, "curl_global_init", global_init))
		return;
	if (!loadSymbol(handle, "curl_global_cleanup", global_cleanup))
		return;
	if (!loadSymbol(handle, "curl_easy_init", easy_init))
		return;
	if (!loadSymbol(handle, "curl_easy_cleanup", easy_cleanup))
		return;
	if (!loadSymbol(handle, "curl_easy_setopt", easy_setopt))
		return;
	if (!loadSymbol(handle, "curl_easy_perform", easy_perform))
		return;
	if (!loadSymbol(handle, "curl_easy_getinfo", easy_getinfo))
		return;
	if (!loadSymbol(handle, "curl_slist_append", slist_append))
		return;
	if (!loadSymbol(handle, "curl_slist_free_all", slist_free_all))
		return;

	global_init(CURL_GLOBAL_DEFAULT);
	loaded = true;
}

CurlClient::Curl::~Curl()
{
	if (loaded)
		global_cleanup();
}

static char toUppercase(char c)
{
	int ch = (unsigned char) c;
	return toupper(ch);
}

static size_t stringReader(char *ptr, size_t size, size_t nmemb, StringReader *reader)
{
	const char *data = reader->str->data();
	size_t len = reader->str->length();
	size_t maxCount = (len - reader->pos) / size;
	size_t desiredBytes = std::min(maxCount, nmemb) * size;

	std::copy(data + reader->pos, data + desiredBytes, ptr);
	reader->pos += desiredBytes;

	return desiredBytes;
}

static size_t stringstreamWriter(char *ptr, size_t size, size_t nmemb, std::stringstream *ss)
{
	size_t count = size*nmemb;
	ss->write(ptr, count);
	return count;
}

static size_t headerWriter(char *ptr, size_t size, size_t nmemb, std::map<std::string,std::string> *userdata)
{
	std::map<std::string, std::string> &headers = *userdata;
	size_t count = size*nmemb;
	std::string line(ptr, count);
	size_t split = line.find(':');
	size_t newline = line.find('\r');
	if (newline == std::string::npos)
		newline = line.size();

	if (split != std::string::npos)
		headers[line.substr(0, split)] = line.substr(split+1, newline-split-1);
	return count;
}

bool CurlClient::valid() const
{
	return curl.loaded;
}

HTTPSClient::Reply CurlClient::request(const HTTPSClient::Request &req)
{
	Reply reply;
	reply.responseCode = 0;

	CURL *handle = curl.easy_init();
	if (!handle)
		throw std::runtime_error("Could not create curl request");

	curl.easy_setopt(handle, CURLOPT_URL, req.url.c_str());
	curl.easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

	std::string method = req.method;
	if (method == "")
		method = "GET";
	else
		std::transform(method.begin(), method.end(), method.begin(), toUppercase);
	curl.easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method.c_str());

	StringReader reader;

	if (req.postdata.size() > 0 && (method != "GET" && method != "HEAD"))
	{
		reader.str = &req.postdata;
		reader.pos = 0;
		curl.easy_setopt(handle, CURLOPT_UPLOAD, 1L);
		curl.easy_setopt(handle, CURLOPT_READFUNCTION, stringReader);
		curl.easy_setopt(handle, CURLOPT_READDATA, &reader);
		curl.easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) req.postdata.length());
	}

	// Curl doesn't copy memory, keep the strings around
	std::vector<std::string> lines;
	for (auto &header : req.headers)
	{
		std::stringstream line;
		line << header.first << ": " << header.second;
		lines.push_back(line.str());
	}

	curl_slist *sendHeaders = nullptr;
	for (auto &line : lines)
		sendHeaders = curl.slist_append(sendHeaders, line.c_str());

	if (sendHeaders)
		curl.easy_setopt(handle, CURLOPT_HTTPHEADER, sendHeaders);

	std::stringstream body;

	curl.easy_setopt(handle, CURLOPT_WRITEFUNCTION, stringstreamWriter);
	curl.easy_setopt(handle, CURLOPT_WRITEDATA, &body);

	curl.easy_setopt(handle, CURLOPT_HEADERFUNCTION, headerWriter);
	curl.easy_setopt(handle, CURLOPT_HEADERDATA, &reply.headers);

	curl.easy_perform(handle);

	if (sendHeaders)
		curl.slist_free_all(sendHeaders);

	{
		long responseCode;
		curl.easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &responseCode);
		reply.responseCode = (int) responseCode;
	}

	reply.body = body.str();

	curl.easy_cleanup(handle);
	return reply;
}

CurlClient::Curl CurlClient::curl;

#endif // HTTPS_BACKEND_CURL
