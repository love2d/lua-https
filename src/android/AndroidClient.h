#pragma once

#include "common/config.h"

#ifdef USE_ANDROID_BACKEND

#include <jni.h>

#include "common/HTTPSClient.h"

class AndroidClient: public HTTPSClient
{
public:
	AndroidClient();
	
	bool valid() const override;
	HTTPSClient::Reply request(const HTTPSClient::Request &req) override;

private:
	JNIEnv *(*SDL_AndroidGetJNIEnv)();
};

#endif
