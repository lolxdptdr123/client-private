#pragma once

class Player
{
public:
	std::string GetName(JNIEnv* env, bool shouldEraseColor = false);
	float GetRotationPitch(JNIEnv* env);
	float GetRotationYaw(JNIEnv* env);
	float GetRotationYawHead(JNIEnv* env);
	float GetPrevRotationPitch(JNIEnv* env);
	float GetPrevRotationYaw(JNIEnv* env);
	float GetPrevRenderYawOffset(JNIEnv* env);
	float GetRenderYawOffset(JNIEnv* env);
	float GetHealth(JNIEnv* env);
	float GetEyeHeight(JNIEnv* env);
	float GetMoveForward(JNIEnv* env);
	float GetMoveStrafing(JNIEnv* env);
	void SetMoveForward(float buffer, JNIEnv* env);
	void SetMoveStrafing(float buffer, JNIEnv* env);
	jobject GetMovementInput(JNIEnv* env);
	int GetJumpTicks(JNIEnv* env);
	void SetJumpTicks(int buffer, JNIEnv* env);
	Vec3D GetLastTickPos(JNIEnv* env);
	Vec3D GetPos(JNIEnv* env);
	Vec3D GetPreviousPos(JNIEnv* env);
	double GetMotionX(JNIEnv* env);
	double GetMotionY(JNIEnv* env);
	double GetMotionZ(JNIEnv* env);
	bool IsNPC(JNIEnv* env);
	bool IsInvisible(JNIEnv* env);
	bool IsDead(JNIEnv* env);
	bool IsSneaking(JNIEnv* env);
	bool IsSprinting(JNIEnv* env);
	void SetSprinting(bool state, JNIEnv* env);
	bool IsUsingItem(JNIEnv* env);
	bool IsSwingInProgress(JNIEnv* env);
	bool IsOnGround(JNIEnv* env);
	void SetOnGround(bool state, JNIEnv* env);
	bool IsInWater(JNIEnv* env);
	bool IsOnLadder(JNIEnv* env);
	bool IsPotionActive(int potionId, JNIEnv* env);
	jobject GetRidingEntity(JNIEnv* env);
	int GetMaxHurtResistantTime(JNIEnv* env);
	int GetHurtResistantTime(JNIEnv* env);
	int GetHurtTime(JNIEnv* env);
	int GetEntityId(JNIEnv* env);
	bool GetUuidBits(JNIEnv* env, jlong& most, jlong& least);
	int GetTicksExisted(JNIEnv* env);
	jobject GetBoundingBox(JNIEnv* env);
	jobject GetHeldItem(JNIEnv *env);
	jobject GetEquipmentInSlot(int slot, JNIEnv* env);
	jobject GetInventoryPlayer(JNIEnv* env);
	jobject GetOpenContainer(JNIEnv* env);
	void CloseScreen(JNIEnv* env);
	void DropOneItem(bool dropAll, JNIEnv* env);

	void SetAlwaysRenderNameTag(bool state, JNIEnv* env);
	void SetMotionX(double buffer, JNIEnv* env);
	void SetMotionY(double buffer, JNIEnv* env);
	void SetMotionZ(double buffer, JNIEnv* env);
	void SetRotationPitch(float buffer, JNIEnv* env);
	void SetRotationYaw(float buffer, JNIEnv* env);
	void SetPrevRotationPitch(float buffer, JNIEnv* env);
	void SetPrevRotationYaw(float buffer, JNIEnv* env);
	void SetAngles(float yaw, float pitch, JNIEnv* env);
};