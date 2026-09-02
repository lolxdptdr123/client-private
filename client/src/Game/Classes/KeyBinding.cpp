#include "pch.h"
#include "KeyBinding.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"

void KeyBinding::SetPressed(bool v, JNIEnv* env)
{
	if (this == NULL)
		return;

	const auto keybindingClass = (Klass*)env->GetObjectClass((jobject)this);
	const auto pressedField = keybindingClass->GetField(env, Mapper::Get("pressed").data(), "Z");

	if (keybindingClass)
		env->DeleteLocalRef((jclass)keybindingClass);

	pressedField->SetBooleanField(env, this, v);
}

bool KeyBinding::IsPressed(JNIEnv* env)
{
	if (this == NULL)
		return false;

	const auto keybindingClass = (Klass*)env->GetObjectClass((jobject)this);
	const auto pressedField = keybindingClass->GetField(env, Mapper::Get("pressed").data(), "Z");

	if (keybindingClass)
		env->DeleteLocalRef((jclass)keybindingClass);

	return pressedField->GetBooleanField(env, this);
}

int KeyBinding::GetKeyCode(JNIEnv* env)
{
	if (this == NULL || !env) return 0;
	const auto keybindingClass = (Klass*)env->GetObjectClass((jobject)this);
	if (!keybindingClass) return 0;
	const auto field = keybindingClass->GetField(env, Mapper::Get("keyCode").data(), "I");
	env->DeleteLocalRef((jclass)keybindingClass);
	if (!field) return 0;
	return field->GetIntField(env, this);
}

static int LwjglToVk(int lwjgl) {
	switch (lwjgl) {
	case 1: return VK_ESCAPE;
	case 2: return '1'; case 3: return '2'; case 4: return '3'; case 5: return '4';
	case 6: return '5'; case 7: return '6'; case 8: return '7'; case 9: return '8';
	case 10: return '9'; case 11: return '0';
	case 14: return VK_BACK; case 15: return VK_TAB; case 28: return VK_RETURN;
	case 29: case 157: return VK_CONTROL;
	case 42: case 54: return VK_SHIFT;
	case 56: case 184: return VK_MENU;
	case 57: return VK_SPACE; case 58: return VK_CAPITAL;
	case 16: return 'Q'; case 17: return 'W'; case 18: return 'E'; case 19: return 'R';
	case 20: return 'T'; case 21: return 'Y'; case 22: return 'U'; case 23: return 'I';
	case 24: return 'O'; case 25: return 'P';
	case 30: return 'A'; case 31: return 'S'; case 32: return 'D'; case 33: return 'F';
	case 34: return 'G'; case 35: return 'H'; case 36: return 'J'; case 37: return 'K';
	case 38: return 'L';
	case 44: return 'Z'; case 45: return 'X'; case 46: return 'C'; case 47: return 'V';
	case 48: return 'B'; case 49: return 'N'; case 50: return 'M';
	case 200: return VK_UP; case 208: return VK_DOWN;
	case 203: return VK_LEFT; case 205: return VK_RIGHT;
	default: return 0;
	}
}

bool KeyBinding::IsPhysDown(JNIEnv* env)
{
	if (this == NULL || !env) return false;
	int kc = GetKeyCode(env);
	if (kc <= 0) return false;

	int vk = LwjglToVk(kc);
	if (vk && (GetAsyncKeyState(vk) & 0x8000))
		return true;

	static jclass s_kbCls = nullptr;
	static jmethodID s_isKeyDown = nullptr;
	if (!s_kbCls) {
		jclass threadClass = env->FindClass("java/lang/Thread");
		if (!threadClass) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }
		jmethodID currentThread = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
		if (!currentThread) { env->ExceptionClear(); env->DeleteLocalRef(threadClass); return false; }
		jobject thread = env->CallStaticObjectMethod(threadClass, currentThread);
		if (!thread) { env->ExceptionClear(); env->DeleteLocalRef(threadClass); return false; }
		jmethodID getCtxCl = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
		env->DeleteLocalRef(threadClass);
		if (!getCtxCl) { env->ExceptionClear(); env->DeleteLocalRef(thread); return false; }
		jobject classLoader = env->CallObjectMethod(thread, getCtxCl);
		env->DeleteLocalRef(thread);
		if (!classLoader) { env->ExceptionClear(); return false; }

		jclass clClass = env->FindClass("java/lang/ClassLoader");
		if (!clClass) { env->ExceptionClear(); env->DeleteLocalRef(classLoader); return false; }
		jmethodID loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
		env->DeleteLocalRef(clClass);
		if (!loadClass) { env->ExceptionClear(); env->DeleteLocalRef(classLoader); return false; }

		jstring className = env->NewStringUTF("org.lwjgl.input.Keyboard");
		jobject cls = env->CallObjectMethod(classLoader, loadClass, className);
		env->DeleteLocalRef(className);
		env->DeleteLocalRef(classLoader);
		if (!cls || env->ExceptionCheck()) { env->ExceptionClear(); return false; }
		s_kbCls = (jclass)env->NewGlobalRef(cls);
		env->DeleteLocalRef(cls);
	}
	if (!s_isKeyDown) {
		s_isKeyDown = env->GetStaticMethodID(s_kbCls, "isKeyDown", "(I)Z");
		if (!s_isKeyDown) { env->ExceptionClear(); return false; }
	}
	jboolean result = env->CallStaticBooleanMethod(s_kbCls, s_isKeyDown, kc);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
	return result == JNI_TRUE;
}

void KeyBinding::SetPressTime(int v, JNIEnv* env)
{
	if (this == NULL) return;
	const auto keybindingClass = (Klass*)env->GetObjectClass((jobject)this);
	const auto field = keybindingClass->GetField(env, Mapper::Get("pressTime").data(), "I");
	if (keybindingClass) env->DeleteLocalRef((jclass)keybindingClass);
	if (!field) return;
	field->SetIntField(env, this, v);
}

int KeyBinding::GetPressTime(JNIEnv* env)
{
	if (this == NULL) return 0;
	const auto keybindingClass = (Klass*)env->GetObjectClass((jobject)this);
	const auto field = keybindingClass->GetField(env, Mapper::Get("pressTime").data(), "I");
	if (keybindingClass) env->DeleteLocalRef((jclass)keybindingClass);
	if (!field) return 0;
	return field->GetIntField(env, this);
}
