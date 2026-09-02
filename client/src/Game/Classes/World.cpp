#include "pch.h"
#include "World.h"
#include "Player.h"
#include "Minecraft.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"

struct WorldCache {
    Field* fPlayerEntities = nullptr;
    Field* fLoadedEntities = nullptr;
} static s_world;

static Field* FindListFieldOnClassTree(JNIEnv* env, jobject worldObj, const char* name)
{
    jclass cls = env->GetObjectClass(worldObj);
    while (cls) {
        jfieldID fid = env->GetFieldID(cls, name, "Ljava/util/List;");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            fid = nullptr;
        }
        if (fid) {
            env->DeleteLocalRef(cls);
            return (Field*)fid;
        }
        jclass super = env->GetSuperclass(cls);
        env->DeleteLocalRef(cls);
        cls = super;
    }
    return nullptr;
}

static void BuildWorldCache(JNIEnv* env, jobject worldObj)
{
    if (s_world.fPlayerEntities && s_world.fLoadedEntities)
        return;

    s_world.fPlayerEntities = FindListFieldOnClassTree(env, worldObj, Mapper::Get("playerEntities").c_str());
    s_world.fLoadedEntities = FindListFieldOnClassTree(env, worldObj, Mapper::Get("loadedEntityList").c_str());
}

static jobjectArray ListToArray(JNIEnv* env, jobject listObj)
{
    if (!listObj)
        return nullptr;

    jclass cls = env->GetObjectClass(listObj);
    if (!cls)
        return nullptr;

    jmethodID toArray = env->GetMethodID(cls, "toArray", "()[Ljava/lang/Object;");
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    if (!toArray)
        return nullptr;

    auto arr = (jobjectArray)env->CallObjectMethod(listObj, toArray);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return arr;
}

static jclass EntityPlayerClass(JNIEnv* env)
{
    static jclass cached = nullptr;
    if (cached)
        return cached;

    Klass* k = g_Instance->FindClass(Mapper::Get("net/minecraft/entity/player/EntityPlayer"));
    if (k) {
        cached = (jclass)env->NewGlobalRef((jclass)k);
        return cached;
    }

    jobject local = Minecraft::GetThePlayer(env);
    if (!local)
        return nullptr;

    jclass c = env->GetObjectClass(local);
    env->DeleteLocalRef(local);
    for (int i = 0; i < 2 && c; i++) {
        jclass super = env->GetSuperclass(c);
        env->DeleteLocalRef(c);
        c = super;
    }
    if (c) {
        cached = (jclass)env->NewGlobalRef(c);
        env->DeleteLocalRef(c);
    }
    return cached;
}

static std::vector<Player*> PlayersFromListField(JNIEnv* env, jobject worldObj, Field* field)
{
    std::vector<Player*> ret;
    if (!field)
        return ret;

    jobject listObj = field->GetObjectField(env, worldObj);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return ret;
    }
    if (!listObj)
        return ret;

    jobjectArray arr = ListToArray(env, listObj);
    env->DeleteLocalRef(listObj);
    if (!arr)
        return ret;

    jsize size = env->GetArrayLength(arr);
    ret.reserve(size);
    for (jsize i = 0; i < size; i++) {
        jobject elem = env->GetObjectArrayElement(arr, i);
        if (elem)
            ret.push_back((Player*)elem);
    }
    env->DeleteLocalRef(arr);
    return ret;
}

std::vector<Player*> World::GetPlayerEntities(JNIEnv* env)
{
    if (this == nullptr || !env)
        return {};

    if (env->ExceptionCheck())
        env->ExceptionClear();

    BuildWorldCache(env, (jobject)this);

    auto ret = PlayersFromListField(env, (jobject)this, s_world.fPlayerEntities);

    jclass playerCls = EntityPlayerClass(env);
    if (playerCls) {
        std::vector<Player*> filtered;
        filtered.reserve(ret.size());
        for (auto* p : ret) {
            jobject e = (jobject)p;
            if (!e)
                continue;
            if (env->IsInstanceOf(e, playerCls))
                filtered.push_back(p);
            else
                env->DeleteLocalRef(e);
        }
        ret.swap(filtered);
    }

    if (ret.size() <= 1 && s_world.fLoadedEntities) {
        jobject listObj = s_world.fLoadedEntities->GetObjectField(env, (jobject)this);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        else if (listObj) {
            jobjectArray arr = ListToArray(env, listObj);
            env->DeleteLocalRef(listObj);
            if (arr) {
                jsize size = env->GetArrayLength(arr);
                std::vector<Player*> extra;
                extra.reserve(size);
                for (jsize i = 0; i < size; i++) {
                    jobject elem = env->GetObjectArrayElement(arr, i);
                    if (!elem)
                        continue;
                    if (playerCls && !env->IsInstanceOf(elem, playerCls)) {
                        env->DeleteLocalRef(elem);
                        continue;
                    }
                    extra.push_back((Player*)elem);
                }
                env->DeleteLocalRef(arr);
                if (extra.size() > ret.size()) {
                    for (auto* p : ret)
                        env->DeleteLocalRef((jobject)p);
                    ret.swap(extra);
                } else {
                    for (auto* p : extra)
                        env->DeleteLocalRef((jobject)p);
                }
            }
        }
    }

    return ret;
}

std::vector<jobject> World::GetLoadedEntities(JNIEnv* env)
{
    if (this == nullptr || !env)
        return {};

    if (env->ExceptionCheck())
        env->ExceptionClear();

    BuildWorldCache(env, (jobject)this);
    if (!s_world.fLoadedEntities)
        return {};

    jobject listObj = s_world.fLoadedEntities->GetObjectField(env, (jobject)this);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return {};
    }
    if (!listObj)
        return {};

    jobjectArray arr = ListToArray(env, listObj);
    env->DeleteLocalRef(listObj);
    if (!arr)
        return {};

    jsize size = env->GetArrayLength(arr);
    std::vector<jobject> ret;
    ret.reserve(size);
    for (jsize i = 0; i < size; i++) {
        jobject elem = env->GetObjectArrayElement(arr, i);
        if (elem)
            ret.push_back(elem);
    }
    env->DeleteLocalRef(arr);
    return ret;
}

std::vector<jobject> World::GetLoadedTileEntities(JNIEnv* env)
{
    if (this == nullptr || !env)
        return {};

    if (env->ExceptionCheck())
        env->ExceptionClear();

    Field* f = FindListFieldOnClassTree(env, (jobject)this, Mapper::Get("loadedTileEntityList").c_str());
    if (!f)
        return {};

    jobject listObj = f->GetObjectField(env, (jobject)this);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return {};
    }
    if (!listObj)
        return {};

    jobjectArray arr = ListToArray(env, listObj);
    env->DeleteLocalRef(listObj);
    if (!arr)
        return {};

    jsize size = env->GetArrayLength(arr);
    std::vector<jobject> ret;
    ret.reserve(size);
    for (jsize i = 0; i < size; i++) {
        jobject elem = env->GetObjectArrayElement(arr, i);
        if (elem)
            ret.push_back(elem);
    }
    env->DeleteLocalRef(arr);
    return ret;
}
