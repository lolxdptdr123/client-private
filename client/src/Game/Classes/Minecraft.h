#pragma once

class Minecraft
{
public:
	static jobject GetTheMinecraft(JNIEnv* env);
	static jobject GetThePlayer(JNIEnv* env);
	static jobject GetTheWorld(JNIEnv* env);
	static jobject GetCurrentScreen(JNIEnv* env);
	static jobject GetRenderManager(JNIEnv* env);
	static jobject GetTimer(JNIEnv* env);
	static jobject GetFontRenderer(JNIEnv* env);
	static jobject GetObjectMouseOver(JNIEnv* env);
	static jobject GetPointedEntity(JNIEnv* env);
	static void SetObjectMouseOver(JNIEnv* env, jobject mop);
	static void SetPointedEntity(JNIEnv* env, jobject entity);
	static jobject GetGameSettings(JNIEnv* env);
	static jobject GetPlayerController(JNIEnv* env);
	static int GetDisplayHeight(JNIEnv* env);
	static int GetDisplayWidth(JNIEnv* env);
	static bool IsFullscreen(JNIEnv* env);
	static jobject WindowClick(JNIEnv* env, int windowId, int slot, int mouseButton, int clickMode, jobject player);
	static bool OpenPlayerInventory(JNIEnv* env);
	static void ClosePlayerInventory(JNIEnv* env);
	static bool IsPlayerInventoryScreen(JNIEnv* env);
	static void PressInventoryKey(JNIEnv* env);
	static void RightClickMouse(JNIEnv* env);
	static int GetRightClickDelayTimer(JNIEnv* env);
	static void SetRightClickDelayTimer(JNIEnv* env, int ticks);
};