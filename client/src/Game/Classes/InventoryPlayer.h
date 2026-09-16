#pragma once

class InventoryPlayer
{
public:
	int GetSlot(JNIEnv* env);
	void SetSlot(int slot, JNIEnv* env);
	jobject GetStackInSlot(int slot, JNIEnv* env);
	bool SetStackInSlot(int slot, jobject stack, JNIEnv* env);
	jobject GetArmorItem(int index, JNIEnv* env);
	bool IsHotbarFull(JNIEnv* env);
	int CountEmptyHotbarSlots(JNIEnv* env);
};