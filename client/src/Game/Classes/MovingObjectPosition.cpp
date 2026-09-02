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

bool MovingObjectPosition::IsAimingBlock(JNIEnv* env)
{
    if (this == NULL || env == NULL) return false;
    std::string mopName = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    std::string mopTypeSig = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType", 2);
    std::string typeOfHit = Mapper::Get("typeOfHit");
    std::string blockName = Mapper::Get("BLOCK");
    return IsAimingType(env, (jobject)this,
        mopName.c_str(), mopTypeSig.c_str(),
        typeOfHit.c_str(), blockName.c_str());
}

bool MovingObjectPosition::IsAimingEntity(JNIEnv* env)
{
    if (this == NULL || env == NULL) return false;
    std::string mopName = Mapper::Get("net/minecraft/util/MovingObjectPosition");
    std::string mopTypeSig = Mapper::Get("net/minecraft/util/MovingObjectPosition$MovingObjectType", 2);
    std::string typeOfHit = Mapper::Get("typeOfHit");
    std::string entityName = Mapper::Get("ENTITY");
    return IsAimingType(env, (jobject)this,
        mopName.c_str(), mopTypeSig.c_str(),
        typeOfHit.c_str(), entityName.c_str());
}