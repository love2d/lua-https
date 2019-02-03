#pragma once

#include "common/HTTPSClient.h"

class NSURLClient : public HTTPSClient
{
public:
	virtual bool valid() const override;
	virtual HTTPSClient::Reply request(const HTTPSClient::Request &req) override;
};
