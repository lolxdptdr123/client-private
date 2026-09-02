#pragma once

class KeyBinding
{
public:
	void SetPressed(bool v, JNIEnv* env);
	bool IsPressed(JNIEnv* env);
	int GetKeyCode(JNIEnv* env);
	bool IsPhysDown(JNIEnv* env);
	void SetPressTime(int v, JNIEnv* env);
	int GetPressTime(JNIEnv* env);
};