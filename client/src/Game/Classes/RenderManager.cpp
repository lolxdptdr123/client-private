#include "pch.h"
#include "RenderManager.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include "../../Cheat/Modules/Settings.h"
#include <utility>

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include "../../Cheat/Modules/Settings.h"

jobject RenderManager::getEntityRender(jobject argEntity, JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto renderManagerClazz = (Klass*)env->GetObjectClass((jobject)this);
	const auto getEntityRenderMethod = renderManagerClazz->GetMethod(env, Mapper::Get("getEntityRenderObject").data(),
		std::string("(" + Mapper::Get("net/minecraft/entity/Entity", 2) + ")" + Mapper::Get("net/minecraft/client/renderer/entity/Render", 2)).data());

	if (renderManagerClazz)
		env->DeleteLocalRef((jclass)renderManagerClazz);

	return getEntityRenderMethod->CallObjectMethod(env, this, false, argEntity);
}

Vec3D RenderManager::GetRenderPos(JNIEnv* env)
{
	if (this == NULL || !env)
		return Vec3D();

	if (env->ExceptionCheck())
		env->ExceptionClear();

	const auto renderManagerClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderManager"));
	if (!renderManagerClazz)
		return Vec3D();

	const std::string nx = Mapper::Get("renderPosX");
	const std::string ny = Mapper::Get("renderPosY");
	const std::string nz = Mapper::Get("renderPosZ");

	auto tryRead = [&](bool asStatic) -> std::pair<bool, Vec3D> {
		const auto fx = renderManagerClazz->GetField(env, nx.c_str(), "D", asStatic);
		if (env->ExceptionCheck()) env->ExceptionClear();
		const auto fy = renderManagerClazz->GetField(env, ny.c_str(), "D", asStatic);
		if (env->ExceptionCheck()) env->ExceptionClear();
		const auto fz = renderManagerClazz->GetField(env, nz.c_str(), "D", asStatic);
		if (env->ExceptionCheck()) env->ExceptionClear();
		if (!fx || !fy || !fz)
			return { false, Vec3D() };
		void* owner = asStatic ? (void*)renderManagerClazz : (void*)this;
		return { true, Vec3D{
			fx->GetDoubleField(env, owner, asStatic),
			fy->GetDoubleField(env, owner, asStatic),
			fz->GetDoubleField(env, owner, asStatic)
		} };
	};

	auto st = tryRead(g_GameVersion != LUNAR_1_8_9);
	if (st.first)
		return st.second;
	return tryRead(g_GameVersion == LUNAR_1_8_9).second;
}

Vec3D RenderManager::GetViewerPos(JNIEnv* env)
{
	if (this == NULL || !env)
		return Vec3D();

	const auto renderManagerClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderManager"));
	const auto fieldX = renderManagerClazz->GetField(env, Mapper::Get("viewerPosX").data(), "D");
	const auto fieldY = renderManagerClazz->GetField(env, Mapper::Get("viewerPosY").data(), "D");
	const auto fieldZ = renderManagerClazz->GetField(env, Mapper::Get("viewerPosZ").data(), "D");

	return Vec3D(
		fieldX->GetDoubleField(env, (jobject)this),
		fieldY->GetDoubleField(env, (jobject)this),
		fieldZ->GetDoubleField(env, (jobject)this)
	);
}

bool RenderManager::DoRenderEntity(jobject entity, double x, double y, double z, float entityYaw, float partialTicks, bool p_147939_10_, JNIEnv* env)
{
	if (this == NULL || !env)
		return false;

	const auto renderManagerClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderManager"));
	const auto doRenderEntityMethod = renderManagerClazz->GetMethod(env, "doRenderEntity", "(Lnet/minecraft/entity/Entity;DDDFFZ)Z");
	return doRenderEntityMethod->CallBoolMethod(env, (jobject)this, false, entity, x, y, z, entityYaw, partialTicks, p_147939_10_);
}

bool RenderManager::RenderEntitySimple(jobject entity, float partialTicks, JNIEnv* env)
{
	if (this == NULL || !env || !entity)
		return false;
	const auto cls = g_Instance->FindClass(Mapper::Get("net/minecraft/client/renderer/entity/RenderManager"));
	if (!cls) return false;
	std::string sig = "(" + Mapper::Get("net/minecraft/entity/Entity", 2) + "F)Z";
	const auto m = cls->GetMethod(env, Mapper::Get("renderEntitySimple").c_str(), sig.c_str());
	if (!m) return false;
	bool r = m->CallBoolMethod(env, (jobject)this, false, entity, partialTicks);
	if (env->ExceptionCheck()) env->ExceptionClear();
	return r;
}