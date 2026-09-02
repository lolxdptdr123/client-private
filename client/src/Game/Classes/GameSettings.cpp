#include "pch.h"
#include "GameSettings.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"

jobject GameSettings::GetKeyBindSneak(JNIEnv* env)
{
    if (this == NULL) return NULL;

    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)this);
    const auto keyBindSneakField = gameSettingsClass->GetField(env,
        Mapper::Get("keyBindSneak").data(),
        Mapper::Get("net/minecraft/client/settings/KeyBinding", 2).data());

    if (gameSettingsClass) env->DeleteLocalRef((jclass)gameSettingsClass);
    return keyBindSneakField->GetObjectField(env, (jobject)this);
}

jobject GameSettings::GetKeyBindJump(JNIEnv* env)
{
    if (this == NULL) return NULL;

    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)this);
    const auto keyBindJumpField = gameSettingsClass->GetField(env,
        Mapper::Get("keyBindJump").data(),
        Mapper::Get("net/minecraft/client/settings/KeyBinding", 2).data());

    if (gameSettingsClass) env->DeleteLocalRef((jclass)gameSettingsClass);
    return keyBindJumpField->GetObjectField(env, (jobject)this);
}

float GameSettings::GetMouseSensitivity(JNIEnv* env)
{
    if (this == NULL || !env) return 0.5f;

    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)this);
    if (!gameSettingsClass) return 0.5f;

    const auto sensField = gameSettingsClass->GetField(env,
        Mapper::Get("mouseSensitivity").data(), "F");
    if (gameSettingsClass) env->DeleteLocalRef((jclass)gameSettingsClass);
    if (!sensField) return 0.5f;
    return sensField->GetFloatField(env, (jobject)this);
}

jobject GameSettings::GetKeyBindInventory(JNIEnv* env)
{
    if (this == NULL) return NULL;
    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)this);
    const auto field = gameSettingsClass->GetField(env,
        Mapper::Get("keyBindInventory").data(),
        Mapper::Get("net/minecraft/client/settings/KeyBinding", 2).data());
    if (gameSettingsClass) env->DeleteLocalRef((jclass)gameSettingsClass);
    if (!field) return NULL;
    return field->GetObjectField(env, (jobject)this);
}

jobject GameSettings::GetKeyBindForward(JNIEnv* env)
{
    if (this == NULL) return NULL;
    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)this);
    const auto field = gameSettingsClass->GetField(env,
        Mapper::Get("keyBindForward").data(),
        Mapper::Get("net/minecraft/client/settings/KeyBinding", 2).data());
    if (gameSettingsClass) env->DeleteLocalRef((jclass)gameSettingsClass);
    if (!field) return NULL;
    return field->GetObjectField(env, (jobject)this);
}

static jobject GetKeyBindByName(GameSettings* self, JNIEnv* env, const char* mapKey)
{
    if (self == NULL || !env) return NULL;
    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)self);
    if (!gameSettingsClass) return NULL;
    const auto field = gameSettingsClass->GetField(env,
        Mapper::Get(mapKey).data(),
        Mapper::Get("net/minecraft/client/settings/KeyBinding", 2).data());
    env->DeleteLocalRef((jclass)gameSettingsClass);
    if (!field) return NULL;
    return field->GetObjectField(env, (jobject)self);
}

jobject GameSettings::GetKeyBindBack(JNIEnv* env) { return GetKeyBindByName(this, env, "keyBindBack"); }
jobject GameSettings::GetKeyBindLeft(JNIEnv* env) { return GetKeyBindByName(this, env, "keyBindLeft"); }
jobject GameSettings::GetKeyBindRight(JNIEnv* env) { return GetKeyBindByName(this, env, "keyBindRight"); }
jobject GameSettings::GetKeyBindSprint(JNIEnv* env) { return GetKeyBindByName(this, env, "keyBindSprint"); }

jobject GameSettings::GetKeyBindUseItem(JNIEnv* env)
{
    return GetKeyBindByName(this, env, "keyBindUseItem");
}

jobject GameSettings::GetKeyBindAttack(JNIEnv* env)
{
    return GetKeyBindByName(this, env, "keyBindAttack");
}

int GameSettings::GetGuiScale(JNIEnv* env)
{
    if (this == NULL || !env) return 0;
    const auto gameSettingsClass = (Klass*)env->GetObjectClass((jobject)this);
    if (!gameSettingsClass) return 0;
    const auto field = gameSettingsClass->GetField(env, Mapper::Get("guiScale").data(), "I");
    if (gameSettingsClass) env->DeleteLocalRef((jclass)gameSettingsClass);
    if (!field) return 0;
    return field->GetIntField(env, (jobject)this);
}