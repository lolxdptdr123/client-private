#include "pch.h"
#include "Mapper.h"
#include "../Cheat/Modules/Settings.h"
#include "Mappings/CheatBreaker_v1_8_9.inc"
#include "Mappings/CheatBreaker_v1_7_10.inc"
#include <unordered_map>
#include <string>

static std::unordered_map<std::string, std::string> g_Mappings;
static std::unordered_map<std::string, std::string> g_CbSimple;
static bool g_UseCb = false;

void Mapper::Initialize(const GameVersions version)
{
	g_Mappings.clear();
	g_CbSimple.clear();
	g_UseCb = (g_GameLauncher == LAUNCHER_CHEATBREAKER);
	if (g_UseCb) {
		if (version == LUNAR_1_8_9) {
			MapperFillCB_1_8(g_Mappings);
			MapperFillCBClasses_1_8(g_CbSimple);
		} else {
			MapperFillCB_1_7(g_Mappings);
			MapperFillCBClasses_1_7(g_CbSimple);
		}
		if (version == LUNAR_1_8_9) {
			auto sp = g_Mappings.find("net/minecraft/client/entity/EntityPlayerSP");
			if (sp != g_Mappings.end())
				g_Mappings["net/minecraft/client/entity/EntityClientPlayerMP"] = sp->second;
		}
		return;
	}
	switch (version)
	{
	case LUNAR_1_8_9:
	case LUNAR_1_7_10:
	{
		// WhipBin mappings/v1_7_10/lunar.json
		g_Mappings = {
			{ "hurtTime", "hurtTime" },
			{ "getIdFromItem", "getIdFromItem" },
			{ "net/minecraft/client/Minecraft", "net/minecraft/client/Minecraft" },
			{ "net/minecraft/client/entity/EntityClientPlayerMP", "net/minecraft/client/entity/EntityClientPlayerMP" },
			{ "net/minecraft/entity/player/EntityPlayer", "net/minecraft/entity/player/EntityPlayer" },
			{ "net/minecraft/client/multiplayer/WorldClient", "net/minecraft/client/multiplayer/WorldClient" },
			{ "net/minecraft/client/gui/GuiScreen", "net/minecraft/client/gui/GuiScreen" },
			{ "net/minecraft/client/renderer/entity/RenderManager", "net/minecraft/client/renderer/entity/RenderManager" },
			{ "net/minecraft/entity/Entity", "net/minecraft/entity/Entity" },
			{ "net/minecraft/client/renderer/entity/Render", "net/minecraft/client/renderer/entity/Render" },
			{ "net/minecraft/client/model/ModelBase", "net/minecraft/client/model/ModelBase" },
			{ "net/minecraft/client/renderer/ActiveRenderInfo", "net/minecraft/client/renderer/ActiveRenderInfo" },
			{ "net/minecraft/util/IChatComponent", "net/minecraft/util/IChatComponent" },
			{ "net/minecraft/client/model/ModelRenderer", "net/minecraft/client/model/ModelRenderer" },
			{ "net/minecraft/util/Timer", "net/minecraft/util/Timer" },
			{ "net/minecraft/client/gui/Gui", "net/minecraft/client/gui/Gui" },
			{ "net/minecraft/client/gui/FontRenderer", "net/minecraft/client/gui/FontRenderer" },
			{ "net/minecraft/util/AxisAlignedBB", "net/minecraft/util/AxisAlignedBB" },
			{ "net/minecraft/client/gui/GuiChat", "net/minecraft/client/gui/GuiChat" },
			{ "net/minecraft/client/renderer/entity/RendererLivingEntity", "net/minecraft/client/renderer/entity/RendererLivingEntity" },
			{ "net/minecraft/item/ItemStack", "net/minecraft/item/ItemStack" },
			{ "net/minecraft/item/ItemSword", "net/minecraft/item/ItemSword" },
			{ "net/minecraft/item/ItemAxe", "net/minecraft/item/ItemAxe" },
			{ "net/minecraft/item/Item", "net/minecraft/item/Item" },
			{ "net/minecraft/item/ItemBlock", "net/minecraft/item/ItemBlock" },
			{ "net/minecraft/util/MovingObjectPosition$MovingObjectType", "net/minecraft/util/MovingObjectPosition$MovingObjectType" },
			{ "net/minecraft/util/MovingObjectPosition", "net/minecraft/util/MovingObjectPosition" },
			{ "net/minecraft/client/gui/inventory/GuiInventory", "net/minecraft/client/gui/inventory/GuiInventory" },
			{ "net/minecraft/entity/player/InventoryPlayer", "net/minecraft/entity/player/InventoryPlayer" },
			{ "net/minecraft/item/ItemEnderPearl", "net/minecraft/item/ItemEnderPearl" },
			{ "net/minecraft/item/ItemPotion", "net/minecraft/item/ItemPotion" },
			{ "isSplash", "isSplash" },
			{ "getColorFromDamage", "getColorFromDamage" },
			{ "getColorFromItemStack", "getColorFromItemStack" },
			{ "net/minecraft/block/Block", "net/minecraft/block/Block" },
			{ "net/minecraft/client/settings/GameSettings", "net/minecraft/client/settings/GameSettings" },
			{ "net/minecraft/client/settings/KeyBinding", "net/minecraft/client/settings/KeyBinding" },
			{ "net/minecraft/block/BlockAir", "net/minecraft/block/BlockAir" },
			{ "keyBindSneak", "keyBindSneak" },
			{ "keyBindJump", "keyBindJump" },
			{ "keyBindForward", "keyBindForward" },
			{ "keyBindBack", "keyBindBack" },
			{ "keyBindLeft", "keyBindLeft" },
			{ "keyBindRight", "keyBindRight" },
			{ "keyBindSprint", "keyBindSprint" },
			{ "keyBindUseItem", "keyBindUseItem" },
			{ "keyBindAttack", "keyBindAttack" },
			{ "keyCode", "keyCode" },
			{ "dropOneItem", "dropOneItem" },
			{ "net/minecraft/entity/item/EntityItem", "net/minecraft/entity/item/EntityItem" },
			{ "getEntityItem", "getEntityItem" },
			{ "itemStackDisplayName", "getDisplayName" },
			{ "pressed", "pressed" },
			{ "gameSettings", "gameSettings" },
			{ "thirdPersonView", "thirdPersonView" },
			{ "getBlock", "getBlock" },
			{ "getIdFromBlock", "getIdFromBlock" },
			{ "theMinecraft", "theMinecraft" },
			{ "thePlayer", "thePlayer" },
			{ "theWorld", "theWorld" },
			{ "currentScreen", "currentScreen" },
			{ "renderManagerInstance", "instance" },
			{ "renderManager", "renderManager" },
			{ "getEntityRenderObject", "getEntityRenderObject" },
			{ "mainModel", "mainModel" },
			{ "PROJECTION", "projection" },
			{ "MODELVIEW", "modelview" },
			{ "playerEntities", "playerEntities" },
			{ "loadedEntityList", "loadedEntityList" },
			{ "loadedTileEntityList", "loadedTileEntityList" },
			{ "xCoord", "xCoord" },
			{ "yCoord", "yCoord" },
			{ "zCoord", "zCoord" },
			{ "pos", "pos" },
			{ "getPos", "getPos" },
			{ "getX", "getX" },
			{ "getY", "getY" },
			{ "getZ", "getZ" },
			{ "net/minecraft/util/BlockPos", "net/minecraft/util/BlockPos" },
			{ "net/minecraft/tileentity/TileEntityChest", "net/minecraft/tileentity/TileEntityChest" },
			{ "net/minecraft/tileentity/TileEntityEnderChest", "net/minecraft/tileentity/TileEntityEnderChest" },
			{ "net/minecraft/tileentity/TileEntityFurnace", "net/minecraft/tileentity/TileEntityFurnace" },
			{ "net/minecraft/tileentity/TileEntityDispenser", "net/minecraft/tileentity/TileEntityDispenser" },
			{ "net/minecraft/tileentity/TileEntityDropper", "net/minecraft/tileentity/TileEntityDropper" },
			{ "net/minecraft/tileentity/TileEntityHopper", "net/minecraft/tileentity/TileEntityHopper" },
			{ "renderEntitySimple", "renderEntitySimple" },
			{ "isDead", "isDead" },
			{ "net/minecraft/entity/EntityLivingBase", "net/minecraft/entity/EntityLivingBase" },
			{ "net/minecraft/entity/monster/EntityMob", "net/minecraft/entity/monster/EntityMob" },
			{ "net/minecraft/entity/passive/EntityAnimal", "net/minecraft/entity/passive/EntityAnimal" },
			{ "net/minecraft/entity/passive/EntityVillager", "net/minecraft/entity/passive/EntityVillager" },
			{ "net/minecraft/entity/item/EntityArmorStand", "net/minecraft/entity/item/EntityArmorStand" },
			{ "getDisplayName", "getFormattedCommandSenderName" },
			{ "getUnformattedTextForChat", "getUnformattedTextForChat" },
			{ "rotationPitch", "rotationPitch" },
			{ "rotationYaw", "rotationYaw" },
			{ "prevRotationYaw", "prevRotationYaw" },
			{ "prevRotationPitch", "prevRotationPitch" },
			{ "posX", "posX" },
			{ "posY", "posY" },
			{ "posZ", "posZ" },
			{ "motionX", "motionX" },
			{ "motionY", "motionY" },
			{ "motionZ", "motionZ" },
			{ "maxHurtResistantTime", "maxHurtResistantTime" },
			{ "hurtResistantTime", "hurtResistantTime" },
			{ "bipedHeadwear", "bipedHeadwear" },
			{ "bipedRightArm", "bipedRightArm" },
			{ "bipedLeftArm", "bipedLeftArm" },
			{ "bipedRightLeg", "bipedRightLeg" },
			{ "bipedLeftLeg", "bipedLeftLeg" },
			{ "rotateAngleX", "rotateAngleX" },
			{ "rotateAngleY", "rotateAngleY" },
			{ "rotateAngleZ", "rotateAngleZ" },
			{ "prevRenderYawOffset", "prevRenderYawOffset" },
			{ "renderYawOffset", "renderYawOffset" },
			{ "renderPartialTicks", "renderPartialTicks" },
			{ "lastTickPosX", "lastTickPosX" },
			{ "lastTickPosY", "lastTickPosY" },
			{ "lastTickPosZ", "lastTickPosZ" },
			{ "timer", "timer" },
			{ "displayWidth", "displayWidth" },
			{ "displayHeight", "displayHeight" },
			{ "renderPosX", "renderPosX" },
			{ "renderPosY", "renderPosY" },
			{ "renderPosZ", "renderPosZ" },
			{ "drawRect", "drawRect" },
			{ "getStringWidth", "getStringWidth" },
			{ "drawString", "drawString" },
			{ "fontRendererObj", "fontRendererObj" },
			{ "getHealth", "getHealth" },
			{ "setAlwaysRenderNameTag", "setAlwaysRenderNameTag" },
			{ "isInvisible", "isInvisible" },
			{ "viewerPosX", "viewerPosX" },
			{ "viewerPosY", "viewerPosY" },
			{ "viewerPosZ", "viewerPosZ" },
			{ "boundingBox", "boundingBox" },
			{ "minX", "minX" },
			{ "minY", "minY" },
			{ "minZ", "minZ" },
			{ "maxX", "maxX" },
			{ "maxY", "maxY" },
			{ "maxZ", "maxZ" },
			{ "prevPosX", "prevPosX" },
			{ "prevPosY", "prevPosY" },
			{ "prevPosZ", "prevPosZ" },
			{ "isSneaking", "isSneaking" },
			{ "isSprinting", "isSprinting" },
			{ "setSprinting", "setSprinting" },
			{ "sprintingTicksLeft", "sprintingTicksLeft" },
			{ "rotationYawHead", "rotationYawHead" },
			{ "onGround", "onGround" },
			{ "isOnLadder", "isOnLadder" },
			{ "ridingEntity", "ridingEntity" },
			{ "isPotionActive", "isPotionActive" },
			{ "ticksPerSecond", "ticksPerSecond" },
			{ "sendQueue", "sendQueue" },
			{ "addToSendQueue", "addToSendQueue" },
			{ "net/minecraft/client/network/NetHandlerPlayClient", "net/minecraft/client/network/NetHandlerPlayClient" },
			{ "net/minecraft/network/Packet", "net/minecraft/network/Packet" },
			{ "net/minecraft/network/play/client/C03PacketPlayer$C04PacketPlayerPosition", "net/minecraft/network/play/client/C03PacketPlayer$C04PacketPlayerPosition" },
			{ "net/minecraft/network/play/client/C03PacketPlayer$C05PacketPlayerLook", "net/minecraft/network/play/client/C03PacketPlayer$C05PacketPlayerLook" },
			{ "net/minecraft/item/ItemFishingRod", "net/minecraft/item/ItemFishingRod" },
			{ "rightClickMouse", "rightClickMouse" },
			{ "rightClickDelayTimer", "rightClickDelayTimer" },
			{ "isSwingInProgress", "isSwingInProgress" },
			{ "getHeldItem", "getHeldItem" },
			{ "item", "theItem" },
			{ "getItem", "getItem" },
			{ "typeOfHit", "typeOfHit" },
			{ "BLOCK", "BLOCK" },
			{ "ENTITY", "ENTITY" },
			{ "objectMouseOver", "objectMouseOver" },
			{ "blockPos", "blockPos" },
			{ "getBlockPos", "getBlockPos" },
			{ "pointedEntity", "pointedEntity" },
			{ "inWater", "inWater" },
			{ "moveForward", "moveForward" },
			{ "moveStrafing", "moveStrafing" },
			{ "moveStrafe", "moveStrafe" },
			{ "jumpTicks", "jumpTicks" },
			{ "movementInput", "movementInput" },
			{ "net/minecraft/util/MovementInput", "net/minecraft/util/MovementInput" },
			{ "timerSpeed", "timerSpeed" },
			{ "inventory", "inventory" },
			{ "currentItem", "currentItem" },
			{ "getStackInSlot", "getStackInSlot" },
			{ "mainInventory", "mainInventory" },
			{ "armorInventory", "armorInventory" },
			{ "stackSize", "stackSize" },
			{ "isItemEnchanted", "isItemEnchanted" },
			{ "net/minecraft/enchantment/EnchantmentHelper", "net/minecraft/enchantment/EnchantmentHelper" },
			{ "getEnchantmentLevel", "getEnchantmentLevel" },
			{ "getActivePotionEffects", "getActivePotionEffects" },
			{ "activePotionsMap", "activePotionsMap" },
			{ "net/minecraft/potion/PotionEffect", "net/minecraft/potion/PotionEffect" },
			{ "getPotionID", "getPotionID" },
			{ "getDuration", "getDuration" },
			{ "getAmplifier", "getAmplifier" },
			{ "limbSwing", "limbSwing" },
			{ "limbSwingAmount", "limbSwingAmount" },
			{ "prevLimbSwingAmount", "prevLimbSwingAmount" },
			{ "metadata", "itemDamage" },
			{ "getItemDamage", "getItemDamage" },
			{ "entityHit", "entityHit" },
			{ "entityId", "entityId" },
			{ "entityUniqueID", "entityUniqueID" },
			{ "ticksExisted", "ticksExisted" },
			{ "gameProfile", "gameProfile" },
			{ "com/mojang/authlib/GameProfile", "com/mojang/authlib/GameProfile" },
			{ "playerInfoMap", "playerInfoMap" },
			{ "setAngles", "setAngles" },
			{ "mouseSensitivity", "mouseSensitivity" },
			{ "net/minecraft/client/multiplayer/PlayerControllerMP", "net/minecraft/client/multiplayer/PlayerControllerMP" },
			{ "net/minecraft/inventory/Container", "net/minecraft/inventory/Container" },
			{ "net/minecraft/inventory/Slot", "net/minecraft/inventory/Slot" },
			{ "net/minecraft/item/ItemSoup", "net/minecraft/item/ItemSoup" },
			{ "net/minecraft/client/gui/inventory/GuiContainer", "net/minecraft/client/gui/inventory/GuiContainer" },
			{ "playerController", "playerController" },
			{ "curBlockDamageMP", "curBlockDamageMP" },
			{ "blockHitDelay", "blockHitDelay" },
			{ "isHittingBlock", "isHittingBlock" },
			{ "resetBlockRemoving", "resetBlockRemoving" },
			{ "currentBlock", "currentBlock" },
			{ "net/minecraft/util/EnumFacing", "net/minecraft/util/EnumFacing" },
			{ "windowClick", "windowClick" },
			{ "openContainer", "openContainer" },
			{ "windowId", "windowId" },
			{ "closeScreen", "closeScreen" },
			{ "keyBindInventory", "keyBindInventory" },
			{ "pressTime", "pressTime" },
			{ "guiScale", "guiScale" },
			{ "renderEngine", "renderEngine" },
			{ "bindTexture", "bindTexture" },
			{ "net/minecraft/util/ResourceLocation", "net/minecraft/util/ResourceLocation" },
			{ "net/minecraft/client/renderer/texture/TextureManager", "net/minecraft/client/renderer/texture/TextureManager" },
			{ "fullscreen", "fullscreen" },
			{ "guiLeft", "guiLeft" },
			{ "guiTop", "guiTop" },
			{ "xDisplayPosition", "xDisplayPosition" },
			{ "yDisplayPosition", "yDisplayPosition" },
			{ "inventorySlots", "inventorySlots" },
			{ "isUsingItem", "isUsingItem" },
			{ "itemInUseCount", "itemInUseCount" },
			{ "getEyeHeight", "getEyeHeight" },
			{ "rayTraceBlocks", "rayTraceBlocks" },
			{ "net/minecraft/util/Vec3", "net/minecraft/util/Vec3" },
			{ "hitVec", "hitVec" },
			{ "createVectorHelper", "createVectorHelper" },
			{ "net/minecraft/client/renderer/entity/RenderItem", "net/minecraft/client/renderer/entity/RenderItem" },
			{ "renderItemAndEffectIntoGUI", "renderItemAndEffectIntoGUI" },
			{ "renderItemOverlayIntoGUI", "renderItemOverlayIntoGUI" },
			{ "getRenderItem", "getRenderItem" },
			{ "mcRenderItem", "renderItem" },
			{ "getEquipmentInSlot", "getEquipmentInSlot" },
			{ "getCurrentArmor", "getCurrentArmor" },
			{ "getEnchantmentTagList", "getEnchantmentTagList" },
			{ "net/minecraft/nbt/NBTTagList", "net/minecraft/nbt/NBTTagList" },
			{ "net/minecraft/nbt/NBTTagCompound", "net/minecraft/nbt/NBTTagCompound" },
			{ "tagCount", "tagCount" },
			{ "getCompoundTagAt", "getCompoundTagAt" },
			{ "net/minecraft/world/World", "net/minecraft/world/World" },
		};
		if (version == LUNAR_1_8_9) {
			g_Mappings["net/minecraft/client/entity/EntityClientPlayerMP"] = "net/minecraft/client/entity/EntityPlayerSP";
			g_Mappings["net/minecraft/client/entity/EntityPlayerSP"] = "net/minecraft/client/entity/EntityPlayerSP";
			g_Mappings["getDisplayName"] = "getDisplayName";
			g_Mappings["item"] = "item";
			g_Mappings["itemStackDisplayName"] = "getDisplayName";
			g_Mappings["PROJECTION"] = "PROJECTION";
			g_Mappings["MODELVIEW"] = "MODELVIEW";
			g_Mappings["fontRendererObj"] = "fontRendererObj";
			g_Mappings["playerInfoMap"] = "playerInfoMap";
		} else {
			g_Mappings["net/minecraft/client/entity/EntityPlayerSP"] = "net/minecraft/client/entity/EntityPlayerSP";
			g_Mappings["fontRendererObj"] = "fontRendererObj";
			g_Mappings["playerInfoMap"] = "playerInfoMap";
			g_Mappings["playerInfoList"] = "playerInfoList";
		}
		break;
	}
	default:
		break;
	}
}

/*
	* 1: classic class/field
	* 2: class type
	* 3: method class type
*/
std::string Mapper::Get(const char* mapping, int type)
{
	if (!g_Mappings.count(mapping))
		return std::string("");

	std::string ret;
	switch (type)
	{
	case 1:
		ret = g_Mappings[mapping];
		break;
	case 2:
		ret = "L" + std::string(g_Mappings[mapping]) + ";";
		break;
	case 3:
		ret = "()L" + std::string(g_Mappings[mapping]) + ";";
		break;
	default:
		ret = g_Mappings[mapping];
		break;
	}

	return ret;
}

bool Mapper::IsCheatBreaker()
{
	return g_UseCb;
}

std::string Mapper::RemapSignature(const char* sig)
{
	if (!g_UseCb || !sig || !sig[0])
		return sig ? std::string(sig) : std::string();
	std::string in(sig);
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ) {
		if (in[i] == 'L') {
			size_t sc = in.find(';', i);
			if (sc == std::string::npos) {
				out.append(in.substr(i));
				break;
			}
			std::string path = in.substr(i + 1, sc - i - 1);
			std::string mapped;
			auto it = g_Mappings.find(path);
			if (it != g_Mappings.end())
				mapped = it->second;
			else {
				size_t slash = path.rfind('/');
				std::string simple = (slash == std::string::npos) ? path : path.substr(slash + 1);
				auto it2 = g_CbSimple.find(simple);
				if (it2 != g_CbSimple.end())
					mapped = it2->second;
			}
			out.push_back('L');
			out.append(mapped.empty() ? path : mapped);
			out.push_back(';');
			i = sc + 1;
		}
		else {
			out.push_back(in[i++]);
		}
	}
	return out;
}
