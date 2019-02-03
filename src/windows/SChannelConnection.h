#pragma once

#include "common/Connection.h"
#include "common/PlaintextConnection.h"

#include <vector>

class SChannelConnection : public Connection
{
public:
	SChannelConnection();
	virtual bool connect(const std::string &hostname, uint16_t port) override;
	virtual size_t read(char *buffer, size_t size) override;
	virtual size_t write(const char *buffer, size_t size) override;
	virtual void close() override;
	virtual ~SChannelConnection();

	static bool valid();

private:
	PlaintextConnection socket;
	void *context; // FIXME
	std::vector<char> encRecvBuffer;
	std::vector<char> decRecvBuffer;

	size_t decrypt(char *buffer, size_t size, bool recurse = true);
};
