#include "pch.h"
#include "Config.h"

#include "Modules/Module.h"
#include "Modules/Menu.h"
#include "Modules/Combat/aimassist.h"
#include "Modules/Combat/clicker.h"
#include "Modules/Combat/velocity.h"
#include "Modules/Combat/Throw.h"
#include "Modules/Combat/KeepSprint.h"
#include "Modules/Combat/Criticals.h"
#include "Modules/Combat/SprintReset.h"
#include "Modules/Combat/LagRange.h"
#include "Modules/Combat/Reach.h"
#include "Modules/Combat/Blink.h"
#include "Modules/Combat/AutoRod.h"
#include "Modules/Combat/AntiBot.h"
#include "Modules/Combat/AutoBlock.h"
#include "Modules/Combat/Backtrack.h"
#include "Modules/Combat/AutoWeapon.h"
#include "Modules/Combat/AutoRefill.h"
#include "Modules/Visuals/arraylist.h"
#include "Modules/Visuals/Chams.h"
#include "Modules/Visuals/Esp.h"
#include "Modules/Visuals/ItemEsp.h"
#include "Modules/Visuals/PlayerEsp.h"
#include "Modules/Visuals/StorageEsp.h"
#include "Modules/Visuals/BlockEsp.h"
#include "Modules/Visuals/Nametag.h"
#include "Modules/Visuals/Tracer.h"
#include "Modules/Visuals/Trajectories.h"
#include "Modules/Visuals/Notifications.h"
#include "Modules/Visuals/Pointers.h"
#include "Modules/Visuals/Indicators.h"
#include "Modules/Visuals/NoHurtCam.h"
#include "Modules/Misc/Friends.h"
#include "Modules/Misc/Enemies.h"
#include "Modules/Misc/NoItemRelease.h"
#include "Modules/Misc/AntiDebuff.h"
#include "Modules/Misc/PingFix.h"
#include "Modules/Misc/RightClicker.h"
#include "Modules/Misc/BowBoost.h"
#include "Modules/Misc/FastPlace.h"
#include "Modules/Misc/FastBreak.h"
#include "Modules/Misc/AutoTool.h"
#include "Modules/Misc/ChestStealer.h"
#include "Modules/Misc/InvManager.h"
#include "Modules/Misc/BridgeAssist.h"
#include "Modules/Misc/BlockIn.h"
#include "Modules/Misc/Clutch.h"
#include "Modules/Misc/TickLocker.h"
#include "Modules/Misc/InvWalk.h"
#include "Modules/Misc/FastStop.h"
#include "Modules/Misc/NoJumpDelay.h"
#include "Modules/Misc/QuickAccel.h"
#include "Modules/Misc/SnapTap.h"
#include "Modules/Misc/Sprint.h"
#include "Modules/Misc/NoSlow.h"
#include "Modules/Misc/Strafe.h"
#include "Modules/Misc/Scroll.h"
#include "Modules/Misc/armor.h"
#include "Modules/Misc/Weapons.h"
#include "Modules/Misc/Armorswitcher.h"

#include <fstream>
#include <map>
#include <sstream>
#include <cctype>
#include <cmath>

namespace {

std::string g_currentId;
std::atomic<bool> g_loading{ false };

std::string ConfigDir() {
    char app[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableA("APPDATA", app, MAX_PATH);
    std::string root = n ? std::string(app) : ".";
    std::string lol = root + "\\lolxd";
    std::string dir = lol + "\\configs";
    CreateDirectoryA(lol.c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

std::string NowStamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string Sanitize(const char* name) {
    std::string o;
    for (const char* p = name; p && *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_') o += (char)c;
        else if (c == ' ') o += '_';
    }
    if (o.empty()) o = "config";
    if (o.size() > 48) o.resize(48);
    return o;
}

std::string JsonEsc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else o += (char)c;
    }
    return o;
}

struct J {
    enum Type { NUL, B, N, S, A, O } t = NUL;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<J> a;
    std::map<std::string, J> o;
    static J Bool(bool v) { J x; x.t = B; x.b = v; return x; }
    static J Num(double v) { J x; x.t = N; x.n = v; return x; }
    static J Str(std::string v) { J x; x.t = S; x.s = std::move(v); return x; }
    static J Arr() { J x; x.t = A; return x; }
    static J Obj() { J x; x.t = O; return x; }
    bool asBool(bool d = false) const { return t == B ? b : d; }
    double asNum(double d = 0) const { return t == N ? n : d; }
    std::string asStr() const { return t == S ? s : std::string{}; }
};

std::string Dump(const J& v, int indent) {
    std::string pad(indent, ' ');
    std::string pad2(indent + 2, ' ');
    switch (v.t) {
    case J::B: return v.b ? "true" : "false";
    case J::N: {
        char buf[64];
        if (std::fabs(v.n - std::round(v.n)) < 1e-6)
            snprintf(buf, sizeof(buf), "%.0f", v.n);
        else
            snprintf(buf, sizeof(buf), "%.6g", v.n);
        return buf;
    }
    case J::S: return std::string("\"") + JsonEsc(v.s) + "\"";
    case J::A: {
        if (v.a.empty()) return "[]";
        std::string o = "[\n";
        for (size_t i = 0; i < v.a.size(); i++) {
            o += pad2 + Dump(v.a[i], indent + 2);
            if (i + 1 < v.a.size()) o += ",";
            o += "\n";
        }
        o += pad + "]";
        return o;
    }
    case J::O: {
        if (v.o.empty()) return "{}";
        std::string o = "{\n";
        size_t i = 0, n = v.o.size();
        for (auto& kv : v.o) {
            o += pad2 + "\"" + JsonEsc(kv.first) + "\": " + Dump(kv.second, indent + 2);
            if (++i < n) o += ",";
            o += "\n";
        }
        o += pad + "}";
        return o;
    }
    default: return "null";
    }
}

struct Parser {
    const char* p;
    void skip() { while (*p && (unsigned char)*p <= 32) ++p; }
    bool eat(char c) { skip(); if (*p == c) { ++p; return true; } return false; }
    J parse() {
        skip();
        if (*p == 't' && strncmp(p, "true", 4) == 0) { p += 4; return J::Bool(true); }
        if (*p == 'f' && strncmp(p, "false", 5) == 0) { p += 5; return J::Bool(false); }
        if (*p == 'n' && strncmp(p, "null", 4) == 0) { p += 4; return J{}; }
        if (*p == '"') {
            ++p;
            std::string s;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) {
                    ++p;
                    if (*p == 'n') s += '\n';
                    else s += *p;
                    ++p;
                } else s += *p++;
            }
            if (*p == '"') ++p;
            return J::Str(std::move(s));
        }
        if (*p == '[') {
            ++p;
            J a = J::Arr();
            skip();
            if (eat(']')) return a;
            for (;;) {
                a.a.push_back(parse());
                skip();
                if (eat(']')) break;
                eat(',');
            }
            return a;
        }
        if (*p == '{') {
            ++p;
            J o = J::Obj();
            skip();
            if (eat('}')) return o;
            for (;;) {
                skip();
                J k = parse();
                eat(':');
                o.o[k.asStr()] = parse();
                skip();
                if (eat('}')) break;
                eat(',');
            }
            return o;
        }
        char* end = nullptr;
        double n = strtod(p, &end);
        if (end != p) { p = end; return J::Num(n); }
        return J{};
    }
};

enum Kind { KB, KI, KF, KC, KS };

struct Bind {
    const char* key;
    Kind kind;
    void* ptr;
    int slen = 0;
};

std::vector<Bind> g_binds;

void BindB(const char* k, bool* p) { g_binds.push_back({ k, KB, p }); }
void BindI(const char* k, int* p) { g_binds.push_back({ k, KI, p }); }
void BindF(const char* k, float* p) { g_binds.push_back({ k, KF, p }); }
void BindC(const char* k, float* p) { g_binds.push_back({ k, KC, p }); }
void BindS(const char* k, char* p, int n) { g_binds.push_back({ k, KS, p, n }); }

void RegisterBinds() {
    if (!g_binds.empty()) return;

    BindI("clicker.cps", &Clicker::cps);
    BindI("clicker.mode", &Clicker::mode);
    BindB("clicker.exhaust", &Clicker::exhaust);
    BindB("clicker.requireClick", &Clicker::requireClick);
    BindB("clicker.weaponsOnly", &Clicker::weaponsOnly);

    BindI("rc.cps", &RightClicker::cps);
    BindB("rc.blatant", &RightClicker::blatant);
    BindB("rc.exhaust", &RightClicker::exhaust);

    BindI("bb.chargeTicks", &BowBoostSettings::chargeTicks);
    BindI("bb.delayMs", &BowBoostSettings::delayMs);
    BindB("bb.switchItem", &BowBoostSettings::switchItem);
    BindB("bb.lookUp", &BowBoostSettings::lookUp);
    BindF("bb.pitch", &BowBoostSettings::pitch);

    BindI("aa.mode", &AimAssistSettings::currentMode);
    BindF("aa.speed", &AimAssistSettings::speed);
    BindF("aa.fovMin", &AimAssistSettings::fovMin);
    BindF("aa.fovMax", &AimAssistSettings::fovMax);
    BindF("aa.distanceMin", &AimAssistSettings::distanceMin);
    BindF("aa.distanceMax", &AimAssistSettings::distanceMax);
    BindI("aa.priority", &AimAssistSettings::priority);
    BindB("aa.targetPlayers", &AimAssistSettings::targetPlayers);
    BindB("aa.allowInvisible", &AimAssistSettings::allowInvisible);
    BindB("aa.allowNaked", &AimAssistSettings::allowNaked);
    BindB("aa.requireClick", &AimAssistSettings::requireClick);
    BindB("aa.weaponsOnly", &AimAssistSettings::weaponsOnly);
    BindB("aa.breakBlock", &AimAssistSettings::breakBlock);
    BindB("aa.keepOnTarget", &AimAssistSettings::keepOnTarget);
    BindI("aa.keepOnTargetKeybind", &AimAssistSettings::keepOnTargetKeybind);
    BindI("aa.multipoint", &AimAssistSettings::multipoint);

    BindI("vel.mode", &VelocitySettings::mode);
    BindF("vel.horizontal", &VelocitySettings::horizontal);
    BindF("vel.vertical", &VelocitySettings::vertical);
    BindF("vel.reverseStrength", &VelocitySettings::reverseStrength);
    BindF("vel.reduceH", &VelocitySettings::reduceH);
    BindB("vel.agcBypass", &VelocitySettings::agcBypass);
    BindI("vel.jumpDelayMs", &VelocitySettings::jumpDelayMs);
    BindI("vel.chance", &VelocitySettings::chance);
    BindB("vel.weaponsOnly", &VelocitySettings::weaponsOnly);
    BindB("vel.onlyWhenMovingForward", &VelocitySettings::onlyWhenMovingForward);
    BindB("vel.onlyLookingAtPlayer", &VelocitySettings::onlyLookingAtPlayer);
    BindB("vel.onlyMousePressed", &VelocitySettings::onlyMousePressed);

    BindB("throw.potEnabled", &ThrowSettings::potEnabled);
    BindF("throw.potSpeed", &ThrowSettings::potSpeed);
    BindB("throw.potSmart", &ThrowSettings::potSmart);
    BindB("throw.potDouble", &ThrowSettings::potDouble);
    BindI("throw.potBind", &ThrowSettings::potBind);
    BindB("throw.soupEnabled", &ThrowSettings::soupEnabled);
    BindF("throw.soupSpeed", &ThrowSettings::soupSpeed);
    BindB("throw.soupSmart", &ThrowSettings::soupSmart);
    BindB("throw.soupDouble", &ThrowSettings::soupDouble);
    BindB("throw.soupAutoDrop", &ThrowSettings::soupAutoDrop);
    BindI("throw.soupBind", &ThrowSettings::soupBind);
    BindB("throw.debuffEnabled", &ThrowSettings::debuffEnabled);
    BindF("throw.debuffSpeed", &ThrowSettings::debuffSpeed);
    BindB("throw.debuffDouble", &ThrowSettings::debuffDouble);
    BindI("throw.debuffBind", &ThrowSettings::debuffBind);
    BindB("throw.pearlEnabled", &ThrowSettings::pearlEnabled);
    BindF("throw.pearlSpeed", &ThrowSettings::pearlSpeed);
    BindI("throw.pearlBind", &ThrowSettings::pearlBind);

    BindI("ks.mode", &KeepSprintSettings::mode);
    BindF("ks.speed", &KeepSprintSettings::speed);
    BindI("ks.chance", &KeepSprintSettings::chance);
    BindB("ks.weaponsOnly", &KeepSprintSettings::weaponsOnly);
    BindB("ks.onlyOnBehind", &KeepSprintSettings::onlyOnBehind);

    BindI("cr.mode", &CriticalsSettings::mode);
    BindI("cr.chance", &CriticalsSettings::chance);
    BindF("cr.timerSpeed", &CriticalsSettings::timerSpeed);
    BindI("cr.maxQueueTime", &CriticalsSettings::maxQueueTime);

    BindI("sr.mode", &SprintResetSettings::mode);
    BindI("sr.delayMs", &SprintResetSettings::delayMs);
    BindI("sr.stopMs", &SprintResetSettings::stopMs);
    BindB("sr.randomize", &SprintResetSettings::randomize);
    BindB("sr.waitForDamage", &SprintResetSettings::waitForDamage);
    BindB("sr.holdingWeapon", &SprintResetSettings::holdingWeapon);

    BindI("lr.mode", &LagRangeSettings::mode);
    BindF("lr.activationDistance", &LagRangeSettings::activationDistance);
    BindF("lr.flushDistance", &LagRangeSettings::flushDistance);
    BindI("lr.delay", &LagRangeSettings::delay);
    BindB("lr.onlyWeapon", &LagRangeSettings::onlyWeapon);
    BindB("lr.onlySprinting", &LagRangeSettings::onlySprinting);
    BindB("lr.drawBox", &LagRangeSettings::drawBox);
    BindC("lr.boxColor", LagRangeSettings::boxColor);
    BindC("lr.outlineColor", LagRangeSettings::outlineColor);

    BindF("rc.distance", &ReachSettings::distance);
    BindI("rc.activateTicks", &ReachSettings::activateTicks);
    BindB("rc.onlySprinting", &ReachSettings::onlySprinting);

    BindF("rod.fov", &AutoRodSettings::fov);
    BindF("rod.maxLookFov", &AutoRodSettings::maxLookFov);
    BindF("rod.maxRange", &AutoRodSettings::maxRange);
    BindI("rod.cooldownMs", &AutoRodSettings::cooldownMs);
    BindB("rod.ignoreEating", &AutoRodSettings::ignoreEating);
    BindB("rod.moveFix", &AutoRodSettings::moveFix);
    BindB("rod.onlyIfNotInReach", &AutoRodSettings::onlyIfNotInReach);
    BindF("rod.meleeReach", &AutoRodSettings::meleeReach);
    BindB("rod.click", &AutoRodSettings::click);
    BindB("rod.rotations", &AutoRodSettings::rotations);
    BindB("rod.luckyThrow", &AutoRodSettings::luckyThrow);
    BindB("rod.targetPlayers", &AutoRodSettings::targetPlayers);
    BindB("rod.targetEnemiesOnly", &AutoRodSettings::targetEnemiesOnly);
    BindB("rod.targetMobs", &AutoRodSettings::targetMobs);
    BindB("rod.esp", &AutoRodSettings::esp);
    BindC("rod.espColor", AutoRodSettings::espColor);

    BindI("ab.minTicks", &AntiBotSettings::minTicks);
    BindB("ab.checkTab", &AntiBotSettings::checkTab);
    BindB("ab.checkPackets", &AntiBotSettings::checkPackets);
    BindI("ab.packetGrace", &AntiBotSettings::packetGrace);

    BindF("abl.range", &AutoBlockSettings::range);
    BindI("abl.maxHurtTimeMs", &AutoBlockSettings::maxHurtTimeMs);
    BindI("abl.maxHoldMs", &AutoBlockSettings::maxHoldMs);
    BindB("abl.forceAnim", &AutoBlockSettings::forceAnim);
    BindB("abl.forceAnimInRange", &AutoBlockSettings::forceAnimInRange);
    BindI("abl.lagChance", &AutoBlockSettings::lagChance);
    BindI("abl.lagMaxMs", &AutoBlockSettings::lagMaxMs);
    BindB("abl.preventDelayAttacks", &AutoBlockSettings::preventDelayAttacks);
    BindB("abl.blockAgainImmediately", &AutoBlockSettings::blockAgainImmediately);
    BindB("abl.condLmb", &AutoBlockSettings::condLmb);
    BindB("abl.condRmb", &AutoBlockSettings::condRmb);
    BindB("abl.condDamaged", &AutoBlockSettings::condDamaged);

    BindI("bt.mode", &BacktrackSettings::mode);
    BindI("bt.delayInTicks", &BacktrackSettings::delayInTicks);
    BindI("bt.cooldown", &BacktrackSettings::cooldown);
    BindB("bt.distanceCheck", &BacktrackSettings::distanceCheck);
    BindF("bt.distance", &BacktrackSettings::distance);
    BindF("bt.distanceMax", &BacktrackSettings::distanceMax);
    BindI("bt.smoothDelayMs", &BacktrackSettings::smoothDelayMs);
    BindI("bt.forceFlushMs", &BacktrackSettings::forceFlushMs);
    BindB("bt.onlySprinting", &BacktrackSettings::onlySprinting);
    BindI("bt.maxDelay", &BacktrackSettings::maxDelay);
    BindI("bt.minDelay", &BacktrackSettings::minDelay);
    BindI("bt.delayBetweenLags", &BacktrackSettings::delayBetweenLags);
    BindI("bt.stopAtHurt", &BacktrackSettings::stopAtHurt);
    BindI("bt.disableOn", &BacktrackSettings::disableOn);
    BindF("bt.stopOnAttackRange", &BacktrackSettings::stopOnAttackRange);
    BindB("bt.onlyWhenNeeded", &BacktrackSettings::onlyWhenNeeded);
    BindB("bt.continueAtHurtTime", &BacktrackSettings::continueAtHurtTime);
    BindB("bt.drawBox", &BacktrackSettings::drawBox);
    BindC("bt.boxColor", BacktrackSettings::boxColor);
    BindC("bt.outlineColor", BacktrackSettings::outlineColor);

    BindI("bl.direction", &BlinkSettings::direction);
    BindI("bl.autoSendDelay", &BlinkSettings::autoSendDelay);
    BindB("bl.disableOnLocalDamage", &BlinkSettings::disableOnLocalDamage);
    BindB("bl.disableOnTargetDamage", &BlinkSettings::disableOnTargetDamage);
    BindB("bl.drawEsp", &BlinkSettings::drawEsp);
    BindC("bl.espColor", BlinkSettings::espColor);

    BindI("aw.activationMs", &AutoWeaponSettings::activationMs);

    BindI("ar.mode", &AutoRefillSettings::mode);
    BindI("ar.itemMode", &AutoRefillSettings::itemMode);
    BindI("ar.speed", &AutoRefillSettings::speed);
    BindB("ar.randomMode", &AutoRefillSettings::randomMode);
    BindB("ar.dynamicSpeed", &AutoRefillSettings::dynamicSpeed);
    BindB("ar.transition", &AutoRefillSettings::transition);

    BindB("al.bgEnabled", &ArrayListSettings::bgEnabled);
    BindC("al.bgColor", ArrayListSettings::bgColor);
    BindC("al.nameColor", ArrayListSettings::nameColor);
    BindC("al.suffixColor", ArrayListSettings::suffixColor);
    BindB("al.barEnabled", &ArrayListSettings::barEnabled);
    BindC("al.barColor", ArrayListSettings::barColor);
    BindF("al.fontSize", &ArrayListSettings::fontSize);
    BindF("al.scrollSpeed", &ArrayListSettings::scrollSpeed);
    BindB("al.showTitle", &ArrayListSettings::showTitle);
    BindS("al.titleText", ArrayListSettings::titleText, 64);
    BindC("al.titleColor", ArrayListSettings::titleColor);

    BindB("ch.players", &ChamsSettings::players);
    BindB("ch.mobs", &ChamsSettings::mobs);
    BindB("ch.animals", &ChamsSettings::animals);
    BindB("ch.villagers", &ChamsSettings::villagers);
    BindB("ch.armorStands", &ChamsSettings::armorStands);
    BindB("ch.invisibles", &ChamsSettings::invisibles);
    BindB("ch.renderTexture", &ChamsSettings::renderTexture);
    BindB("ch.glowMode", &ChamsSettings::glowMode);
    BindB("ch.hideFriends", &ChamsSettings::hideFriends);
    BindB("ch.enemiesOnly", &ChamsSettings::enemiesOnly);
    BindC("ch.colorNeutral", ChamsSettings::colorNeutral);
    BindC("ch.colorFriend", ChamsSettings::colorFriend);
    BindC("ch.colorEnemy", ChamsSettings::colorEnemy);

    BindI("esp.renderMode", &EspSettings::renderMode);
    BindI("esp.mode3d", &EspSettings::mode3d);
    BindI("esp.mode2d", &EspSettings::mode2d);
    BindB("esp.showHealthBar", &EspSettings::showHealthBar);
    BindB("esp.hideFriends", &EspSettings::hideFriends);
    BindB("esp.enemiesOnly", &EspSettings::enemiesOnly);
    BindF("esp.maxRenderDistance", &EspSettings::maxRenderDistance);
    BindC("esp.outline3dColor", EspSettings::outline3dColor);
    BindC("esp.fill3dColor", EspSettings::fill3dColor);
    BindF("esp.fill3dOpacity", &EspSettings::fill3dOpacity);
    BindC("esp.outline2dColor", EspSettings::outline2dColor);
    BindC("esp.fill2dColor", EspSettings::fill2dColor);
    BindC("esp.friendColor", EspSettings::friendColor);
    BindC("esp.enemyColor", EspSettings::enemyColor);
    BindC("esp.neutralColor", EspSettings::neutralColor);
    BindC("esp.healthBarBg", EspSettings::healthBarBg);
    BindC("esp.healthBarFull", EspSettings::healthBarFull);
    BindC("esp.healthBarLow", EspSettings::healthBarLow);
    BindF("esp.outline3dWidth", &EspSettings::outline3dWidth);
    BindF("esp.outline2dWidth", &EspSettings::outline2dWidth);
    BindF("esp.healthBarWidth", &EspSettings::healthBarWidth);
    BindF("esp.healthBarOffset", &EspSettings::healthBarOffset);

    BindC("iesp.color", ItemEspSettings::color);
    BindF("iesp.maxDistance", &ItemEspSettings::maxDistance);

    BindB("pesp.armor", &PlayerEspSettings::armor);
    BindB("pesp.potions", &PlayerEspSettings::potions);
    BindB("pesp.heldItem", &PlayerEspSettings::heldItem);
    BindB("pesp.skeleton", &PlayerEspSettings::skeleton);
    BindB("pesp.outline", &PlayerEspSettings::outline);
    BindB("pesp.gapple", &PlayerEspSettings::gapple);
    BindB("pesp.hideFriends", &PlayerEspSettings::hideFriends);
    BindB("pesp.enemiesOnly", &PlayerEspSettings::enemiesOnly);
    BindI("pesp.outlineMode", &PlayerEspSettings::outlineMode);
    BindB("pesp.outlineGlow", &PlayerEspSettings::outlineGlow);
    BindF("pesp.displayScale", &PlayerEspSettings::displayScale);
    BindF("pesp.skeletonThickness", &PlayerEspSettings::skeletonThickness);
    BindF("pesp.outlineThickness", &PlayerEspSettings::outlineThickness);
    BindF("pesp.outlineGlowRadius", &PlayerEspSettings::outlineGlowRadius);
    BindC("pesp.skeletonColor", PlayerEspSettings::skeletonColor);
    BindC("pesp.outlineColor", PlayerEspSettings::outlineColor);
    BindF("pesp.maxRenderDistance", &PlayerEspSettings::maxRenderDistance);

    BindI("sesp.renderMode", &StorageEspSettings::renderMode);
    BindI("sesp.mode3d", &StorageEspSettings::mode3d);
    BindI("sesp.mode2d", &StorageEspSettings::mode2d);
    BindB("sesp.showLabels", &StorageEspSettings::showLabels);
    BindF("sesp.maxDistance", &StorageEspSettings::maxDistance);
    BindF("sesp.outline3dWidth", &StorageEspSettings::outline3dWidth);
    BindF("sesp.outline2dWidth", &StorageEspSettings::outline2dWidth);
    BindF("sesp.fillAlpha3d", &StorageEspSettings::fillAlpha3d);
    BindF("sesp.fillAlpha2d", &StorageEspSettings::fillAlpha2d);
    BindF("sesp.labelScale", &StorageEspSettings::labelScale);
    BindB("sesp.chest", &StorageEspSettings::chest);
    BindB("sesp.enderChest", &StorageEspSettings::enderChest);
    BindB("sesp.furnace", &StorageEspSettings::furnace);
    BindB("sesp.dispenser", &StorageEspSettings::dispenser);
    BindB("sesp.dropper", &StorageEspSettings::dropper);
    BindB("sesp.hopper", &StorageEspSettings::hopper);
    BindC("sesp.chestColor", StorageEspSettings::chestColor);
    BindC("sesp.enderChestColor", StorageEspSettings::enderChestColor);
    BindC("sesp.furnaceColor", StorageEspSettings::furnaceColor);
    BindC("sesp.dispenserColor", StorageEspSettings::dispenserColor);
    BindC("sesp.dropperColor", StorageEspSettings::dropperColor);
    BindC("sesp.hopperColor", StorageEspSettings::hopperColor);
    BindC("sesp.labelColor", StorageEspSettings::labelColor);

    BindI("besp.rangeChunks", &BlockEspSettings::rangeChunks);
    BindI("besp.limitPerChunk", &BlockEspSettings::limitPerChunk);
    BindF("besp.outlineWidth", &BlockEspSettings::outlineWidth);
    for (int i = 0; i < 32; i++) {
        static char ik[32][24];
        static char ck[32][24];
        snprintf(ik[i], 24, "besp.id.%d", i);
        snprintf(ck[i], 24, "besp.col.%d", i);
        BindI(ik[i], &BlockEspSettings::ids[i]);
        BindC(ck[i], BlockEspSettings::colors[i]);
    }

    BindB("nt.showNames", &NametagSettings::showNames);
    BindB("nt.showHealth", &NametagSettings::showHealth);
    BindB("nt.showDistance", &NametagSettings::showDistance);
    BindB("nt.showBackground", &NametagSettings::showBackground);
    BindB("nt.showHealthBar", &NametagSettings::showHealthBar);
    BindB("nt.showOutline", &NametagSettings::showOutline);
    BindB("nt.hideFriends", &NametagSettings::hideFriends);
    BindB("nt.enemiesOnly", &NametagSettings::enemiesOnly);
    BindF("nt.nametagScale", &NametagSettings::nametagScale);
    BindF("nt.textSize", &NametagSettings::textSize);
    BindF("nt.outlineThickness", &NametagSettings::outlineThickness);
    BindF("nt.maxRenderDistance", &NametagSettings::maxRenderDistance);
    BindF("nt.healthBarWidth", &NametagSettings::healthBarWidth);
    BindF("nt.healthBarHeight", &NametagSettings::healthBarHeight);
    BindC("nt.friendColor", NametagSettings::friendColor);
    BindC("nt.enemyColor", NametagSettings::enemyColor);
    BindC("nt.neutralColor", NametagSettings::neutralColor);
    BindC("nt.nameColor", NametagSettings::nameColor);
    BindC("nt.distanceColor", NametagSettings::distanceColor);
    BindC("nt.backgroundColor", NametagSettings::backgroundColor);
    BindC("nt.healthBarBg", NametagSettings::healthBarBg);
    BindC("nt.healthBarFull", NametagSettings::healthBarFull);
    BindC("nt.healthBarLow", NametagSettings::healthBarLow);
    BindC("nt.outlineColor", NametagSettings::outlineColor);

    BindB("tr.hideFriends", &TracerSettings::hideFriends);
    BindB("tr.enemiesOnly", &TracerSettings::enemiesOnly);
    BindF("tr.maxRenderDistance", &TracerSettings::maxRenderDistance);
    BindF("tr.tracerWidth", &TracerSettings::tracerWidth);
    BindC("tr.tracerColor", TracerSettings::tracerColor);
    BindC("tr.friendColor", TracerSettings::friendColor);
    BindC("tr.enemyColor", TracerSettings::enemyColor);

    BindB("tj.bow", &TrajectoriesSettings::bow);
    BindB("tj.potion", &TrajectoriesSettings::potion);
    BindB("tj.pearl", &TrajectoriesSettings::pearl);
    BindB("tj.snowball", &TrajectoriesSettings::snowball);
    BindB("tj.egg", &TrajectoriesSettings::egg);
    BindB("tj.rod", &TrajectoriesSettings::rod);
    BindF("tj.lineWidth", &TrajectoriesSettings::lineWidth);
    BindC("tj.arcColor", TrajectoriesSettings::arcColor);

    BindF("ptr.range", &PointersSettings::range);
    BindF("ptr.ignoreFov", &PointersSettings::ignoreFov);
    BindB("ptr.hideFriendlies", &PointersSettings::hideFriendlies);
    BindI("ptr.style", &PointersSettings::style);
    BindI("ptr.colorMode", &PointersSettings::colorMode);
    BindC("ptr.nearColor", PointersSettings::nearColor);
    BindC("ptr.farColor", PointersSettings::farColor);
    BindC("ptr.enemyColor", PointersSettings::enemyColor);
    BindC("ptr.friendColor", PointersSettings::friendColor);
    BindF("ptr.nearDist", &PointersSettings::nearDist);
    BindF("ptr.farDist", &PointersSettings::farDist);
    BindF("ptr.scale", &PointersSettings::scale);
    BindF("ptr.radius", &PointersSettings::radius);
    BindB("ptr.distanceRadius", &PointersSettings::distanceRadius);

    BindB("ind.fireballs", &IndicatorsSettings::fireballs);
    BindB("ind.pearls", &IndicatorsSettings::pearls);
    BindB("ind.arrows", &IndicatorsSettings::arrows);
    BindB("ind.comingCloser", &IndicatorsSettings::comingCloser);

    BindB("notif.hideIfInGame", &NotificationSettings::hideIfInGame);
    BindB("notif.hideIfHoldBind", &NotificationSettings::hideIfHoldBind);
    BindB("notif.catCombat", &NotificationSettings::catCombat);
    BindB("notif.catVisual", &NotificationSettings::catVisual);
    BindB("notif.catUtility", &NotificationSettings::catUtility);
    BindB("notif.catBlocks", &NotificationSettings::catBlocks);
    BindF("notif.duration", &NotificationSettings::duration);
    BindF("notif.animSpeed", &NotificationSettings::animSpeed);

    BindI("fr.addFriendKey", &FriendsSettings::addFriendKey);
    BindI("fr.addNearbyKey", &FriendsSettings::addNearbyKey);
    BindI("fr.clearFriendsKey", &FriendsSettings::clearFriendsKey);
    BindF("fr.nearbyRadius", &FriendsSettings::nearbyRadius);

    BindI("en.addEnemyKey", &EnemiesSettings::addEnemyKey);
    BindI("en.addNearbyKey", &EnemiesSettings::addNearbyKey);
    BindI("en.clearEnemiesKey", &EnemiesSettings::clearEnemiesKey);
    BindF("en.nearbyRadius", &EnemiesSettings::nearbyRadius);

    BindI("fp.mode", &FastPlaceSettings::mode);
    BindI("fp.tickDelay", &FastPlaceSettings::tickDelay);
    BindB("fp.onlyBlock", &FastPlaceSettings::onlyBlock);
    BindF("fp.average", &FastPlaceSettings::average);
    BindB("fp.holdToClick", &FastPlaceSettings::holdToClick);
    BindB("fp.exhaust", &FastPlaceSettings::exhaust);

    BindI("fb.mode", &FastBreakSettings::mode);
    BindF("fb.power", &FastBreakSettings::power);
    BindF("fb.multiplier", &FastBreakSettings::multiplier);

    BindB("at.switchBack", &AutoToolSettings::switchBack);
    BindB("at.onlyMining", &AutoToolSettings::onlyMining);
    BindB("at.preferSilk", &AutoToolSettings::preferSilk);
    BindI("at.delayMs", &AutoToolSettings::delayMs);

    BindI("cs.delayMin", &ChestStealerSettings::delayMin);
    BindI("cs.delayMax", &ChestStealerSettings::delayMax);
    BindI("cs.firstDelay", &ChestStealerSettings::firstDelay);
    BindI("cs.closeDelay", &ChestStealerSettings::closeDelay);
    BindB("cs.autoClose", &ChestStealerSettings::autoClose);
    BindB("cs.nameCheck", &ChestStealerSettings::nameCheck);
    BindB("cs.randomize", &ChestStealerSettings::randomize);
    BindB("cs.intelligent", &ChestStealerSettings::intelligent);

    BindI("im.delayAfterOpen", &InvManagerSettings::delayAfterOpen);
    BindI("im.speed", &InvManagerSettings::speed);
    BindB("im.smartSpeed", &InvManagerSettings::smartSpeed);
    BindB("im.randomize", &InvManagerSettings::randomize);
    BindB("im.equipArmor", &InvManagerSettings::equipArmor);
    BindB("im.sortHotbar", &InvManagerSettings::sortHotbar);
    BindB("im.smartFallbacks", &InvManagerSettings::smartFallbacks);
    for (int i = 0; i < 9; i++) {
        static char keys[9][24];
        snprintf(keys[i], 24, "im.hotbar.%d", i);
        BindI(keys[i], &InvManagerSettings::hotbar[i]);
    }

    BindF("ba.edgeOffset", &BridgeAssistSettings::edgeOffset);
    BindI("ba.unsneakDelay", &BridgeAssistSettings::unsneakDelay);
    BindF("ba.pitch", &BridgeAssistSettings::pitch);
    BindB("ba.onlyBlocks", &BridgeAssistSettings::onlyBlocks);
    BindB("ba.lookingDown", &BridgeAssistSettings::lookingDown);
    BindB("ba.sneakOnJump", &BridgeAssistSettings::sneakOnJump);

    BindF("bi.speed", &BlockInSettings::speed);
    BindB("bi.onlyOnGround", &BlockInSettings::onlyOnGround);

    BindI("cl.range", &ClutchSettings::range);
    BindF("cl.fov", &ClutchSettings::fov);
    BindI("cl.minHeight", &ClutchSettings::minHeight);
    BindF("cl.clickSpeed", &ClutchSettings::clickSpeed);
    BindF("cl.randomization", &ClutchSettings::randomization);
    BindI("cl.selectBlocks", &ClutchSettings::selectBlocks);
    BindB("cl.onlySideways", &ClutchSettings::onlySideways);
    BindF("cl.baseSpeed", &ClutchSettings::baseSpeed);
    BindF("cl.acceleration", &ClutchSettings::acceleration);
    BindF("cl.accelStrength", &ClutchSettings::accelStrength);
    BindB("cl.multipoint", &ClutchSettings::multipoint);
    BindI("cl.snapDelay", &ClutchSettings::snapDelay);
    BindI("cl.snapDuration", &ClutchSettings::snapDuration);
    BindB("cl.keepJumpDir", &ClutchSettings::keepJumpDir);
    BindB("cl.disableAfter", &ClutchSettings::disableAfter);
    BindB("cl.midAir", &ClutchSettings::midAir);
    BindB("cl.onHurt", &ClutchSettings::onHurt);
    BindB("cl.backwards", &ClutchSettings::backwards);

    BindB("tl.renderSelectedBlock", &TickLockerSettings::renderSelectedBlock);
    BindB("tl.showOutline", &TickLockerSettings::showOutline);
    BindB("tl.showFill", &TickLockerSettings::showFill);
    BindF("tl.outlineWidth", &TickLockerSettings::outlineWidth);
    BindC("tl.outlineColor", TickLockerSettings::outlineColor);
    BindC("tl.fillColor", TickLockerSettings::fillColor);
    BindI("tl.targetKey", &TickLockerSettings::targetKey);

    BindI("iw.mode", &InvWalkSettings::mode);
    BindI("fs.axis", &FastStopSettings::axis);
    BindB("fs.disableOnSneak", &FastStopSettings::disableOnSneak);
    BindB("qa.disableOnSneak", &QuickAccelSettings::disableOnSneak);
    BindI("st.axis", &SnapTapSettings::axis);
    BindB("st.onlyOnGround", &SnapTapSettings::onlyOnGround);
    BindB("st.disableOnSneak", &SnapTapSettings::disableOnSneak);

    BindB("sp.usingItem", &SprintSettings::usingItem);
    BindB("sp.backwards", &SprintSettings::backwards);
    BindB("sp.sideways", &SprintSettings::sideways);
    BindB("sp.inInventory", &SprintSettings::inInventory);

    BindI("ns.swords", &NoSlowSettings::swords);
    BindI("ns.bows", &NoSlowSettings::bows);
    BindI("ns.consumables", &NoSlowSettings::consumables);

    BindI("nir.mode", &NoItemReleaseSettings::mode);
    BindB("ad.blindness", &AntiDebuffSettings::blindness);
    BindB("ad.nausea", &AntiDebuffSettings::nausea);

    BindI("sf.onGround", &StrafeSettings::onGround);
    BindI("sf.inAir", &StrafeSettings::inAir);
    BindI("sf.onJump", &StrafeSettings::onJump);
    BindI("sf.maxHurtTime", &StrafeSettings::maxHurtTime);
    BindB("sf.holdingWeapon", &StrafeSettings::holdingWeapon);

    BindI("scroll.bind", &Scroll::scroll_bind);
    BindI("scroll.delay", &Scroll::scrollDelay);
    BindI("scroll.swordSlot", &Scroll::swordSlot);
    for (int i = 0; i < Scroll::ITEM_COUNT; i++) {
        static char keys[Scroll::ITEM_COUNT][32];
        snprintf(keys[i], 32, "scroll.wl.%d", i);
        BindB(keys[i], &Scroll::g_whitelistEnabled[i]);
    }
    for (int i = 0; i < 9; i++) {
        static char keys[9][32];
        snprintf(keys[i], 32, "scroll.hotbar.%d", i);
        BindI(keys[i], &Scroll::hotbarKeys[i]);
    }

    BindI("armor.bind", &Armor::bind);
    BindI("armor.speed", &Armor::speed);
    for (int p = 0; p < 4; p++) {
        for (int s = 0; s < 8; s++) {
            static char keys[32][32];
            int idx = p * 8 + s;
            snprintf(keys[idx], 32, "armor.id.%d.%d", p, s);
            BindI(keys[idx], &Armor::armorIds[p][s]);
        }
    }

    BindB("wp.fist", &WeaponsSettings::fist);
    BindB("wp.swords", &WeaponsSettings::swords);
    BindB("wp.axes", &WeaponsSettings::axes);
    BindB("wp.sharpness", &WeaponsSettings::sharpness);
    BindB("wp.knockback", &WeaponsSettings::knockback);
    BindB("wp.fireAspect", &WeaponsSettings::fireAspect);
    for (int i = 0; i < 32; i++) {
        static char ek[32][24];
        snprintf(ek[i], 24, "wp.extra.%d", i);
        BindI(ek[i], &WeaponsSettings::extraIds[i]);
    }
    for (int i = 0; i < 9; i++) {
        static char hk[9][24];
        snprintf(hk[i], 24, "wp.hotbar.%d", i);
        BindB(hk[i], &WeaponsSettings::hotbar[i]);
    }

    BindI("bind.lc", &MenuBinds::lc_bind);
    BindI("bind.aa", &MenuBinds::aa_bind);
    BindI("bind.vel", &MenuBinds::vel_bind);
    BindI("bind.ks", &MenuBinds::ks_bind);
    BindI("bind.cr", &MenuBinds::cr_bind);
    BindI("bind.sr", &MenuBinds::sr_bind);
    BindI("bind.lr", &MenuBinds::lr_bind);
    BindI("bind.reach", &MenuBinds::reach_bind);
    BindI("bind.blink", &MenuBinds::blink_bind);
    BindI("bind.rod", &MenuBinds::rod_bind);
    BindI("bind.ab", &MenuBinds::ab_bind);
    BindI("bind.ablock", &MenuBinds::ablock_bind);
    BindI("bind.bt", &MenuBinds::bt_bind);
    BindI("bind.aw", &MenuBinds::aw_bind);
    BindI("bind.ar", &MenuBinds::ar_bind);
    BindI("bind.al", &MenuBinds::al_bind);
    BindI("bind.ch", &MenuBinds::ch_bind);
    BindI("bind.esp", &MenuBinds::esp_bind);
    BindI("bind.itemesp", &MenuBinds::itemesp_bind);
    BindI("bind.pesp", &MenuBinds::pesp_bind);
    BindI("bind.sesp", &MenuBinds::sesp_bind);
    BindI("bind.besp", &MenuBinds::besp_bind);
    BindI("bind.ntag", &MenuBinds::ntag_bind);
    BindI("bind.tr", &MenuBinds::tr_bind);
    BindI("bind.tj", &MenuBinds::tj_bind);
    BindI("bind.fr", &MenuBinds::fr_bind);
    BindI("bind.en", &MenuBinds::en_bind);
    BindI("bind.nir", &MenuBinds::nir_bind);
    BindI("bind.ad", &MenuBinds::ad_bind);
    BindI("bind.pf", &MenuBinds::pf_bind);
    BindI("bind.rc", &MenuBinds::rc_bind);
    BindI("bind.bboost", &MenuBinds::bboost_bind);
    BindI("bind.fp", &MenuBinds::fp_bind);
    BindI("bind.fb", &MenuBinds::fb_bind);
    BindI("bind.at", &MenuBinds::at_bind);
    BindI("bind.cs", &MenuBinds::cs_bind);
    BindI("bind.im", &MenuBinds::im_bind);
    BindI("bind.ba", &MenuBinds::ba_bind);
    BindI("bind.bi", &MenuBinds::bi_bind);
    BindI("bind.clutch", &MenuBinds::clutch_bind);
    BindI("bind.tl", &MenuBinds::tl_bind);
    BindI("bind.iw", &MenuBinds::iw_bind);
    BindI("bind.fs", &MenuBinds::fs_bind);
    BindI("bind.njd", &MenuBinds::njd_bind);
    BindI("bind.qa", &MenuBinds::qa_bind);
    BindI("bind.st", &MenuBinds::st_bind);
    BindI("bind.sp", &MenuBinds::sp_bind);
    BindI("bind.ns", &MenuBinds::ns_bind);
    BindI("bind.strf", &MenuBinds::strf_bind);
    BindI("bind.notif", &MenuBinds::notif_bind);
    BindI("bind.ptr", &MenuBinds::ptr_bind);
    BindI("bind.ind", &MenuBinds::ind_bind);
    BindI("bind.nhc", &MenuBinds::nhc_bind);
    BindI("bind.destruct", &MenuBinds::destruct_bind);
}

bool* EnabledOf(Module* m) {
    if (!m) return nullptr;
    if (auto* x = dynamic_cast<AimAssist*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Velocity*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<KeepSprint*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Criticals*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<SprintReset*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<LagRange*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Reach*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Blink*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AutoRod*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AntiBot*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AutoBlock*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Backtrack*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AutoWeapon*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AutoRefill*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<ArrayList*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Chams*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Esp*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<ItemEsp*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<PlayerEsp*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<StorageEsp*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<BlockEsp*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Nametag*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Tracer*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Trajectories*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<FastPlace*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<FastBreak*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AutoTool*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<ChestStealer*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<InvManager*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<BridgeAssist*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<BlockIn*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Clutch*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<TickLocker*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<InvWalk*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<FastStop*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<NoJumpDelay*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<QuickAccel*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<SnapTap*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Sprint*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<NoSlow*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Strafe*>(m)) return &x->enabled;
    if (dynamic_cast<LeftClicker*>(m)) return &Clicker::enabled;
    if (dynamic_cast<ThrowModule*>(m)) return &ThrowSettings::enabled;
    if (dynamic_cast<FriendsModule*>(m)) return &FriendsSettings::enabled;
    if (dynamic_cast<EnemiesModule*>(m)) return &EnemiesSettings::enabled;
    if (auto* x = dynamic_cast<NoItemRelease*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<AntiDebuff*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<PingFix*>(m)) return &x->enabled;
    if (dynamic_cast<RightClickerModule*>(m)) return &RightClicker::enabled;
    if (auto* x = dynamic_cast<BowBoost*>(m)) return &x->enabled;
    if (dynamic_cast<ScrollModule*>(m)) return &Scroll::enabled;
    if (dynamic_cast<NotificationsModule*>(m)) return &NotificationSettings::enabled;
    if (auto* x = dynamic_cast<Pointers*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<Indicators*>(m)) return &x->enabled;
    if (auto* x = dynamic_cast<NoHurtCam*>(m)) return &x->enabled;
    if (dynamic_cast<ArmorSwitcher*>(m)) return &Armor::enabled;
    return nullptr;
}

J Capture() {
    RegisterBinds();
    J root = J::Obj();
    J values = J::Obj();
    for (auto& b : g_binds) {
        switch (b.kind) {
        case KB: values.o[b.key] = J::Bool(*(bool*)b.ptr); break;
        case KI: values.o[b.key] = J::Num(*(int*)b.ptr); break;
        case KF: values.o[b.key] = J::Num(*(float*)b.ptr); break;
        case KS: values.o[b.key] = J::Str((char*)b.ptr); break;
        case KC: {
            J a = J::Arr();
            float* c = (float*)b.ptr;
            for (int i = 0; i < 4; i++) a.a.push_back(J::Num(c[i]));
            values.o[b.key] = std::move(a);
            break;
        }
        }
    }
    for (auto* m : Modules::GetRegisteredModules()) {
        const char* n = m->GetName();
        bool* e = EnabledOf(m);
        if (!n || !*n || !e) continue;
        values.o[std::string("mod.") + n] = J::Bool(*e);
    }
    root.o["values"] = std::move(values);

    J fr = J::Arr();
    for (auto& n : FriendsSettings::DisplayCopy()) fr.a.push_back(J::Str(n));
    root.o["friends"] = std::move(fr);
    J en = J::Arr();
    for (auto& n : EnemiesSettings::DisplayCopy()) en.a.push_back(J::Str(n));
    root.o["enemies"] = std::move(en);
    return root;
}

void Apply(const J& root) {
    RegisterBinds();
    auto itv = root.o.find("values");
    if (itv == root.o.end() || itv->second.t != J::O) return;
    const J& values = itv->second;

    for (auto& b : g_binds) {
        auto it = values.o.find(b.key);
        if (it == values.o.end()) continue;
        const J& v = it->second;
        switch (b.kind) {
        case KB: *(bool*)b.ptr = v.asBool(*(bool*)b.ptr); break;
        case KI: *(int*)b.ptr = (int)lround(v.asNum(*(int*)b.ptr)); break;
        case KF: *(float*)b.ptr = (float)v.asNum(*(float*)b.ptr); break;
        case KS:
            if (v.t == J::S && b.slen > 0)
                strncpy_s((char*)b.ptr, b.slen, v.s.c_str(), _TRUNCATE);
            break;
        case KC:
            if (v.t == J::A) {
                float* c = (float*)b.ptr;
                for (int i = 0; i < 4 && i < (int)v.a.size(); i++)
                    c[i] = (float)v.a[i].asNum(c[i]);
            }
            break;
        }
    }
    for (auto* m : Modules::GetRegisteredModules()) {
        const char* n = m->GetName();
        bool* e = EnabledOf(m);
        if (!n || !*n || !e) continue;
        auto it = values.o.find(std::string("mod.") + n);
        if (it != values.o.end()) *e = it->second.asBool(*e);
    }
    auto itf = root.o.find("friends");
    if (itf != root.o.end() && itf->second.t == J::A) {
        std::vector<std::string> names;
        for (auto& x : itf->second.a) if (x.t == J::S) names.push_back(x.s);
        FriendsSettings::ReplaceFromNames(names);
    }
    auto ite = root.o.find("enemies");
    if (ite != root.o.end() && ite->second.t == J::A) {
        std::vector<std::string> names;
        for (auto& x : ite->second.a) if (x.t == J::S) names.push_back(x.s);
        EnemiesSettings::ReplaceFromNames(names);
    }
}

std::string PathFor(const std::string& id) {
    return ConfigDir() + "\\" + id + ".json";
}

bool WriteFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size());
    return (bool)f;
}

bool ReadFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

J MakeRoot(const char* name, const char* desc, const char* created, const char* modified) {
    J root = Capture();
    root.o["name"] = J::Str(name ? name : "");
    root.o["description"] = J::Str(desc ? desc : "");
    root.o["author"] = J::Str("User");
    root.o["version"] = J::Str("1.0");
    root.o["created"] = J::Str(created ? created : NowStamp());
    root.o["modified"] = J::Str(modified ? modified : NowStamp());
    return root;
}

bool UniqueId(std::string& id) {
    std::string base = id;
    int n = 2;
    while (GetFileAttributesA(PathFor(id).c_str()) != INVALID_FILE_ATTRIBUTES) {
        id = base + "_" + std::to_string(n++);
        if (n > 99) return false;
    }
    return true;
}

} // namespace

std::string ConfigManager::Directory() { return ConfigDir(); }

const std::string& ConfigManager::CurrentId() { return g_currentId; }

bool ConfigManager::IsLoading() { return g_loading.load(); }

std::vector<ConfigInfo> ConfigManager::List() {
    std::vector<ConfigInfo> out;
    std::string dir = ConfigDir() + "\\*.json";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(dir.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string file = fd.cFileName;
        if (file.size() < 6) continue;
        std::string id = file.substr(0, file.size() - 5);
        std::string raw;
        if (!ReadFile(PathFor(id), raw)) continue;
        Parser pr{ raw.c_str() };
        J root = pr.parse();
        ConfigInfo c;
        c.id = id;
        c.name = root.o.count("name") ? root.o["name"].asStr() : id;
        c.description = root.o.count("description") ? root.o["description"].asStr() : "";
        c.created = root.o.count("created") ? root.o["created"].asStr() : "";
        c.modified = root.o.count("modified") ? root.o["modified"].asStr() : "";
        c.loaded = (g_currentId == id);
        out.push_back(std::move(c));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const ConfigInfo& a, const ConfigInfo& b) {
        return a.name < b.name;
    });
    return out;
}

bool ConfigManager::SaveNew(const char* name, const char* description) {
    if (!name || !name[0]) return false;
    std::string id = Sanitize(name);
    if (!UniqueId(id)) return false;
    std::string now = NowStamp();
    J root = MakeRoot(name, description, now.c_str(), now.c_str());
    if (!WriteFile(PathFor(id), Dump(root, 0))) return false;
    g_currentId = id;
    NotificationSettings::PushInfo("Config", "Config created", "Utility");
    return true;
}

bool ConfigManager::Update(const std::string& id) {
    if (id.empty()) return false;
    std::string raw;
    std::string name = id, desc, created = NowStamp();
    if (ReadFile(PathFor(id), raw)) {
        Parser pr{ raw.c_str() };
        J old = pr.parse();
        if (old.o.count("name")) name = old.o["name"].asStr();
        if (old.o.count("description")) desc = old.o["description"].asStr();
        if (old.o.count("created")) created = old.o["created"].asStr();
    }
    J root = MakeRoot(name.c_str(), desc.c_str(), created.c_str(), NowStamp().c_str());
    if (!WriteFile(PathFor(id), Dump(root, 0))) return false;
    g_currentId = id;
    NotificationSettings::PushInfo("Config", "Config updated", "Utility");
    return true;
}

bool ConfigManager::Load(const std::string& id) {
    std::string raw;
    if (!ReadFile(PathFor(id), raw)) return false;
    Parser pr{ raw.c_str() };
    J root = pr.parse();
    if (root.t != J::O) return false;
    g_loading.store(true);
    Apply(root);
    g_loading.store(false);
    g_currentId = id;
    NotificationSettings::PushInfo("Config", "Config loaded", "Utility");
    return true;
}

bool ConfigManager::Delete(const std::string& id) {
    if (id.empty()) return false;
    if (!DeleteFileA(PathFor(id).c_str())) return false;
    if (g_currentId == id) g_currentId.clear();
    NotificationSettings::PushInfo("Config", "Config deleted", "Utility");
    return true;
}

bool ConfigManager::Rename(const std::string& id, const char* newName) {
    if (id.empty() || !newName || !newName[0]) return false;
    std::string raw;
    if (!ReadFile(PathFor(id), raw)) return false;
    Parser pr{ raw.c_str() };
    J root = pr.parse();
    if (root.t != J::O) return false;
    root.o["name"] = J::Str(newName);
    root.o["modified"] = J::Str(NowStamp());
    return WriteFile(PathFor(id), Dump(root, 0));
}
