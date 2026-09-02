#include "pch.h"
#include "Klass.h"
#include <string>

const char* Klass::GetName(JNIEnv* env)
{
	if (this == NULL || env == NULL)
		return NULL;

	const auto cls = env->FindClass("java/lang/Class");
	if (!cls)
		return "";

	const auto mid_getName = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
	env->DeleteLocalRef(cls);
	if (!mid_getName)
		return "";

	const auto jname = (jstring)env->CallObjectMethod((jclass)this, mid_getName);
	if (!jname)
		return "";

	const char* utf = env->GetStringUTFChars(jname, nullptr);
	thread_local std::string cached;
	cached = utf ? utf : "";
	if (utf)
		env->ReleaseStringUTFChars(jname, utf);
	env->DeleteLocalRef(jname);
	return cached.c_str();
}

Field* Klass::GetField(JNIEnv* env, const char* name, const char* sig, bool staticField)
{
	if (this == NULL || env == NULL)
		return NULL;

	return staticField ? (Field*)env->GetStaticFieldID((jclass)this, name, sig) : (Field*)env->GetFieldID((jclass)this, name, sig);
}

Method* Klass::GetMethod(JNIEnv* env, const char* name, const char* sig, bool staticMethod)
{
	if (this == NULL || env == NULL)
		return NULL;

	return staticMethod ? (Method*)env->GetStaticMethodID((jclass)this, name, sig) : (Method*)env->GetMethodID((jclass)this, name, sig);
}