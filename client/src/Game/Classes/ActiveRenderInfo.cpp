#include "pch.h"
#include "ActiveRenderInfo.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include <cstring>

static std::vector<float> ReadFloatBuffer16(JNIEnv* env, const char* mappedField)
{
	if (!env)
		return {};
	if (env->ExceptionCheck())
		env->ExceptionClear();

	const auto activeRenderInfoClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/ActiveRenderInfo"));
	if (!activeRenderInfoClazz)
		return {};

	std::string name = Mapper::Get(mappedField);
	const char* tryNames[3] = { name.c_str(), nullptr, nullptr };
	if (strcmp(mappedField, "PROJECTION") == 0) {
		tryNames[1] = "PROJECTION";
		tryNames[2] = "projection";
	} else if (strcmp(mappedField, "MODELVIEW") == 0) {
		tryNames[1] = "MODELVIEW";
		tryNames[2] = "modelview";
	}

	Field* field = nullptr;
	for (int i = 0; i < 3; i++) {
		if (!tryNames[i] || !tryNames[i][0]) continue;
		field = activeRenderInfoClazz->GetField(env, tryNames[i], "Ljava/nio/FloatBuffer;", true);
		if (env->ExceptionCheck()) { env->ExceptionClear(); field = nullptr; }
		if (field) break;
	}
	if (!field)
		return {};

	const auto bufferObj = field->GetObjectField(env, activeRenderInfoClazz, true);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		return {};
	}
	if (!bufferObj)
		return {};

	jclass floatBufferClazz = env->FindClass("java/nio/FloatBuffer");
	if (!floatBufferClazz) {
		if (env->ExceptionCheck())
			env->ExceptionClear();
		env->DeleteLocalRef((jobject)bufferObj);
		return {};
	}

	jmethodID getMethod = env->GetMethodID(floatBufferClazz, "get", "(I)F");
	env->DeleteLocalRef(floatBufferClazz);
	if (!getMethod) {
		if (env->ExceptionCheck())
			env->ExceptionClear();
		env->DeleteLocalRef((jobject)bufferObj);
		return {};
	}

	std::vector<float> ret;
	ret.reserve(16);
	for (int i = 0; i < 16; i++) {
		ret.push_back(env->CallFloatMethod((jobject)bufferObj, getMethod, i));
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
			env->DeleteLocalRef((jobject)bufferObj);
			return {};
		}
	}

	env->DeleteLocalRef((jobject)bufferObj);
	return ret;
}

std::vector<float> ActiveRenderInfo::GetProjection(JNIEnv* env)
{
	return ReadFloatBuffer16(env, "PROJECTION");
}

std::vector<float> ActiveRenderInfo::GetModelView(JNIEnv* env)
{
	return ReadFloatBuffer16(env, "MODELVIEW");
}
