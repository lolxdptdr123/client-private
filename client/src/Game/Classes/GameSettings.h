#pragma once

class GameSettings
{
public:
    jobject GetKeyBindSneak(JNIEnv* env);
    jobject GetKeyBindJump(JNIEnv* env);
    jobject GetKeyBindInventory(JNIEnv* env);
    jobject GetKeyBindForward(JNIEnv* env);
    jobject GetKeyBindBack(JNIEnv* env);
    jobject GetKeyBindLeft(JNIEnv* env);
    jobject GetKeyBindRight(JNIEnv* env);
    jobject GetKeyBindSprint(JNIEnv* env);
    jobject GetKeyBindUseItem(JNIEnv* env);
    jobject GetKeyBindAttack(JNIEnv* env);
    float GetMouseSensitivity(JNIEnv* env);
    int GetGuiScale(JNIEnv* env);
};