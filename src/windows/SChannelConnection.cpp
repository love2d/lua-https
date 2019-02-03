#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <schnlsp.h>
#include <assert.h>

#include "SChannelConnection.h"

#ifndef SCH_USE_STRONG_CRYPTO
#	define SCH_USE_STRONG_CRYPTO 0x00400000
#endif
#ifndef SP_PROT_TLS1_1_CLIENT
#	define SP_PROT_TLS1_1_CLIENT 0x00000200
#endif
#ifndef SP_PROT_TLS1_2_CLIENT
#	define SP_PROT_TLS1_2_CLIENT 0x00000800
#endif

#ifdef DEBUG_SCHANNEL
#include <iostream>
std::ostream &debug = std::cout;
#else
struct Debug
{
	template<typename T>
	Debug &operator<<(const T&) { return *this; }
} debug;
#endif

static void enqueue(std::vector<char> &buffer, char *data, size_t size)
{
	size_t oldSize = buffer.size();
	buffer.resize(oldSize + size);
	memcpy(&buffer[oldSize], data, size);
}

static void enqueue_prepend(std::vector<char> &buffer, char *data, size_t size)
{
	size_t oldSize = buffer.size();
	buffer.resize(oldSize + size);
	memmove(&buffer[size], &buffer[0], oldSize);
	memcpy(&buffer[0], data, size);
}

static size_t dequeue(std::vector<char> &buffer, char *data, size_t size)
{
	size = std::min(size, buffer.size());
	size_t remaining = buffer.size() - size;

	memcpy(data, &buffer[0], size);
	memmove(&buffer[0], &buffer[size], remaining);
	buffer.resize(remaining);

	return size;
}

SChannelConnection::SChannelConnection()
	: context(nullptr)
{
}

SChannelConnection::~SChannelConnection()
{
	// TODO?
	if (CtxtHandle *context = static_cast<CtxtHandle*>(this->context))
	{
		DeleteSecurityContext(context);
		delete context;
	}
}

bool SChannelConnection::connect(const std::string &hostname, uint16_t port)
{
	debug << "Trying to connect to " << hostname << ":" << port << "\n";
	if (!socket.connect(hostname, port))
		return false;
	debug << "Connected\n";

	SCHANNEL_CRED cred;
	memset(&cred, 0, sizeof(cred));

	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;
	cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO | SCH_CRED_REVOCATION_CHECK_CHAIN;

	CredHandle credHandle;
	if (AcquireCredentialsHandle(nullptr, (char*) UNISP_NAME, SECPKG_CRED_OUTBOUND, nullptr, &cred, nullptr, nullptr, &credHandle, nullptr) != SEC_E_OK)
	{
		debug << "Failed to acquire handle\n";
		socket.close();
		return false;
	}
	debug << "Acquired handle\n";

	CtxtHandle *context = new CtxtHandle;
	CtxtHandle *inHandle = nullptr, *outHandle = context;

	SecBufferDesc inputBuffer, outputBuffer;
	inputBuffer.ulVersion = outputBuffer.ulVersion = SECBUFFER_VERSION;
	inputBuffer.cBuffers = outputBuffer.cBuffers = 0;
	inputBuffer.pBuffers = outputBuffer.pBuffers = nullptr;

	ULONG contextAttr;

	static constexpr size_t bufferSize = 8192;
	bool done = false, success = false, contextCreated = false;
	char *recvBuffer = nullptr;
	char *sendBuffer = new char[2*bufferSize];

	SecBuffer recvSecBuffer, sendSecBuffer;
	recvSecBuffer.BufferType = sendSecBuffer.BufferType = SECBUFFER_TOKEN;
	sendSecBuffer.cbBuffer = bufferSize;
	sendSecBuffer.pvBuffer = sendBuffer;

	outputBuffer.cBuffers = 1;
	outputBuffer.pBuffers = &sendSecBuffer;

	do
	{
		bool recvData = false;
		auto ret = InitializeSecurityContext(&credHandle, inHandle, (char*) hostname.c_str(), ISC_REQ_STREAM, 0, 0, &inputBuffer, 0, outHandle, &outputBuffer, &contextAttr, nullptr);
		switch (ret)
		{
		case SEC_I_COMPLETE_NEEDED:
		case SEC_I_COMPLETE_AND_CONTINUE:
			if (CompleteAuthToken(outHandle, &outputBuffer) != SEC_E_OK)
				done = true;
			else if (ret == SEC_I_COMPLETE_NEEDED)
				success = done = true;
			break;
		case SEC_I_CONTINUE_NEEDED:
			recvData = true;
			break;
		case SEC_E_INCOMPLETE_CREDENTIALS:
			done = true;
			break;
		case SEC_E_INCOMPLETE_MESSAGE:
			recvData = true;
			break;
		case SEC_E_OK:
			success = done = true;
			break;
		default:
			done = true;
			// TODO: error
			break;
		}

		if (!done)
			contextCreated = true;

		inHandle = context;
		outHandle = nullptr;

		debug << "Initialize done, with " << outputBuffer.cBuffers << " output buffers and status " << ret << "\n";
		for (unsigned int i = 0; i < outputBuffer.cBuffers && !success; ++i)
		{
			auto &buffer = outputBuffer.pBuffers[i];
			debug << "\tBuffer of size: " << buffer.cbBuffer << "\n";
			if (buffer.cbBuffer > 0 && buffer.BufferType == SECBUFFER_TOKEN)
			{
				socket.write((const char*) buffer.pvBuffer, buffer.cbBuffer);
			}
			else
				debug << "Got buffer with type " << buffer.BufferType << "\n";

			if (buffer.pvBuffer == sendBuffer)
			{
				memset(sendBuffer, 0, bufferSize);
				buffer.cbBuffer = bufferSize;
			}
			//FreeContextBuffer(&buffer);
		}

		if (recvData)
		{
			debug << "Receiving data\n";
			if (!recvBuffer)
				recvBuffer = new char[bufferSize];

			recvSecBuffer.cbBuffer = socket.read(recvBuffer, bufferSize);
			recvSecBuffer.pvBuffer = recvBuffer;

			inputBuffer.cBuffers = 1;
			inputBuffer.pBuffers = &recvSecBuffer;
		}
		else
		{
			inputBuffer.cBuffers = 0;
			inputBuffer.pBuffers = nullptr;
		}

		// TODO: A bunch of frees?
	} while (!done);

	delete[] sendBuffer;
	delete[] recvBuffer;

	debug << "Done!\n";
	// TODO: Check resulting context attributes
	if (success)
	{
		this->context = static_cast<void*>(context);
	}
	else if (contextCreated)
	{
		DeleteSecurityContext(context);
		delete context;
	}

	return success;
}

size_t SChannelConnection::read(char *buffer, size_t size)
{
	if (decRecvBuffer.size() > 0)
	{
		size = dequeue(decRecvBuffer, buffer, size);
		debug << "Read " << size << " bytes of previously decoded data\n";
		return size;
	}
	else if (encRecvBuffer.size() > 0)
	{
		size = dequeue(encRecvBuffer, buffer, size);
		debug << "Read " << size << " bytes of extra data\n";
	}
	else
	{
		size = socket.read(buffer, size);
		debug << "Received " << size << " bytes of data\n";
	}

	return decrypt(buffer, size);
}

size_t SChannelConnection::decrypt(char *buffer, size_t size, bool recurse)
{
	SecBuffer secBuffers[4];
	secBuffers[0].cbBuffer = size;
	secBuffers[0].BufferType = SECBUFFER_DATA;
	secBuffers[0].pvBuffer = buffer;

	for (size_t i = 1; i < 4; ++i)
		secBuffers[i].BufferType = SECBUFFER_EMPTY;

	SecBufferDesc secBufferDesc;
	secBufferDesc.ulVersion = SECBUFFER_VERSION;
	secBufferDesc.cBuffers = 4;
	secBufferDesc.pBuffers = &secBuffers[0];

	auto ret = DecryptMessage(static_cast<CtxtHandle*>(context), &secBufferDesc, 0, nullptr); // FIXME
	debug << "DecryptMessage returns: " << ret << "\n";
	switch (ret)
	{
	case SEC_E_OK:
	{
		void *actualDataStart = buffer;
		for (size_t i = 0; i < 4; ++i)
		{
			auto &buffer = secBuffers[i];
			if (buffer.BufferType == SECBUFFER_DATA)
			{
				actualDataStart = buffer.pvBuffer;
				size = buffer.cbBuffer;
			}
			else if (buffer.BufferType == SECBUFFER_EXTRA)
			{
				debug << "\tExtra data in buffer " << i << " (" << buffer.cbBuffer << " bytes)\n";
				enqueue(encRecvBuffer, static_cast<char*>(buffer.pvBuffer), buffer.cbBuffer);
			}
			else if (buffer.BufferType != SECBUFFER_EMPTY)
				debug << "\tBuffer of type " << buffer.BufferType << "\n";
		}

		if (actualDataStart)
			memmove(buffer, actualDataStart, size);

		break;
	}
	case SEC_E_INCOMPLETE_MESSAGE:
	{
		// Move all our current data to encRecvBuffer
		enqueue(encRecvBuffer, buffer, size);

		// Now try to read some more data from the socket
		size_t bufferSize = encRecvBuffer.size() + 8192;
		char *recvBuffer = new char[bufferSize];
		size_t recvd = socket.read(recvBuffer+encRecvBuffer.size(), 8192);
		debug << recvd << " bytes of extra data read from socket\n";

		if (recvd == 0 && !recurse)
		{
			debug << "Recursion prevented, bailing\n";
			return 0;
		}

		// Fill our buffer with the queued data and the newly received data
		size_t totalSize = encRecvBuffer.size() + recvd;
		dequeue(encRecvBuffer, recvBuffer, encRecvBuffer.size());
		debug << "Trying to decrypt with " << totalSize << " bytes of data\n";

		// Now try to decrypt that
		size_t decrypted = decrypt(recvBuffer, totalSize, false);
		debug << "\tObtained " << decrypted << " bytes of decrypted data\n";

		// Copy the first size bytes to the output buffer
		size = std::min(size, decrypted);
		memcpy(buffer, recvBuffer, size);

		// And write the remainder to our queued decrypted data...
		// Note: we prepend, since our recursive call may already have written
		// something and we can be sure decrypt wasn't called if the buffer was
		// non-empty in read
		enqueue_prepend(decRecvBuffer, recvBuffer+size, decrypted-size);
		debug << "\tStoring " << decrypted-size << " bytes of extra decrypted data\n";
		return size;
	}
	// TODO: More?
	default:
		size = 0;
		break;
	}

	debug << "\tDecrypted " << size << " bytes of data\n";

	return size;
}

size_t SChannelConnection::write(const char *buffer, size_t size)
{
	static constexpr size_t bufferSize = 8192;
	assert(size <= bufferSize);

	SecPkgContext_StreamSizes Sizes;
	QueryContextAttributes(
            static_cast<CtxtHandle*>(context),
            SECPKG_ATTR_STREAM_SIZES,
            &Sizes);
	debug << "stream sizes:\n\theader: " << Sizes.cbHeader << "\n\tfooter: " << Sizes.cbTrailer << "\n";

	char *sendBuffer = new char[bufferSize + Sizes.cbHeader + Sizes.cbTrailer];
	memcpy(sendBuffer+Sizes.cbHeader, buffer, size);

	SecBuffer secBuffers[4];
	secBuffers[0].cbBuffer = Sizes.cbHeader;
	secBuffers[0].BufferType = SECBUFFER_STREAM_HEADER;
	secBuffers[0].pvBuffer = sendBuffer;

	secBuffers[1].cbBuffer = size;
	secBuffers[1].BufferType = SECBUFFER_DATA;
	secBuffers[1].pvBuffer = sendBuffer+Sizes.cbHeader;

	secBuffers[2].cbBuffer = Sizes.cbTrailer;
	secBuffers[2].pvBuffer = sendBuffer+Sizes.cbHeader+size;
	secBuffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

	secBuffers[3].cbBuffer = 0;
	secBuffers[3].BufferType = SECBUFFER_EMPTY;
	secBuffers[3].pvBuffer = nullptr;

	SecBufferDesc secBufferDesc;
	secBufferDesc.ulVersion = SECBUFFER_VERSION;
	secBufferDesc.cBuffers = 4;
	secBufferDesc.pBuffers = secBuffers;

	auto ret = EncryptMessage(static_cast<CtxtHandle*>(context), 0, &secBufferDesc, 0); // FIXME
	debug << "Send:\n\tHeader size: " << secBuffers[0].cbBuffer << "\n\t\ttype: " << secBuffers[0].BufferType << "\n\tData size: " << secBuffers[1].cbBuffer << "\n\t\ttype: " << secBuffers[1].BufferType << "\n\tFooter size: " << secBuffers[2].cbBuffer << "\n\t\ttype: " << secBuffers[2].BufferType << "\n";

	size_t sendSize = 0;
	for (size_t i = 0; i < 4; ++i)
		if (secBuffers[i].cbBuffer != bufferSize)
			sendSize += secBuffers[i].cbBuffer;

	debug << "\tReal length? " << sendSize << "\n";
	switch (ret)
	{
	case SEC_E_OK:
		socket.write(sendBuffer, sendSize);
		break;
	// TODO: More?
	default:
		size = 0;
		break;
	}

	delete[] sendBuffer;
	return size;
}

void SChannelConnection::close()
{
	// TODO
}

bool SChannelConnection::valid()
{
	return true;
}
