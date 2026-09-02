#include "pch.h"
#include "MovingObjectPosition.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"

// __try est incompatible avec les objets C++ (C2712), meme dans un helper.
// Les null checks explicites a chaque etape suffisent pour la securite.

static bool IsAimingType(JNIEnv* env, jobject mop,
    const char* mopName, const char* mopTypeSig,
    const char* typeOfHitName, const char* enumFieldName)
{
    jclass mopClass = (jclass)g_Instance->FindClass(mopName);
    if (!mopClass) return false;

    jfieldID fTypeOfHit = env->GetFieldID(mopClass, typeOfHitName, mopTypeSig);
    if (!fTypeOfHit) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }

    jobject typeOfHitObj = env->GetObjectField(mop, fTypeOfHit);
    if (!typeOfHitObj) return false;

    jclass enumClass = env->GetObjectClass(typeOfHitObj);
    if (!enumClass) return false;

    jfieldID fEnum = env->GetStaticFieldID(enumClass, enumFieldName, mopTypeSig);
    if (!fEnum) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }

    jobject enumVal = env->GetStaticObjectField(enumClass, fEnum);
    if (!enumVal) return false;

    return env->IsSameObject(typeOfHitObj, enumVal);
}

static int TypeOfHitInt(JNIEnv* env, jobject mop)
{
    jclass mopClass = env->GetObjectClass(mop);
    if (!mopClass) return -1;
    std::string typeOfHit = Mapper::Get("typeOfHit");
    jfieldID f = env->GetFieldID(mopClass, typeOfHit.empty() ? "typeOfHit" : typeOfHit.c_str(), "I");
    env->DeleteLocalRef(mopClass);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
    if (!f) return -1;
    return env->GetIntField(mop, f);
}

bool MovingObjectPosition::IsAimingBlock(JNIEnv* env)
{
    if (this == NULL || env == NULL) return false;
    std::string mopName = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    std::string mopTypeSig = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType", 2);
    std::string typeOfHit = Mapper::Get("typeOfHit");
    std::string blockName = Mapper::Get("BLOCK");
    if (IsAimingType(env, (jobject)this,
        mopName.c_str(), mopTypeSig.c_str(),
        typeOfHit.c_str(), blockName.c_str()))
        return true;
    return TypeOfHitInt(env, (jobject)this) == 1;
}

bool MovingObjectPosition::IsAimingEntity(JNIEnv* env)
{
    if (this == NULL || env == NULL) return false;
    std::string mopName = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    std::string mopTypeSig = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType", 2);
    std::string typeOfHit = Mapper::Get("typeOfHit");
    std::string entityName = Mapper::Get("ENTITY");
    if (IsAimingType(env, (jobject)this,
        mopName.c_str(), mopTypeSig.c_str(),
        typeOfHit.c_str(), entityName.c_str()))
        return true;
    return TypeOfHitInt(env, (jobject)this) == 2;
}