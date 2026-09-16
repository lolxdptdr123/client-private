#pragma once
#include <string>

class ItemStack
{
public:
	jobject GetItem(JNIEnv* env);
	bool IsWeapon(JNIEnv* env);
	bool IsBlock(JNIEnv* env);
	bool IsEnderPearl(JNIEnv* env);
	bool IsSoup(JNIEnv* env);
	bool IsRod(JNIEnv* env);
	bool Is(const char* clazz, JNIEnv* env);
	bool IsEmpty(JNIEnv* env);
	int GetMetadata(JNIEnv* env);
	std::string GetDisplayName(JNIEnv* env);
	int GetItemId(JNIEnv* env);
	int GetStackSize(JNIEnv* env);
	bool IsEnchanted(JNIEnv* env);
	int GetEnchantmentLevel(int enchId, JNIEnv* env);
};