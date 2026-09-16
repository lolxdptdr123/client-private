#include "pch.h"
#include "Klass.h"
#include "Mapper.h"
#include <string>

const char* Klass::GetName(JNIEnv* env)
{
	if (this == NULL || env == NULL)
		return NULL;

	if (env->ExceptionCheck()) env->ExceptionClear();

	static jmethodID s_getName = nullptr;
	if (!s_getName) {
		jclass cls = env->FindClass("java/lang/Class");
		if (!cls) return "";
		s_getName = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
		env->DeleteLocalRef(cls);
		if (!s_getName) return "";
	}

	const auto jname = (jstring)env->CallObjectMethod((jclass)this, s_getName);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return ""; }
	if (!jname) return "";

	const char* utf = env->GetStringUTFChars(jname, nullptr);
	thread_local std::string cached;
	cached = utf ? utf : "";
	if (utf) env->ReleaseStringUTFChars(jname, utf);
	env->DeleteLocalRef(jname);
	return cached.c_str();
}

Field* Klass::GetField(JNIEnv* env, const char* name, const char* sig, bool staticField)
{
	if (this == NULL || env == NULL)
		return NULL;

	std::string rs = Mapper::RemapSignature(sig);
	const char* s = rs.c_str();
	return staticField
		? (Field*)env->GetStaticFieldID((jclass)this, name, s)
		: (Field*)env->GetFieldID((jclass)this, name, s);
}

Method* Klass::GetMethod(JNIEnv* env, const char* name, const char* sig, bool staticMethod)
{
	if (this == NULL || env == NULL)
		return NULL;

	std::string rs = Mapper::RemapSignature(sig);
	const char* s = rs.c_str();
	return staticMethod
		? (Method*)env->GetStaticMethodID((jclass)this, name, s)
		: (Method*)env->GetMethodID((jclass)this, name, s);
}
