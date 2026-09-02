#include "pch.h"
#include "ItemStack.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"

jobject ItemStack::GetItem(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto itemStackClass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemStack"));
	if (!itemStackClass)
		return NULL;
	const auto getItemMethod = itemStackClass->GetMethod(env, Mapper::Get("getItem").data(), Mapper::Get("net/minecraft/item/Item", 3).data());
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (getItemMethod) {
		jobject item = getItemMethod->CallObjectMethod(env, (jobject)this);
		if (env->ExceptionCheck()) { env->ExceptionClear(); item = nullptr; }
		if (item) return item;
	}

	std::string itemSig = Mapper::Get("net/minecraft/item/Item", 2);
	std::string mappedField = Mapper::Get("item");
	const char* fields[] = { mappedField.c_str(), "theItem", "item" };
	for (const char* n : fields) {
		if (!n || !n[0] || itemSig.empty()) continue;
		Field* f = itemStackClass->GetField(env, n, itemSig.c_str());
		if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
		if (!f) continue;
		jobject item = f->GetObjectField(env, (jobject)this, false);
		if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
		if (item) return item;
	}
	return NULL;
}

bool ItemStack::IsWeapon(JNIEnv* env)
{
	if (this == NULL)
		return NULL;

	const auto heldItem = this->GetItem(env);

	const auto axeKlass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemAxe"));
	const auto swordKlass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemSword"));
	if (!axeKlass || !swordKlass) {
		return NULL;
	}

	return env->IsInstanceOf(heldItem, (jclass)axeKlass)
		|| env->IsInstanceOf(heldItem, (jclass)swordKlass);
}

bool ItemStack::IsBlock(JNIEnv* env)
{
	if (this == NULL)
		return NULL;

	const auto heldItem = this->GetItem(env);
	const auto klass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemBlock"));
	if (!klass)
		return NULL;

	return env->IsInstanceOf(heldItem, (jclass)klass);
}

bool ItemStack::IsEnderPearl(JNIEnv* env)
{
	if (this == NULL)
		return NULL;

	const auto heldItem = this->GetItem(env);
	const auto klass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemEnderPearl"));
	if (!klass)
		return NULL;

	return env->IsInstanceOf(heldItem, (jclass)klass);
}

bool ItemStack::IsSoup(JNIEnv* env)
{
	if (this == NULL) return false;
	const auto heldItem = this->GetItem(env);
	if (!heldItem) return false;
	const auto klass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemSoup"));
	if (!klass) return false;
	return env->IsInstanceOf(heldItem, (jclass)klass);
}

bool ItemStack::IsRod(JNIEnv* env)
{
	if (this == NULL) return false;
	const auto heldItem = this->GetItem(env);
	if (!heldItem) return false;
	const auto klass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemFishingRod"));
	if (klass && env->IsInstanceOf(heldItem, (jclass)klass)) return true;
	return this->GetItemId(env) == 346;
}

bool ItemStack::Is(const char* clazz, JNIEnv* env)
{
	if (this == NULL)
		return NULL;

	const auto heldItem = this->GetItem(env);
	const auto klass = g_Instance->FindClass(clazz);
	if (!klass)
		return NULL;

	return env->IsInstanceOf(heldItem, (jclass)klass);
}

int ItemStack::GetMetadata(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0;

	static jfieldID s_field = nullptr;
	static jmethodID s_method = nullptr;
	static int s_mode = 0;
	if (s_mode == 0) {
		s_mode = 3;
		const auto itemStackClass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemStack"));
		if (!itemStackClass)
			return 0;
		s_field = env->GetFieldID((jclass)itemStackClass, Mapper::Get("metadata").c_str(), "I");
		if (env->ExceptionCheck()) { env->ExceptionClear(); s_field = nullptr; }
		if (s_field) {
			s_mode = 1;
		} else {
			s_method = env->GetMethodID((jclass)itemStackClass, Mapper::Get("getItemDamage").c_str(), "()I");
			if (env->ExceptionCheck()) { env->ExceptionClear(); s_method = nullptr; }
			if (s_method) s_mode = 2;
		}
	}
	if (s_mode == 1) {
		int v = env->GetIntField((jobject)this, s_field);
		if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
		return v;
	}
	if (s_mode == 2) {
		int v = env->CallIntMethod((jobject)this, s_method);
		if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
		return v;
	}
	return 0;
}

std::string ItemStack::GetDisplayName(JNIEnv* env)
{
	if (this == NULL || !env)
		return "";

	const auto itemStackClass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemStack"));
	if (!itemStackClass) return "";
	const auto m = itemStackClass->GetMethod(env, Mapper::Get("itemStackDisplayName").data(), "()Ljava/lang/String;");
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (!m) return "";
	jstring js = (jstring)m->CallObjectMethod(env, (jobject)this);
	if (env->ExceptionCheck()) { env->ExceptionClear(); if (js) env->DeleteLocalRef(js); return ""; }
	if (!js) return "";
	const char* c = env->GetStringUTFChars(js, nullptr);
	std::string out = c ? c : "";
	if (c) env->ReleaseStringUTFChars(js, c);
	env->DeleteLocalRef(js);
	return out;
}

int ItemStack::GetItemId(JNIEnv* env)
{
	if (this == NULL || !env) return -1;
	jobject itemObj = GetItem(env);
	if (!itemObj) return -1;
	const auto itemClass = g_Instance->FindClass(Mapper::Get("net/minecraft/item/Item"));
	if (!itemClass) { env->DeleteLocalRef(itemObj); return -1; }
	std::string sig = "(" + Mapper::Get("net/minecraft/item/Item", 2) + ")I";
	const auto m = itemClass->GetMethod(env, Mapper::Get("getIdFromItem").c_str(), sig.c_str(), true);
	if (!m) { env->DeleteLocalRef(itemObj); return -1; }
	int id = m->CallIntMethod(env, (jobject)itemClass, true, itemObj);
	env->DeleteLocalRef(itemObj);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
	return id;
}

int ItemStack::GetStackSize(JNIEnv* env)
{
	if (this == NULL || !env) return 0;
	const auto cls = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemStack"));
	if (!cls) return 0;
	const auto f = cls->GetField(env, Mapper::Get("stackSize").c_str(), "I");
	if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
	if (!f) return 0;
	return f->GetIntField(env, (jobject)this);
}

bool ItemStack::IsEnchanted(JNIEnv* env)
{
	if (this == NULL || !env) return false;
	const auto cls = g_Instance->FindClass(Mapper::Get("net/minecraft/item/ItemStack"));
	if (!cls) return false;
	const auto m = cls->GetMethod(env, Mapper::Get("isItemEnchanted").c_str(), "()Z");
	if (!m) return false;
	bool r = m->CallBoolMethod(env, (jobject)this);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
	return r;
}

int ItemStack::GetEnchantmentLevel(int enchId, JNIEnv* env)
{
	if (this == NULL || !env) return 0;
	const auto helper = g_Instance->FindClass(Mapper::Get("net/minecraft/enchantment/EnchantmentHelper"));
	if (!helper) return 0;
	std::string sig = "(I" + Mapper::Get("net/minecraft/item/ItemStack", 2) + ")I";
	const auto m = helper->GetMethod(env, Mapper::Get("getEnchantmentLevel").c_str(), sig.c_str(), true);
	if (!m) return 0;
	int lvl = m->CallIntMethod(env, (jobject)helper, true, enchId, (jobject)this);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
	return lvl;
}
