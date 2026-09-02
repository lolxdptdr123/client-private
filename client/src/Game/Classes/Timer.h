#pragma once

class Timer
{
public:
	float GetRenderPartialTicks(JNIEnv* env);
	float GetTimerSpeed(JNIEnv* env);
	void SetTimerSpeed(float v, JNIEnv* env);
	float GetTicksPerSecond(JNIEnv* env);
	void SetTicksPerSecond(float v, JNIEnv* env);
};