local https = require "https"
local json

-- Helper

local function hexencode(c)
	return string.format("%%%02x", string.byte(c))
end

local function escape(s)
	return (string.gsub(s, "([^A-Za-z0-9_])", hexencode))
end

local function urlencode(list)
	local result = {}

	for k, v in pairs(list) do
		result[#result + 1] = escape(k).."="..escape(v)
	end

	return table.concat(result, "&")
end

local function checkcode(code, expected)
	if code ~= expected then
		error("expected code "..expected..", got "..tostring(code))
	end
end

math.randomseed(os.time())

-- Tests function

local function test_download_json()
	local code, response = https.request("https://raw.githubusercontent.com/rxi/json.lua/master/json.lua")
	checkcode(code, 200)
	json = assert(loadstring(response, "=json.lua"))()
end

local function test_head()
	local code, response = https.request("https://postman-echo.com/get", {method = "HEAD"})
	assert(code == 200, "expected code 200, got "..code)
	assert(#response == 0, "expected empty response")
end

local function test_custom_header()
	local headerName = "RandomNumber"
	local random = math.random(1, 1000)
	local code, response = https.request("https://postman-echo.com/get", {
		headers = {
			[headerName] = tostring(random)
		}
	})
	checkcode(code, 200)
	local root = json.decode(response)

	-- Headers are case-insensitive
	local found = false
	for k, v in pairs(root.headers) do
		if k:lower() == headerName:lower() then
			assert(tonumber(v) == random, "random number does not match, expected "..random..", got "..v)
			found = true
		end
	end

	assert(found, "custom header RandomNumber not found")
end

local function test_send(method, kind)
	local data = {Foo = "Bar", Key = "Value"}
	local input, contentType
	if kind == "json" then
		input = json.encode
		contentType = "application/json"
	else
		input = urlencode
		contentType = "application/x-www-form-urlencoded"
	end

	local code, response = https.request("https://postman-echo.com/"..method:lower(), {
		headers = {["Content-Type"] = contentType},
		data = input(data),
		method = method
	})

	checkcode(code, 200)
	local root = json.decode(response)

	for k, v in pairs(data) do
		local v0 = assert(root[kind][k], "Missing key "..k.." for "..kind)
		assert(v0 == v, "Key "..k.." value mismatch, expected '"..v.."' got '"..v0.."'")
	end
end

-- Tests call
print("test downloading json library") test_download_json()
print("test custom header") test_custom_header()
print("test HEAD") test_head()

for _, method in ipairs({"POST", "PUT", "PATCH", "DELETE"}) do
	for _, kind in ipairs({"form", "json"}) do
		print("test "..method.." with data send as "..kind)
		test_send(method, kind)
	end
end

print("Test successful!")
