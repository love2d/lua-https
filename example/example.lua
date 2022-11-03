local https = require "https"

do
	local code, body = https.request("https://example.com")
	assert(code == 200, body)
end

do
	local code, body, headers = https.request("http://example.com", {method = "post", headers = {}, data = "cake"})
	assert(code == 200 and headers, body)

	for i, v in pairs(headers) do
		print(i, v)
	end
end
