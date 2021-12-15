#include <lua.hpp>

// Sorry for the ifdef soup ahead
#include "common/config.h"
#include "common/HTTPSClient.h"
#include "common/ConnectionClient.h"
#ifdef USE_CURL_BACKEND
#	include "generic/CurlClient.h"
#endif
#ifdef USE_OPENSSL_BACKEND
#	include "generic/OpenSSLConnection.h"
#endif
#ifdef USE_SCHANNEL_BACKEND
#	include "windows/SChannelConnection.h"
#endif
#ifdef USE_NSURL_BACKEND
#	include "macos/NSURLClient.h"
#endif

#ifdef USE_CURL_BACKEND
	static CurlClient curlclient;
#endif
#ifdef USE_OPENSSL_BACKEND
	static ConnectionClient<OpenSSLConnection> opensslclient;
#endif
#ifdef USE_SCHANNEL_BACKEND
	static ConnectionClient<SChannelConnection> schannelclient;
#endif
#ifdef USE_NSURL_BACKEND
	static NSURLClient nsurlclient;
#endif

static HTTPSClient *clients[] = {
#ifdef USE_CURL_BACKEND
	&curlclient,
#endif
#ifdef USE_OPENSSL_BACKEND
	&opensslclient,
#endif
#ifdef USE_SCHANNEL_BACKEND
	&schannelclient,
#endif
#ifdef USE_NSURL_BACKEND
	&nsurlclient,
#endif
	nullptr,
};

static std::string w_checkstring(lua_State *L, int idx)
{
	size_t len;
	const char *str = luaL_checklstring(L, idx, &len);
	return std::string(str, len);
}

static void w_pushstring(lua_State *L, const std::string &str)
{
	lua_pushlstring(L, str.data(), str.size());
}

static void w_readheaders(lua_State *L, int idx, HTTPSClient::header_map &headers)
{
	if (idx < 0)
		idx += lua_gettop(L) + 1;

	lua_pushnil(L);
	while (lua_next(L, idx))
	{
		auto header = w_checkstring(L, -2);
		headers[header] = w_checkstring(L, -1);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static HTTPSClient::Request::Method w_optmethod(lua_State *L, int idx, HTTPSClient::Request::Method defaultMethod)
{
	if (lua_isnoneornil(L, idx))
		return defaultMethod;

	auto str = w_checkstring(L, idx);
	if (str == "get")
		return HTTPSClient::Request::GET;
	else if (str == "post")
		return HTTPSClient::Request::POST;
	else
		luaL_argerror(L, idx, "expected one of \"get\" or \"set\"");

	return defaultMethod;
}

static int w_request(lua_State *L)
{
	auto url = w_checkstring(L, 1);
	HTTPSClient::Request req(url);

	std::string errorMessage("No applicable implementation found");
	bool foundClient = false;
	bool advanced = false;

	if (lua_istable(L, 2))
	{
		advanced = true;

		HTTPSClient::Request::Method defaultMethod = HTTPSClient::Request::GET;

		lua_getfield(L, 2, "data");
		if (!lua_isnoneornil(L, -1))
		{
			req.postdata = w_checkstring(L, -1);
			defaultMethod = HTTPSClient::Request::POST;
		}
		lua_pop(L, 1);

		lua_getfield(L, 2, "method");
		req.method = w_optmethod(L, -1, defaultMethod);
		lua_pop(L, 1);

		lua_getfield(L, 2, "headers");
		if (!lua_isnoneornil(L, -1))
			w_readheaders(L, -1, req.headers);
		lua_pop(L, 1);
	}

	for (size_t i = 0; clients[i]; ++i)
	{
		HTTPSClient &client = *clients[i];
		HTTPSClient::Reply reply;

		if (!client.valid())
			continue;

		try
		{
			reply = client.request(req);
		}
		catch(const std::exception& e)
		{
			errorMessage = e.what();
			break;
		}
		
		lua_pushinteger(L, reply.responseCode);
		w_pushstring(L, reply.body);

		if (advanced)
		{
			lua_newtable(L);
			for (const auto &header : reply.headers)
			{
				w_pushstring(L, header.first);
				w_pushstring(L, header.second);
				lua_settable(L, -3);
			}
		}

		foundClient = true;
		break;
	}

	if (!foundClient)
	{
		lua_pushnil(L);
		lua_pushstring(L, errorMessage.c_str());
	}

	return (advanced && foundClient) ? 3 : 2;
}

extern "C" int DLLEXPORT luaopen_https(lua_State *L)
{
	lua_newtable(L);

	lua_pushcfunction(L, w_request);
	lua_setfield(L, -2, "request");

	return 1;
}
