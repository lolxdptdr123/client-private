using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

class GenCbMappings
{
    static readonly Dictionary<string, string> Pref = new Dictionary<string, string>
    {
        ["theMinecraft"] = "Minecraft", ["thePlayer"] = "Minecraft", ["theWorld"] = "Minecraft",
        ["currentScreen"] = "Minecraft", ["objectMouseOver"] = "Minecraft", ["pointedEntity"] = "Minecraft",
        ["gameSettings"] = "Minecraft", ["timer"] = "Minecraft", ["fontRendererObj"] = "Minecraft", ["fontRenderer"] = "Minecraft",
        ["displayWidth"] = "Minecraft", ["displayHeight"] = "Minecraft", ["rightClickDelayTimer"] = "Minecraft",
        ["rightClickMouse"] = "Minecraft", ["renderManager"] = "Minecraft", ["playerController"] = "Minecraft",
        ["renderEngine"] = "Minecraft", ["fullscreen"] = "Minecraft", ["getRenderItem"] = "Minecraft", ["renderItem"] = "Minecraft",
        ["item"] = "ItemStack", ["theItem"] = "ItemStack", ["getItem"] = "ItemStack", ["stackSize"] = "ItemStack",
        ["isItemEnchanted"] = "ItemStack", ["itemDamage"] = "ItemStack", ["metadata"] = "ItemStack",
        ["getItemDamage"] = "ItemStack", ["getEnchantmentTagList"] = "ItemStack", ["getDisplayName"] = "ItemStack",
        ["itemStackDisplayName"] = "ItemStack", ["getStackInSlot"] = "InventoryPlayer", ["currentItem"] = "InventoryPlayer",
        ["mainInventory"] = "InventoryPlayer", ["armorInventory"] = "InventoryPlayer", ["inventory"] = "EntityPlayer",
        ["hurtTime"] = "EntityLivingBase", ["hurtResistantTime"] = "EntityLivingBase", ["maxHurtResistantTime"] = "EntityLivingBase",
        ["getHealth"] = "EntityLivingBase", ["getActivePotionEffects"] = "EntityLivingBase", ["activePotionsMap"] = "EntityLivingBase",
        ["isPotionActive"] = "EntityLivingBase", ["jumpTicks"] = "EntityLivingBase", ["limbSwing"] = "EntityLivingBase",
        ["limbSwingAmount"] = "EntityLivingBase", ["prevLimbSwingAmount"] = "EntityLivingBase", ["rotationYawHead"] = "EntityLivingBase",
        ["renderYawOffset"] = "EntityLivingBase", ["prevRenderYawOffset"] = "EntityLivingBase",
        ["getEquipmentInSlot"] = "EntityLivingBase", ["getCurrentArmor"] = "EntityLivingBase",
        ["posX"] = "Entity", ["posY"] = "Entity", ["posZ"] = "Entity", ["prevPosX"] = "Entity", ["prevPosY"] = "Entity", ["prevPosZ"] = "Entity",
        ["lastTickPosX"] = "Entity", ["lastTickPosY"] = "Entity", ["lastTickPosZ"] = "Entity",
        ["motionX"] = "Entity", ["motionY"] = "Entity", ["motionZ"] = "Entity",
        ["rotationYaw"] = "Entity", ["rotationPitch"] = "Entity", ["prevRotationYaw"] = "Entity", ["prevRotationPitch"] = "Entity",
        ["isDead"] = "Entity", ["onGround"] = "Entity", ["entityId"] = "Entity", ["ticksExisted"] = "Entity", ["boundingBox"] = "Entity",
        ["ridingEntity"] = "Entity", ["isSneaking"] = "Entity", ["isSprinting"] = "Entity", ["setSprinting"] = "Entity",
        ["isInvisible"] = "Entity", ["getEyeHeight"] = "Entity", ["inWater"] = "Entity", ["entityUniqueID"] = "Entity",
        ["setAlwaysRenderNameTag"] = "Entity", ["instance"] = "RenderManager",
        ["renderPosX"] = "RenderManager", ["renderPosY"] = "RenderManager", ["renderPosZ"] = "RenderManager",
        ["viewerPosX"] = "RenderManager", ["viewerPosY"] = "RenderManager", ["viewerPosZ"] = "RenderManager",
        ["getEntityRenderObject"] = "RenderManager", ["renderEntitySimple"] = "RenderManager",
        ["PROJECTION"] = "ActiveRenderInfo", ["projection"] = "ActiveRenderInfo",
        ["MODELVIEW"] = "ActiveRenderInfo", ["modelview"] = "ActiveRenderInfo",
        ["playerEntities"] = "World", ["loadedEntityList"] = "World", ["loadedTileEntityList"] = "World",
        ["getBlock"] = "World", ["rayTraceBlocks"] = "World",
        ["typeOfHit"] = "MovingObjectPosition", ["entityHit"] = "MovingObjectPosition",
        ["blockPos"] = "MovingObjectPosition", ["hitVec"] = "MovingObjectPosition",
        ["getPotionID"] = "PotionEffect", ["getDuration"] = "PotionEffect", ["getAmplifier"] = "PotionEffect",
        ["sendQueue"] = "EntityClientPlayerMP", ["movementInput"] = "EntityPlayerSP",
        ["moveForward"] = "MovementInput", ["moveStrafing"] = "MovementInput", ["moveStrafe"] = "MovementInput",
        ["keyCode"] = "KeyBinding", ["pressed"] = "KeyBinding", ["pressTime"] = "KeyBinding",
    };

    static readonly Dictionary<string, string[]> Aliases = new Dictionary<string, string[]>
    {
        ["itemStackDisplayName"] = new[] { "getDisplayName" },
        ["renderManagerInstance"] = new[] { "instance" },
        ["mcRenderItem"] = new[] { "renderItem" },
        ["metadata"] = new[] { "itemDamage", "metadata" },
        ["item"] = new[] { "item", "theItem" },
        ["PROJECTION"] = new[] { "PROJECTION", "projection" },
        ["MODELVIEW"] = new[] { "MODELVIEW", "modelview" },
        ["getDisplayName"] = new[] { "getDisplayName", "getFormattedCommandSenderName" },
        ["fontRendererObj"] = new[] { "fontRendererObj", "fontRenderer" },
        ["playerInfoMap"] = new[] { "playerInfoMap", "playerInfoList" },
    };

    static Dictionary<string, object> Load(string path)
    {
        var ser = new JavaScriptSerializer { MaxJsonLength = int.MaxValue, RecursionLimit = 100 };
        return (Dictionary<string, object>)ser.DeserializeObject(File.ReadAllText(path));
    }

    static string MemberName(Dictionary<string, object> data, string mcp, string prefer)
    {
        string TryCls(string clsKey)
        {
            if (clsKey == null || !data.ContainsKey(clsKey)) return null;
            var cls = data[clsKey] as Dictionary<string, object>;
            if (cls == null) return null;
            foreach (var bucket in new[] { "fields", "methods" })
            {
                if (!cls.ContainsKey(bucket)) continue;
                var bag = cls[bucket] as Dictionary<string, object>;
                if (bag == null || !bag.ContainsKey(mcp)) continue;
                var ent = bag[mcp] as Dictionary<string, object>;
                if (ent != null && ent.ContainsKey("name"))
                {
                    var n = ent["name"] as string;
                    if (!string.IsNullOrEmpty(n)) return n;
                }
            }
            return null;
        }
        var hit = TryCls(prefer);
        if (hit != null) return hit;
        var names = new HashSet<string>();
        string first = null;
        foreach (var kv in data)
        {
            var n = TryCls(kv.Key);
            if (n == null) continue;
            if (first == null) first = n;
            names.Add(n);
        }
        if (names.Count == 1) return first;
        return first;
    }

    static string Resolve(Dictionary<string, object> data, string key)
    {
        if (key.Contains("/"))
        {
            var sn = key.Split('/').Last();
            if (data.ContainsKey(sn))
            {
                var cls = data[sn] as Dictionary<string, object>;
                if (cls != null && cls.ContainsKey("className"))
                    return cls["className"] as string;
            }
            return null;
        }
        var tryNames = new List<string> { key };
        if (Aliases.ContainsKey(key)) tryNames.AddRange(Aliases[key]);
        Pref.TryGetValue(key, out var prefer);
        foreach (var n in tryNames)
        {
            string p = prefer;
            if (p == null) Pref.TryGetValue(n, out p);
            var got = MemberName(data, n, p);
            if (got != null) return got;
        }
        if (data.ContainsKey(key))
        {
            var cls = data[key] as Dictionary<string, object>;
            if (cls != null && cls.ContainsKey("className"))
                return cls["className"] as string;
        }
        return null;
    }

    static string ClassNameOf(Dictionary<string, object> data, string simple)
    {
        if (!data.ContainsKey(simple)) return "";
        var cls = data[simple] as Dictionary<string, object>;
        if (cls == null || !cls.ContainsKey("className")) return "";
        return cls["className"] as string ?? "";
    }

    static void Emit(string version, string fnMap, string fnCls, string outPath, List<string> keys, string jsonPath)
    {
        Console.WriteLine("Loading " + version);
        var data = Load(jsonPath);
        var map = new SortedDictionary<string, string>();
        var missing = new List<string>();
        foreach (var key in keys)
        {
            var val = Resolve(data, key);
            if (!string.IsNullOrEmpty(val)) map[key] = val;
            else missing.Add(key);
        }
        var classes = new SortedDictionary<string, string>();
        foreach (var kv in data)
        {
            var cls = kv.Value as Dictionary<string, object>;
            if (cls != null && cls.ContainsKey("className"))
            {
                var cn = cls["className"] as string;
                if (!string.IsNullOrEmpty(cn)) classes[kv.Key] = cn;
            }
        }
        var sp = ClassNameOf(data, "EntityPlayerSP");
        var mp = ClassNameOf(data, "EntityClientPlayerMP");
        var sb = new StringBuilder();
        sb.AppendLine("// Auto-generated from Whip cheatbreaker.json — do not edit.");
        sb.AppendLine("// " + version);
        sb.AppendLine("#pragma once");
        sb.AppendLine("#include <unordered_map>");
        sb.AppendLine("#include <string>");
        sb.AppendLine();
        sb.AppendLine("inline const char* " + fnMap + "_EntityPlayerSP = \"" + sp + "\";");
        sb.AppendLine("inline const char* " + fnMap + "_EntityClientPlayerMP = \"" + mp + "\";");
        sb.AppendLine();
        sb.AppendLine("inline void " + fnMap + "(std::unordered_map<std::string, std::string>& m) {");
        foreach (var kv in map)
            sb.AppendLine("    m[\"" + kv.Key + "\"] = \"" + kv.Value + "\";");
        sb.AppendLine("}");
        sb.AppendLine();
        sb.AppendLine("inline void " + fnCls + "(std::unordered_map<std::string, std::string>& m) {");
        foreach (var kv in classes)
            sb.AppendLine("    m[\"" + kv.Key + "\"] = \"" + kv.Value + "\";");
        sb.AppendLine("}");
        if (missing.Count > 0)
        {
            sb.AppendLine();
            sb.AppendLine("// missing keys:");
            foreach (var k in missing) sb.AppendLine("//   " + k);
        }
        Directory.CreateDirectory(Path.GetDirectoryName(outPath));
        File.WriteAllText(outPath, sb.ToString());
        Console.WriteLine(version + " mapped " + map.Count + "/" + keys.Count + " keys, " + classes.Count + " classes, missing " + missing.Count);
        foreach (var k in missing) Console.WriteLine("  missing " + k);
    }

    static int Main(string[] args)
    {
        var root = args.Length > 0 ? args[0] : @"C:\Users\bipbo\Documents\project";
        var whip = @"c:\Users\bipbo\Downloads\whip-client\WhipBin-master\WhipBin-master\mappings";
        var mapperCpp = Path.Combine(root, @"client\src\Game\Mapper.cpp");
        var outDir = Path.Combine(root, @"client\src\Game\Mappings");
        var cpp = File.ReadAllText(mapperCpp);
        var keys = new List<string>();
        var seen = new HashSet<string>();
        foreach (Match m in Regex.Matches(cpp, @"\{\s*""([^""]+)""\s*,\s*""([^""]+)""\s*\}"))
        {
            if (seen.Add(m.Groups[1].Value)) keys.Add(m.Groups[1].Value);
        }
        foreach (var extra in new[] {
            "net/minecraft/world/World", "fontRenderer", "itemID", "theItem",
            "projection", "modelview", "instance", "getFormattedCommandSenderName" })
        {
            if (seen.Add(extra)) keys.Add(extra);
        }
        Emit("v1_8_9", "MapperFillCB_1_8", "MapperFillCBClasses_1_8",
            Path.Combine(outDir, "CheatBreaker_v1_8_9.inc"), keys, Path.Combine(whip, @"v1_8_9\cheatbreaker.json"));
        Emit("v1_7_10", "MapperFillCB_1_7", "MapperFillCBClasses_1_7",
            Path.Combine(outDir, "CheatBreaker_v1_7_10.inc"), keys, Path.Combine(whip, @"v1_7_10\cheatbreaker.json"));
        return 0;
    }
}
