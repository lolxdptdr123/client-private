#define _CRT_SECURE_NO_WARNINGS
#include "pch.h"
#include "InventoryPlayer.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"
#include "ItemStack.h"

#include "../../Cheat/Hack.h"
#include <jvmti.h>
#include <string>

static jobjectArray ResolveMainInventory(JNIEnv* env, jobject self, jclass invCls) {
	if (!env || !self || !invCls) return nullptr;
	std::string sig = "[" + Mapper::Get("net/minecraft/item/ItemStack", 2);
	auto tryName = [&](const char* name) -> jobjectArray {
		if (!name || !name[0] || sig.size() < 3) return nullptr;
		jfieldID f = env->GetFieldID(invCls, name, sig.c_str());
		if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
		if (!f) return nullptr;
		auto arr = (jobjectArray)env->GetObjectField(self, f);
		if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
		return arr;
	};
	jobjectArray a = tryName(Mapper::Get("mainInventory").c_str());
	jobjectArray b = tryName(Mapper::Get("armorInventory").c_str());
	auto len = [&](jobjectArray arr) -> jsize {
		if (!arr) return 0;
		jsize n = env->GetArrayLength(arr);
		if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
		return n;
	};
	jsize la = len(a), lb = len(b);
	if (la >= 36) {
		if (b) env->DeleteLocalRef(b);
		return a;
	}
	if (lb >= 36) {
		if (a) env->DeleteLocalRef(a);
		return b;
	}
	if (a) env->DeleteLocalRef(a);
	if (b) env->DeleteLocalRef(b);

	JavaVM* vm = nullptr;
	jvmtiEnv* jvmti = nullptr;
	if (env->GetJavaVM(&vm) != 0 || !vm) return nullptr;
	if (vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2) != JNI_OK || !jvmti) return nullptr;
	jobjectArray best = nullptr;
	jsize bestLen = 0;
	jint nf = 0; jfieldID* fids = nullptr;
	if (jvmti->GetClassFields(invCls, &nf, &fids) == JVMTI_ERROR_NONE && fids) {
		for (jint i = 0; i < nf; i++) {
			char* fs = nullptr; jint fm = 0;
			if (jvmti->GetFieldName(invCls, fids[i], nullptr, &fs, nullptr) != JVMTI_ERROR_NONE) continue;
			jvmti->GetFieldModifiers(invCls, fids[i], &fm);
			bool arrField = fs && fs[0] == '[' && fs[1] == 'L' && (fm & 0x0008) == 0;
			if (fs) jvmti->Deallocate((unsigned char*)fs);
			if (!arrField) continue;
			auto cand = (jobjectArray)env->GetObjectField(self, fids[i]);
			if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
			if (!cand) continue;
			jsize n = env->GetArrayLength(cand);
			if (n >= 36 && n > bestLen) {
				if (best) env->DeleteLocalRef(best);
				best = cand;
				bestLen = n;
			} else {
				env->DeleteLocalRef(cand);
			}
		}
		jvmti->Deallocate((unsigned char*)fids);
	}
	return best;
}

int InventoryPlayer::GetSlot(JNIEnv* env)
{
	if (this == NULL)
		return NULL;

	const auto inventoryPlayerClass = (Klass*)env->GetObjectClass((jobject)this);
	const auto hotbarSlotField = inventoryPlayerClass->GetField(env, Mapper::Get("currentItem").data(), "I");

	if (inventoryPlayerClass)
		env->DeleteLocalRef((jclass)inventoryPlayerClass);

	if (!hotbarSlotField)
		return 0;

	return hotbarSlotField->GetIntField(env, this);
}

void InventoryPlayer::SetSlot(int slot, JNIEnv* env)
{
	if (this == NULL)
		return;

	const auto inventoryPlayerClass = (Klass*)env->GetObjectClass((jobject)this);
	const auto hotbarSlotField = inventoryPlayerClass->GetField(env, Mapper::Get("currentItem").data(), "I");

	if (inventoryPlayerClass)
		env->DeleteLocalRef((jclass)inventoryPlayerClass);

	hotbarSlotField->SetIntField(env, this, slot);
}

jobject InventoryPlayer::GetStackInSlot(int slot, JNIEnv* env)
{
	if (this == NULL)
		return NULL;

	const auto inventoryPlayerClass = (Klass*)env->GetObjectClass((jobject)this);
	if (!inventoryPlayerClass)
		return NULL;

	auto fromArray = [&]() -> jobject {
		auto arr = ResolveMainInventory(env, (jobject)this, (jclass)inventoryPlayerClass);
		if (!arr) return NULL;
		jobject r = nullptr;
		jsize n = env->GetArrayLength(arr);
		if (slot >= 0 && slot < n)
			r = env->GetObjectArrayElement(arr, slot);
		if (env->ExceptionCheck()) { env->ExceptionClear(); r = nullptr; }
		env->DeleteLocalRef(arr);
		return r;
	};

	jobject r = fromArray();
	if (r) {
		env->DeleteLocalRef((jclass)inventoryPlayerClass);
		return r;
	}

	char methodClass[128];
	sprintf(methodClass, "(I)%s", Mapper::Get("net/minecraft/item/ItemStack", 2).data());
	const auto getStackInSlotMethod = inventoryPlayerClass->GetMethod(env, Mapper::Get("getStackInSlot").data(), methodClass);
	if (env->ExceptionCheck()) env->ExceptionClear();

	if (getStackInSlotMethod) {
		r = getStackInSlotMethod->CallObjectMethod(env, (jobject)this, false, slot);
		if (env->ExceptionCheck()) { env->ExceptionClear(); r = NULL; }
		if (r) {
			env->DeleteLocalRef((jclass)inventoryPlayerClass);
			return r;
		}
	}

	r = fromArray();
	env->DeleteLocalRef((jclass)inventoryPlayerClass);
	return r;
}

bool InventoryPlayer::SetStackInSlot(int slot, jobject stack, JNIEnv* env)
{
	if (this == NULL || !env) return false;
	const auto inventoryPlayerClass = (Klass*)env->GetObjectClass((jobject)this);
	if (!inventoryPlayerClass) return false;
	auto arr = ResolveMainInventory(env, (jobject)this, (jclass)inventoryPlayerClass);
	env->DeleteLocalRef((jclass)inventoryPlayerClass);
	if (!arr) return false;
	jsize n = env->GetArrayLength(arr);
	if (slot < 0 || slot >= n) {
		env->DeleteLocalRef(arr);
		return false;
	}
	env->SetObjectArrayElement(arr, slot, stack);
	if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(arr); return false; }
	env->DeleteLocalRef(arr);
	return true;
}

jobject InventoryPlayer::GetArmorItem(int index, JNIEnv* env)
{
	if (this == NULL || !env || index < 0 || index > 3)
		return NULL;

	const auto inventoryPlayerClass = (Klass*)env->GetObjectClass((jobject)this);
	if (!inventoryPlayerClass)
		return NULL;

	std::string sig = "[" + Mapper::Get("net/minecraft/item/ItemStack", 2);
	Field* f = inventoryPlayerClass->GetField(env, Mapper::Get("armorInventory").c_str(), sig.c_str());
	if (env->ExceptionCheck()) { env->ExceptionClear(); f = nullptr; }
	env->DeleteLocalRef((jclass)inventoryPlayerClass);
	if (!f)
		return NULL;

	auto arr = (jobjectArray)f->GetObjectField(env, (jobject)this);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return NULL; }
	if (!arr)
		return NULL;
	jsize n = env->GetArrayLength(arr);
	jobject r = nullptr;
	if (index < n)
		r = env->GetObjectArrayElement(arr, index);
	if (env->ExceptionCheck()) { env->ExceptionClear(); r = nullptr; }
	env->DeleteLocalRef(arr);
	return r;
}

bool InventoryPlayer::IsHotbarFull(JNIEnv* env)
{
	return CountEmptyHotbarSlots(env) == 0;
}

int InventoryPlayer::CountEmptyHotbarSlots(JNIEnv* env)
{
	if (this == NULL) return 9;
	int empty = 0;
	for (int i = 0; i < 9; i++) {
		jobject stack = GetStackInSlot(i, env);
		if (!stack || ((ItemStack*)stack)->IsEmpty(env)) empty++;
		if (stack) env->DeleteLocalRef(stack);
	}
	return empty;
}
