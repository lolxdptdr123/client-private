#!/usr/bin/env python3
"""Build CheatBreaker mapping tables for this client from Whip JSON."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WHIP = Path(r"c:\Users\bipbo\Downloads\whip-client\WhipBin-master\WhipBin-master\mappings")
MAPPER_CPP = ROOT / "client" / "src" / "Game" / "Mapper.cpp"
OUT_DIR = ROOT / "client" / "src" / "Game" / "Mappings"

PREF_OWNER = {
    "theMinecraft": "Minecraft",
    "thePlayer": "Minecraft",
    "theWorld": "Minecraft",
    "currentScreen": "Minecraft",
    "objectMouseOver": "Minecraft",
    "pointedEntity": "Minecraft",
    "gameSettings": "Minecraft",
    "timer": "Minecraft",
    "fontRendererObj": "Minecraft",
    "fontRenderer": "Minecraft",
    "displayWidth": "Minecraft",
    "displayHeight": "Minecraft",
    "rightClickDelayTimer": "Minecraft",
    "rightClickMouse": "Minecraft",
    "renderManager": "Minecraft",
    "playerController": "Minecraft",
    "renderEngine": "Minecraft",
    "fullscreen": "Minecraft",
    "getRenderItem": "Minecraft",
    "renderItem": "Minecraft",
    "item": "ItemStack",
    "theItem": "ItemStack",
    "getItem": "ItemStack",
    "stackSize": "ItemStack",
    "isItemEnchanted": "ItemStack",
    "itemDamage": "ItemStack",
    "metadata": "ItemStack",
    "getItemDamage": "ItemStack",
    "getEnchantmentTagList": "ItemStack",
    "getDisplayName": "ItemStack",
    "itemStackDisplayName": "ItemStack",
    "getStackInSlot": "InventoryPlayer",
    "currentItem": "InventoryPlayer",
    "mainInventory": "InventoryPlayer",
    "armorInventory": "InventoryPlayer",
    "inventory": "EntityPlayer",
    "hurtTime": "EntityLivingBase",
    "hurtResistantTime": "EntityLivingBase",
    "maxHurtResistantTime": "EntityLivingBase",
    "getHealth": "EntityLivingBase",
    "getActivePotionEffects": "EntityLivingBase",
    "activePotionsMap": "EntityLivingBase",
    "isPotionActive": "EntityLivingBase",
    "jumpTicks": "EntityLivingBase",
    "limbSwing": "EntityLivingBase",
    "limbSwingAmount": "EntityLivingBase",
    "prevLimbSwingAmount": "EntityLivingBase",
    "rotationYawHead": "EntityLivingBase",
    "renderYawOffset": "EntityLivingBase",
    "prevRenderYawOffset": "EntityLivingBase",
    "getEquipmentInSlot": "EntityLivingBase",
    "getCurrentArmor": "EntityLivingBase",
    "posX": "Entity",
    "posY": "Entity",
    "posZ": "Entity",
    "prevPosX": "Entity",
    "prevPosY": "Entity",
    "prevPosZ": "Entity",
    "lastTickPosX": "Entity",
    "lastTickPosY": "Entity",
    "lastTickPosZ": "Entity",
    "motionX": "Entity",
    "motionY": "Entity",
    "motionZ": "Entity",
    "rotationYaw": "Entity",
    "rotationPitch": "Entity",
    "prevRotationYaw": "Entity",
    "prevRotationPitch": "Entity",
    "isDead": "Entity",
    "onGround": "Entity",
    "entityId": "Entity",
    "ticksExisted": "Entity",
    "boundingBox": "Entity",
    "ridingEntity": "Entity",
    "isSneaking": "Entity",
    "isSprinting": "Entity",
    "setSprinting": "Entity",
    "isInvisible": "Entity",
    "getEyeHeight": "Entity",
    "inWater": "Entity",
    "entityUniqueID": "Entity",
    "setAlwaysRenderNameTag": "Entity",
    "instance": "RenderManager",
    "renderPosX": "RenderManager",
    "renderPosY": "RenderManager",
    "renderPosZ": "RenderManager",
    "viewerPosX": "RenderManager",
    "viewerPosY": "RenderManager",
    "viewerPosZ": "RenderManager",
    "getEntityRenderObject": "RenderManager",
    "renderEntitySimple": "RenderManager",
    "PROJECTION": "ActiveRenderInfo",
    "projection": "ActiveRenderInfo",
    "MODELVIEW": "ActiveRenderInfo",
    "modelview": "ActiveRenderInfo",
    "playerEntities": "World",
    "loadedEntityList": "World",
    "loadedTileEntityList": "World",
    "getBlock": "World",
    "rayTraceBlocks": "World",
    "typeOfHit": "MovingObjectPosition",
    "entityHit": "MovingObjectPosition",
    "blockPos": "MovingObjectPosition",
    "hitVec": "MovingObjectPosition",
    "getPotionID": "PotionEffect",
    "getDuration": "PotionEffect",
    "getAmplifier": "PotionEffect",
    "sendQueue": "EntityClientPlayerMP",
    "movementInput": "EntityPlayerSP",
    "moveForward": "MovementInput",
    "moveStrafing": "MovementInput",
    "moveStrafe": "MovementInput",
    "keyCode": "KeyBinding",
    "pressed": "KeyBinding",
    "pressTime": "KeyBinding",
}

ALIASES = {
    "itemStackDisplayName": ["getDisplayName"],
    "renderManagerInstance": ["instance"],
    "mcRenderItem": ["renderItem"],
    "metadata": ["itemDamage", "metadata"],
    "item": ["item", "theItem"],
    "PROJECTION": ["PROJECTION", "projection"],
    "MODELVIEW": ["MODELVIEW", "modelview"],
    "getDisplayName": ["getDisplayName", "getFormattedCommandSenderName"],
    "fontRendererObj": ["fontRendererObj", "fontRenderer"],
    "playerInfoMap": ["playerInfoMap", "playerInfoList"],
}


def load_json(path: Path):
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def parse_mapper_keys(text: str) -> list[str]:
    keys = []
    for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}', text):
        keys.append(m.group(1))
    extra = [
        "net/minecraft/world/World",
        "fontRenderer",
        "itemID",
        "theItem",
        "projection",
        "modelview",
        "instance",
        "getFormattedCommandSenderName",
    ]
    for k in extra:
        if k not in keys:
            keys.append(k)
    # unique preserve order
    seen = set()
    out = []
    for k in keys:
        if k not in seen:
            seen.add(k)
            out.append(k)
    return out


def simple_name(path: str) -> str:
    return path.rsplit("/", 1)[-1]


def lookup_member(data: dict, mcp: str, prefer: str | None):
    if prefer and prefer in data:
        cls = data[prefer]
        for bucket in ("fields", "methods"):
            ent = cls.get(bucket, {}).get(mcp)
            if ent and ent.get("name"):
                return ent["name"], prefer
    hits = []
    for cls_key, cls in data.items():
        for bucket in ("fields", "methods"):
            ent = cls.get(bucket, {}).get(mcp)
            if ent and ent.get("name"):
                hits.append((ent["name"], cls_key))
    if not hits:
        return None, None
    names = {h[0] for h in hits}
    if len(names) == 1:
        return hits[0][0], hits[0][1]
    if prefer:
        for name, owner in hits:
            if owner == prefer:
                return name, owner
    return hits[0][0], hits[0][1]


def resolve_key(data: dict, key: str):
    if "/" in key or "$" in key and key.startswith("net"):
        sn = simple_name(key)
        if sn in data and data[sn].get("className"):
            return data[sn]["className"]
        return None
    names_to_try = [key] + ALIASES.get(key, [])
    prefer = PREF_OWNER.get(key)
    for n in names_to_try:
        name, owner = lookup_member(data, n, prefer or PREF_OWNER.get(n))
        if name:
            return name
    # class simple name used as key
    if key in data and data[key].get("className"):
        return data[key]["className"]
    return None


def emit_cpp(path: Path, fn_map: str, fn_cls: str, mappings: dict, classes: dict, missing: list[str], marker: str):
    lines = [
        f"// Auto-generated from Whip cheatbreaker.json — do not edit.",
        f"// {marker}",
        "#pragma once",
        "#include <unordered_map>",
        "#include <string>",
        "",
        f"inline void {fn_map}(std::unordered_map<std::string, std::string>& m) {{",
    ]
    for k in sorted(mappings):
        v = mappings[k]
        lines.append(f'    m["{k}"] = "{v}";')
    lines.append("}")
    lines.append("")
    lines.append(f"inline void {fn_cls}(std::unordered_map<std::string, std::string>& m) {{")
    for k in sorted(classes):
        v = classes[k]
        if not v:
            continue
        lines.append(f'    m["{k}"] = "{v}";')
    lines.append("}")
    lines.append("")
    if missing:
        lines.append("// missing keys:")
        for k in missing:
            lines.append(f"//   {k}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build(version: str, keys: list[str]):
    data = load_json(WHIP / version / "cheatbreaker.json")
    mappings = {}
    missing = []
    for key in keys:
        val = resolve_key(data, key)
        if val:
            mappings[key] = val
        else:
            missing.append(key)
    classes = {}
    for ck, cd in data.items():
        cn = cd.get("className")
        if cn:
            classes[ck] = cn
    return mappings, classes, missing


def main():
    text = MAPPER_CPP.read_text(encoding="utf-8")
    keys = parse_mapper_keys(text)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for ver, fn_map, fn_cls, out_name in (
        ("v1_8_9", "MapperFillCB_1_8", "MapperFillCBClasses_1_8", "CheatBreaker_v1_8_9.inc"),
        ("v1_7_10", "MapperFillCB_1_7", "MapperFillCBClasses_1_7", "CheatBreaker_v1_7_10.inc"),
    ):
        mappings, classes, missing = build(ver, keys)
        emit_cpp(OUT_DIR / out_name, fn_map, fn_cls, mappings, classes, missing, ver)
        print(f"{ver}: mapped {len(mappings)}/{len(keys)} keys, {len(classes)} classes, missing {len(missing)}")
        for k in missing:
            print(f"  missing {k}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
