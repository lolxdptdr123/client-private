#define _CRT_SECURE_NO_WARNINGS
#include "pch.h"
#include "BlockPos.h"
#include "Minecraft.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include "../../Cheat/Modules/Settings.h"

jobject BlockPos::GetBlock(Vec3D pos, jobject worldObject, JNIEnv* env)
{
	char* type = new char[256];
	sprintf(type, "(III)%s", Mapper::Get("net/minecraft/block/Block", 2).data());

	const auto worldClazz = (Klass*)env->GetObjectClass(worldObject);
	const auto getBlockMethod = worldClazz->GetMethod(env, Mapper::Get("getBlock").data(), type);
	delete[] type;

	if (worldClazz)
		env->DeleteLocalRef((jclass)worldClazz);
	return getBlockMethod->CallObjectMethod(env, (jobject)worldObject, false, (int)pos.x, (int)pos.y, (int)pos.z);
}