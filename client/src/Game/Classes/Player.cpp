#include "pch.h"
#include "Player.h"

#include "../Klass.h"
#include "../Field.h"
#include "../Method.h"
#include "../Mapper.h"

#include "../../Cheat/Hack.h"
#include <cmath>

std::string Player::GetName(JNIEnv* env, bool shouldEraseColor)
{
	if (this == NULL || !env)
		return "";

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return "";

	std::string chatSig = Mapper::Get("net/minecraft/util/IChatComponent", 3);
	std::string mappedName = Mapper::Get("getDisplayName");
	const char* chatNames[] = {
		mappedName.c_str(),
		"getFormattedCommandSenderName",
		"getDisplayName"
	};
	Method* getEntityStringMethod = nullptr;
	for (const char* n : chatNames) {
		if (!n || !n[0] || chatSig.empty()) continue;
		getEntityStringMethod = playerClazz->GetMethod(env, n, chatSig.c_str());
		if (env->ExceptionCheck()) { env->ExceptionClear(); getEntityStringMethod = nullptr; }
		if (getEntityStringMethod) break;
	}
	if (!getEntityStringMethod) {
		const char* strNames[] = { "getCommandSenderName", "getName" };
		for (const char* n : strNames) {
			Method* m = playerClazz->GetMethod(env, n, "()Ljava/lang/String;");
			if (env->ExceptionCheck()) { env->ExceptionClear(); m = nullptr; }
			if (!m) continue;
			const auto formattedTextObject = m->CallObjectMethod(env, (jobject)this);
			if (!formattedTextObject) { env->DeleteLocalRef((jclass)playerClazz); return ""; }
			const auto formattedText = (jstring)formattedTextObject;
			const auto entityName = env->GetStringUTFChars(formattedText, NULL);
			if (!entityName) { env->DeleteLocalRef(formattedTextObject); env->DeleteLocalRef((jclass)playerClazz); return ""; }
			auto ret = std::string(entityName);
			env->ReleaseStringUTFChars(formattedText, entityName);
			env->DeleteLocalRef(formattedTextObject);
			env->DeleteLocalRef((jclass)playerClazz);
			if (shouldEraseColor) {
				for (size_t i = 0; i + 1 < ret.size(); ) {
					if (ret[i] == '\xC2' && (unsigned char)ret[i + 1] == 0xA7) {
						ret.erase(i, (i + 3 <= ret.size()) ? 3 : 2);
						continue;
					}
					if (ret[i] == '\u00A7') {
						ret.erase(i, (i + 2 <= ret.size()) ? 2 : 1);
						continue;
					}
					i++;
				}
			}
			return ret;
		}
		env->DeleteLocalRef((jclass)playerClazz);
		return "";
	}
	const auto entityStringObject = getEntityStringMethod->CallObjectMethod(env, (jobject)this);
	if (!entityStringObject) { env->DeleteLocalRef((jclass)playerClazz); return ""; }
	const auto iChatComponentClazz = g_Instance->FindClass(Mapper::Get("net/minecraft/util/IChatComponent"));
	if (!iChatComponentClazz) { env->DeleteLocalRef(entityStringObject); env->DeleteLocalRef((jclass)playerClazz); return ""; }
	const auto getFormattedTextMethod = iChatComponentClazz->GetMethod(env, Mapper::Get("getUnformattedTextForChat").data(), "()Ljava/lang/String;");
	if (!getFormattedTextMethod) { env->DeleteLocalRef(entityStringObject); env->DeleteLocalRef((jclass)playerClazz); return ""; }
	const auto formattedTextObject = getFormattedTextMethod->CallObjectMethod(env, entityStringObject);
	if (!formattedTextObject) { env->DeleteLocalRef(entityStringObject); env->DeleteLocalRef((jclass)playerClazz); return ""; }
	const auto formattedText = (jstring)formattedTextObject;
	const auto entityName = env->GetStringUTFChars(formattedText, NULL);
	if (!entityName) { env->DeleteLocalRef(formattedTextObject); env->DeleteLocalRef(entityStringObject); env->DeleteLocalRef((jclass)playerClazz); return ""; }
	auto ret = std::string(entityName);

	if (formattedTextObject)
		env->ReleaseStringUTFChars(formattedText, entityName);

	if (playerClazz)
		env->DeleteLocalRef((jclass)playerClazz);

	if (entityStringObject)
		env->DeleteLocalRef(entityStringObject);

	if (formattedText)
		env->DeleteLocalRef(formattedText);

	auto getLastIndex = [](char* s, char c)
		{
			std::vector<int> indexes;
			int length;
			int i;

			length = (int)strlen(s);

			for (i = (length - 1); i >= 0; i--)
			{
				if (s[i] == c)
					indexes.push_back(i);
			}

			return indexes;
		};

	if (shouldEraseColor)
		for (auto index : getLastIndex(ret.data(), '\u00A7'))
			ret.erase(index - 1, 3);

	return ret;
}

float Player::GetRotationPitch(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("rotationPitch").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return 0.0f;
	return pitchField->GetFloatField(env, this);
}

float Player::GetRotationYaw(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("rotationYaw").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return 0.0f;
	return pitchField->GetFloatField(env, this);
}

float Player::GetRotationYawHead(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("rotationYawHead").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return 0.0f;
	return pitchField->GetFloatField(env, this);
}

float Player::GetPrevRotationPitch(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("prevRotationPitch").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return 0.0f;
	return pitchField->GetFloatField(env, this);
}

float Player::GetPrevRotationYaw(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("prevRotationYaw").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return 0.0f;
	return pitchField->GetFloatField(env, this);
}

float Player::GetPrevRenderYawOffset(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto prevRenderYawOffsetField = playerClazz->GetField(env, Mapper::Get("prevRenderYawOffset").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!prevRenderYawOffsetField) return 0.0f;
	return prevRenderYawOffsetField->GetFloatField(env, this);
}

float Player::GetRenderYawOffset(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto yawOffsetField = playerClazz->GetField(env, Mapper::Get("renderYawOffset").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!yawOffsetField) return 0.0f;
	return yawOffsetField->GetFloatField(env, this);
}

Vec3D Player::GetLastTickPos(JNIEnv* env)
{
	if (this == NULL || !env)
		return Vec3D();

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return Vec3D();

	const auto pitchFieldX = playerClazz->GetField(env, Mapper::Get("lastTickPosX").data(), "D");
	const auto pitchFieldY = playerClazz->GetField(env, Mapper::Get("lastTickPosY").data(), "D");
	const auto pitchFieldZ = playerClazz->GetField(env, Mapper::Get("lastTickPosZ").data(), "D");

	env->DeleteLocalRef((jclass)playerClazz);

	if (!pitchFieldX || !pitchFieldY || !pitchFieldZ)
		return Vec3D();

	return { pitchFieldX->GetDoubleField(env, this),
		pitchFieldY->GetDoubleField(env, this),
		pitchFieldZ->GetDoubleField(env, this)
	};
}

Vec3D Player::GetPos(JNIEnv* env)
{
	if (this == NULL || !env)
		return Vec3D();

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return Vec3D();

	const auto posFieldX = playerClazz->GetField(env, Mapper::Get("posX").data(), "D");
	const auto posFieldY = playerClazz->GetField(env, Mapper::Get("posY").data(), "D");
	const auto posFieldZ = playerClazz->GetField(env, Mapper::Get("posZ").data(), "D");

	env->DeleteLocalRef((jclass)playerClazz);

	if (!posFieldX || !posFieldY || !posFieldZ)
		return Vec3D();

	return { posFieldX->GetDoubleField(env, this),
		posFieldY->GetDoubleField(env, this),
		posFieldZ->GetDoubleField(env, this)
	};
}

Vec3D Player::GetPreviousPos(JNIEnv* env)
{
	if (this == NULL || !env)
		return Vec3D();

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return Vec3D();

	const auto posFieldX = playerClazz->GetField(env, Mapper::Get("prevPosX").data(), "D");
	const auto posFieldY = playerClazz->GetField(env, Mapper::Get("prevPosY").data(), "D");
	const auto posFieldZ = playerClazz->GetField(env, Mapper::Get("prevPosZ").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!posFieldX || !posFieldY || !posFieldZ) return Vec3D();
	return { posFieldX->GetDoubleField(env, this),
		posFieldY->GetDoubleField(env, this),
		posFieldZ->GetDoubleField(env, this)
	};
}

double Player::GetMotionX(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0;

	const auto motionField = playerClazz->GetField(env, Mapper::Get("motionX").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!motionField) return 0.0;
	return motionField->GetDoubleField(env, this);
}

double Player::GetMotionY(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0;

	const auto motionField = playerClazz->GetField(env, Mapper::Get("motionY").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!motionField) return 0.0;
	return motionField->GetDoubleField(env, this);
}

double Player::GetMotionZ(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0;

	const auto motionField = playerClazz->GetField(env, Mapper::Get("motionZ").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!motionField) return 0.0;
	return motionField->GetDoubleField(env, this);
}

float Player::GetHealth(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto getHealthMethod = playerClazz->GetMethod(env, Mapper::Get("getHealth").data(), "()F");

	env->DeleteLocalRef((jclass)playerClazz);

	if (!getHealthMethod)
		return 0.f;

	return getHealthMethod->CallFloatMethod(env, (jobject)this);
}

float Player::GetEyeHeight(JNIEnv* env)
{
	if (this == NULL || !env)
		return 1.62f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 1.62f;

	const auto m = playerClazz->GetMethod(env, Mapper::Get("getEyeHeight").data(), "()F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return 1.62f;
	return m->CallFloatMethod(env, (jobject)this);
}

float Player::GetMoveForward(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto moveField = playerClazz->GetField(env, Mapper::Get("moveForward").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!moveField) return 0.0f;
	return moveField->GetFloatField(env, this);
}

float Player::GetMoveStrafing(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0.0f;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0.0f;

	const auto moveField = playerClazz->GetField(env, Mapper::Get("moveStrafing").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!moveField) return 0.0f;
	return moveField->GetFloatField(env, this);
}

void Player::SetMoveForward(float buffer, JNIEnv* env)
{
	if (this == NULL || !env) return;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return;
	const auto moveField = playerClazz->GetField(env, Mapper::Get("moveForward").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!moveField) return;
	moveField->SetFloatField(env, this, buffer);
}

void Player::SetMoveStrafing(float buffer, JNIEnv* env)
{
	if (this == NULL || !env) return;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return;
	const auto moveField = playerClazz->GetField(env, Mapper::Get("moveStrafing").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!moveField) return;
	moveField->SetFloatField(env, this, buffer);
}

jobject Player::GetMovementInput(JNIEnv* env)
{
	if (this == NULL || !env) return NULL;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return NULL;
	const auto field = playerClazz->GetField(env,
		Mapper::Get("movementInput").data(),
		Mapper::Get("net/minecraft/util/MovementInput", 2).data());
	env->DeleteLocalRef((jclass)playerClazz);
	if (!field) return NULL;
	return field->GetObjectField(env, (jobject)this);
}

int Player::GetJumpTicks(JNIEnv* env)
{
	if (this == NULL || !env) return 0;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return 0;
	const auto field = playerClazz->GetField(env, Mapper::Get("jumpTicks").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!field) return 0;
	return field->GetIntField(env, this);
}

void Player::SetJumpTicks(int buffer, JNIEnv* env)
{
	if (this == NULL || !env) return;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return;
	const auto field = playerClazz->GetField(env, Mapper::Get("jumpTicks").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!field) return;
	field->SetIntField(env, this, buffer);
}

void Player::SetAlwaysRenderNameTag(bool state, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto setAlwaysRenderNametagMethod = playerClazz->GetMethod(env, Mapper::Get("setAlwaysRenderNameTag").data(), "(Z)V");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!setAlwaysRenderNametagMethod) return;
	setAlwaysRenderNametagMethod->CallVoidMethod(env, (jobject)this, false, state);
}

bool Player::IsNPC(JNIEnv* env)
{
	if (this == NULL || !env)
		return true;

	const std::string playerName = this->GetName(env, true);
	if (playerName.find("[NPC]") != std::string::npos)
		return true;

	return false;
}

bool Player::IsInvisible(JNIEnv* env)
{
	if (this == NULL || !env)
		return true;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return true;

	const auto isInvisibleMethod = playerClazz->GetMethod(env, Mapper::Get("isInvisible").data(), "()Z");

	env->DeleteLocalRef((jclass)playerClazz);

	if (!isInvisibleMethod)
		return false;

	return isInvisibleMethod->CallBoolMethod(env, (jobject)this);
}

bool Player::IsDead(JNIEnv* env)
{
	if (this == NULL || !env)
		return true;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return true;
	const auto f = playerClazz->GetField(env, Mapper::Get("isDead").data(), "Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!f) return false;
	return f->GetBooleanField(env, this);
}

bool Player::IsSneaking(JNIEnv* env)
{
	if (this == NULL || !env)
		return true;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return true;

	const auto isInvisibleMethod = playerClazz->GetMethod(env, Mapper::Get("isSneaking").data(), "()Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!isInvisibleMethod) return false;
	return isInvisibleMethod->CallBoolMethod(env, (jobject)this);
}

bool Player::IsSprinting(JNIEnv* env)
{
	if (this == NULL || !env)
		return false;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return false;

	const auto m = playerClazz->GetMethod(env, Mapper::Get("isSprinting").data(), "()Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return false;
	return m->CallBoolMethod(env, (jobject)this);
}

void Player::SetSprinting(bool state, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto m = playerClazz->GetMethod(env, Mapper::Get("setSprinting").data(), "(Z)V");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return;
	m->CallVoidMethod(env, (jobject)this, false, state);
}

bool Player::IsUsingItem(JNIEnv* env)
{
	if (this == NULL || !env)
		return false;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return false;

	const auto m = playerClazz->GetMethod(env, Mapper::Get("isUsingItem").data(), "()Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return false;
	return m->CallBoolMethod(env, (jobject)this);
}

bool Player::IsSwingInProgress(JNIEnv* env)
{
	if (this == NULL || !env)
		return false;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return false;

	const auto f = playerClazz->GetField(env, Mapper::Get("isSwingInProgress").data(), "Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!f) return false;
	return f->GetBooleanField(env, this);
}

bool Player::IsOnGround(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto hurtField = playerClazz->GetField(env, Mapper::Get("onGround").data(), "Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!hurtField) return false;
	return hurtField->GetBooleanField(env, this);
}

void Player::SetOnGround(bool state, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto f = playerClazz->GetField(env, Mapper::Get("onGround").data(), "Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!f) return;
	f->SetBooleanField(env, this, state);
}

bool Player::IsOnLadder(JNIEnv* env)
{
	if (this == NULL || !env)
		return false;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return false;

	const auto m = playerClazz->GetMethod(env, Mapper::Get("isOnLadder").data(), "()Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return false;
	return m->CallBoolMethod(env, (jobject)this);
}

bool Player::IsPotionActive(int potionId, JNIEnv* env)
{
	if (this == NULL || !env)
		return false;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return false;

	const auto m = playerClazz->GetMethod(env, Mapper::Get("isPotionActive").data(), "(I)Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return false;
	return m->CallBoolMethod(env, (jobject)this, false, potionId);
}

jobject Player::GetRidingEntity(JNIEnv* env)
{
	if (this == NULL || !env)
		return nullptr;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return nullptr;

	std::string sig = Mapper::Get("net/minecraft/entity/Entity", 2);
	const auto f = playerClazz->GetField(env, Mapper::Get("ridingEntity").data(), sig.c_str());
	env->DeleteLocalRef((jclass)playerClazz);
	if (!f) return nullptr;
	return f->GetObjectField(env, this);
}

bool Player::IsInWater(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto hurtField = playerClazz->GetField(env, Mapper::Get("inWater").data(), "Z");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!hurtField) return false;
	return hurtField->GetBooleanField(env, this);
}

int Player::GetMaxHurtResistantTime(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto hurtField = playerClazz->GetField(env, Mapper::Get("maxHurtResistantTime").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!hurtField) return 0;
	return hurtField->GetIntField(env, this);
}

int Player::GetHurtResistantTime(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto hurtField = playerClazz->GetField(env, Mapper::Get("hurtResistantTime").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!hurtField) return 0;
	return hurtField->GetIntField(env, this);
}

int Player::GetHurtTime(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto hurtField = playerClazz->GetField(env, Mapper::Get("hurtTime").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!hurtField) return 0;
	return hurtField->GetIntField(env, this);
}

jobject Player::GetBoundingBox(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto boundingBoxField = playerClazz->GetField(env, Mapper::Get("boundingBox").data(), Mapper::Get("net/minecraft/util/AxisAlignedBB", 2).data());

	env->DeleteLocalRef((jclass)playerClazz);

	if (!boundingBoxField)
		return nullptr;

	return boundingBoxField->GetObjectField(env, this);
}

jobject Player::GetHeldItem(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto getHeldItemMethod = playerClazz->GetMethod(env, Mapper::Get("getHeldItem").data(), Mapper::Get("net/minecraft/item/ItemStack", 3).data());

	env->DeleteLocalRef((jclass)playerClazz);

	if (!getHeldItemMethod)
		return nullptr;

	jobject held = getHeldItemMethod->CallObjectMethod(env, (jobject)this);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
	return held;
}

jobject Player::GetEquipmentInSlot(int slot, JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	std::string sig = "(I)" + Mapper::Get("net/minecraft/item/ItemStack", 2);
	const auto m = playerClazz->GetMethod(env, Mapper::Get("getEquipmentInSlot").data(), sig.c_str());
	env->DeleteLocalRef((jclass)playerClazz);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
	if (!m)
		return nullptr;

	jobject r = m->CallObjectMethod(env, (jobject)this, false, slot);
	if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
	return r;
}

jobject Player::GetInventoryPlayer(JNIEnv* env)
{
	if (this == NULL || !env)
		return NULL;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return NULL;

	const auto inventoryContainerField = playerClazz->GetField(env, Mapper::Get("inventory").data(), Mapper::Get("net/minecraft/entity/player/InventoryPlayer", 2).data());
	if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef((jclass)playerClazz); return nullptr; }
	env->DeleteLocalRef((jclass)playerClazz);
	if (!inventoryContainerField) return nullptr;
	return inventoryContainerField->GetObjectField(env, this);
}

jobject Player::GetOpenContainer(JNIEnv* env)
{
	if (this == NULL || !env) return nullptr;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return nullptr;
	const auto f = playerClazz->GetField(env, Mapper::Get("openContainer").data(),
		Mapper::Get("net/minecraft/inventory/Container", 2).data());
	env->DeleteLocalRef((jclass)playerClazz);
	if (!f) return nullptr;
	return f->GetObjectField(env, this);
}

void Player::CloseScreen(JNIEnv* env)
{
	if (this == NULL || !env) return;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return;
	const auto m = playerClazz->GetMethod(env, Mapper::Get("closeScreen").data(), "()V");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return;
	m->CallVoidMethod(env, (jobject)this);
}

void Player::DropOneItem(bool dropAll, JNIEnv* env)
{
	if (this == NULL || !env) return;
	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz) return;
	std::string sig = "(Z)" + Mapper::Get("net/minecraft/entity/item/EntityItem", 2);
	const auto m = playerClazz->GetMethod(env, Mapper::Get("dropOneItem").data(), sig.c_str());
	env->DeleteLocalRef((jclass)playerClazz);
	if (!m) return;
	jobject r = m->CallObjectMethod(env, (jobject)this, false, (jboolean)(dropAll ? JNI_TRUE : JNI_FALSE));
	if (r) env->DeleteLocalRef(r);
	if (env->ExceptionCheck()) env->ExceptionClear();
}

void Player::SetMotionX(double buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto motionField = playerClazz->GetField(env, Mapper::Get("motionX").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!motionField) return;
	motionField->SetDoubleField(env, this, buffer);
}

void Player::SetMotionY(double buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto motionField = playerClazz->GetField(env, Mapper::Get("motionY").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!motionField) return;
	motionField->SetDoubleField(env, this, buffer);
}

void Player::SetMotionZ(double buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto motionField = playerClazz->GetField(env, Mapper::Get("motionZ").data(), "D");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!motionField) return;
	motionField->SetDoubleField(env, this, buffer);
}

void Player::SetRotationPitch(float buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("rotationPitch").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return;
	pitchField->SetFloatField(env, this, buffer);
}

void Player::SetRotationYaw(float buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("rotationYaw").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return;
	pitchField->SetFloatField(env, this, buffer);
}

void Player::SetPrevRotationPitch(float buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("prevRotationPitch").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return;
	pitchField->SetFloatField(env, this, buffer);
}

void Player::SetPrevRotationYaw(float buffer, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return;

	const auto pitchField = playerClazz->GetField(env, Mapper::Get("prevRotationYaw").data(), "F");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!pitchField) return;
	pitchField->SetFloatField(env, this, buffer);
}

int Player::GetEntityId(JNIEnv* env)
{
	if (this == NULL || !env)
		return -1;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return -1;

	const auto idField = playerClazz->GetField(env, Mapper::Get("entityId").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!idField) return -1;
	return idField->GetIntField(env, this);
}

bool Player::GetUuidBits(JNIEnv* env, jlong& most, jlong& least)
{
	most = 0;
	least = 0;
	if (this == NULL || !env)
		return false;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return false;

	const auto uuidField = playerClazz->GetField(env, Mapper::Get("entityUniqueID").data(), "Ljava/util/UUID;");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!uuidField) return false;

	jobject uuid = uuidField->GetObjectField(env, this);
	if (!uuid) {
		if (env->ExceptionCheck()) env->ExceptionClear();
		return false;
	}

	static jclass uuidCls = nullptr;
	static jmethodID mMost = nullptr;
	static jmethodID mLeast = nullptr;
	if (!uuidCls) {
		jclass local = env->FindClass("java/util/UUID");
		if (!local) {
			if (env->ExceptionCheck()) env->ExceptionClear();
			env->DeleteLocalRef(uuid);
			return false;
		}
		uuidCls = (jclass)env->NewGlobalRef(local);
		env->DeleteLocalRef(local);
		mMost = env->GetMethodID(uuidCls, "getMostSignificantBits", "()J");
		mLeast = env->GetMethodID(uuidCls, "getLeastSignificantBits", "()J");
	}
	if (!mMost || !mLeast) {
		env->DeleteLocalRef(uuid);
		return false;
	}

	most = env->CallLongMethod(uuid, mMost);
	least = env->CallLongMethod(uuid, mLeast);
	env->DeleteLocalRef(uuid);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		most = 0;
		least = 0;
		return false;
	}
	return true;
}

int Player::GetTicksExisted(JNIEnv* env)
{
	if (this == NULL || !env)
		return 0;

	const auto playerClazz = (Klass*)env->GetObjectClass((jobject)this);
	if (!playerClazz)
		return 0;

	const auto f = playerClazz->GetField(env, Mapper::Get("ticksExisted").data(), "I");
	env->DeleteLocalRef((jclass)playerClazz);
	if (!f) return 0;
	return f->GetIntField(env, this);
}

void Player::SetAngles(float yaw, float pitch, JNIEnv* env)
{
	if (this == NULL || !env)
		return;

	const float y0 = GetRotationYaw(env);
	const float p0 = GetRotationPitch(env);

	jclass cls = env->GetObjectClass((jobject)this);
	if (!cls) return;

	jmethodID mid = env->GetMethodID(cls, Mapper::Get("setAngles").c_str(), "(FF)V");
	if (env->ExceptionCheck()) { env->ExceptionClear(); mid = nullptr; }
	if (!mid && g_Instance) {
		Klass* ent = g_Instance->FindClass(Mapper::Get("net/minecraft/entity/Entity").c_str());
		if (ent) {
			mid = env->GetMethodID((jclass)ent, Mapper::Get("setAngles").c_str(), "(FF)V");
			if (env->ExceptionCheck()) { env->ExceptionClear(); mid = nullptr; }
		}
	}
	if (mid) {
		env->CallVoidMethod((jobject)this, mid, yaw, pitch);
		if (env->ExceptionCheck()) env->ExceptionClear();
	}

	const float y1 = GetRotationYaw(env);
	const float p1 = GetRotationPitch(env);
	const bool methodMoved = (fabsf(y1 - y0) > 1e-4f) || (fabsf(p1 - p0) > 1e-4f);

	if (!methodMoved) {
		jfieldID fy = env->GetFieldID(cls, Mapper::Get("rotationYaw").c_str(), "F");
		if (env->ExceptionCheck()) { env->ExceptionClear(); fy = nullptr; }
		jfieldID fp = env->GetFieldID(cls, Mapper::Get("rotationPitch").c_str(), "F");
		if (env->ExceptionCheck()) { env->ExceptionClear(); fp = nullptr; }
		jfieldID fpy = env->GetFieldID(cls, Mapper::Get("prevRotationYaw").c_str(), "F");
		if (env->ExceptionCheck()) { env->ExceptionClear(); fpy = nullptr; }
		jfieldID fpp = env->GetFieldID(cls, Mapper::Get("prevRotationPitch").c_str(), "F");
		if (env->ExceptionCheck()) { env->ExceptionClear(); fpp = nullptr; }

		const float ny = y0 + yaw * 0.15f;
		float np = p0 - pitch * 0.15f;
		if (np > 90.f) np = 90.f;
		if (np < -90.f) np = -90.f;

		if (fy) env->SetFloatField((jobject)this, fy, ny);
		if (fp) env->SetFloatField((jobject)this, fp, np);
		if (fpy) env->SetFloatField((jobject)this, fpy, ny);
		if (fpp) env->SetFloatField((jobject)this, fpp, np);
	}

	env->DeleteLocalRef(cls);
}