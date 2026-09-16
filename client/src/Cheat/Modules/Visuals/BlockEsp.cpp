#include "pch.h"
#include "BlockEsp.h"

#include "../Misc/Overlay.h"
#include "../../../Game/Classes/Minecraft.h"
#include "../../../Game/Classes/World.h"
#include "../../../Game/Classes/Player.h"
#include "../../../Game/Classes/Block.h"
#include "../../../Game/Classes/RenderManager.h"
#include "../../../Game/Classes/ActiveRenderInfo.h"
#include "../../../Game/Mapper.h"
#include "../../../Game/Klass.h"
#include "../../../Cheat/Hack.h"
#include "../../../Helper/Utils.h"

#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>
#pragma comment(lib, "opengl32.lib")

#ifndef GL_LINE_SMOOTH
#define GL_LINE_SMOOTH 0x0B20
#endif
#ifndef GL_LINE_SMOOTH_HINT
#define GL_LINE_SMOOTH_HINT 0x0C52
#endif
#ifndef GL_NICEST
#define GL_NICEST 0x1102
#endif

struct BlockHit {
    int x, y, z, id;
    float color[4];
};

static std::mutex s_mu;
static std::vector<BlockHit> s_draw;
static std::vector<BlockHit> s_render;

static jmethodID s_getBlockIII = nullptr;
static jmethodID s_getBlockState = nullptr;
static jmethodID s_stateGetBlock = nullptr;
static jmethodID s_bpCtor = nullptr;
static jclass    s_bpCls = nullptr;
static jclass    s_ibsCls = nullptr;
static bool      s_tried = false;
static bool      s_iiiTried = false;

static void JniOk(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

static void DefaultColor(int id, float* c) {
    unsigned h = (unsigned)id * 2654435761u;
    c[0] = 0.25f + ((h >> 0) & 255) / 255.f * 0.75f;
    c[1] = 0.25f + ((h >> 8) & 255) / 255.f * 0.75f;
    c[2] = 0.25f + ((h >> 16) & 255) / 255.f * 0.75f;
    c[3] = 1.f;
}

bool BlockEsp_Has(int id) {
    if (id <= 0) return false;
    for (int i = 0; i < 32; i++)
        if (BlockEspSettings::ids[i] == id) return true;
    return false;
}

bool BlockEsp_Add(int id) {
    if (id <= 0 || BlockEsp_Has(id)) return false;
    for (int i = 0; i < 32; i++) {
        if (BlockEspSettings::ids[i] <= 0) {
            BlockEspSettings::ids[i] = id;
            if (BlockEspSettings::colors[i][3] <= 0.01f)
                DefaultColor(id, BlockEspSettings::colors[i]);
            return true;
        }
    }
    return false;
}

void BlockEsp_Remove(int id) {
    int w = 0;
    float cols[32][4]{};
    for (int i = 0; i < 32; i++) {
        if (BlockEspSettings::ids[i] > 0 && BlockEspSettings::ids[i] != id) {
            cols[w][0] = BlockEspSettings::colors[i][0];
            cols[w][1] = BlockEspSettings::colors[i][1];
            cols[w][2] = BlockEspSettings::colors[i][2];
            cols[w][3] = BlockEspSettings::colors[i][3];
            BlockEspSettings::ids[w++] = BlockEspSettings::ids[i];
        }
    }
    for (int i = 0; i < 32; i++) {
        if (i < w) {
            BlockEspSettings::colors[i][0] = cols[i][0];
            BlockEspSettings::colors[i][1] = cols[i][1];
            BlockEspSettings::colors[i][2] = cols[i][2];
            BlockEspSettings::colors[i][3] = cols[i][3];
        } else {
            BlockEspSettings::ids[i] = 0;
            BlockEspSettings::colors[i][0] = BlockEspSettings::colors[i][1] =
                BlockEspSettings::colors[i][2] = 0.f;
            BlockEspSettings::colors[i][3] = 1.f;
        }
    }
}

const float* BlockEsp_Color(int id) {
    for (int i = 0; i < 32; i++) {
        if (BlockEspSettings::ids[i] == id)
            return BlockEspSettings::colors[i];
    }
    return BlockEspSettings::colors[0];
}

static int FloorDiv16(int v) {
    return v < 0 ? (v - 15) / 16 : v / 16;
}

static void Ensure(JNIEnv* env) {
    if (s_tried || !env) return;
    s_tried = true;

    std::string bpName = Mapper::Get("net/minecraft/util/BlockPos");
    if (!bpName.empty()) {
        Klass* bpK = g_Instance->FindClass(bpName.c_str());
        if (bpK) {
            s_bpCls = (jclass)env->NewGlobalRef((jclass)bpK);
            s_bpCtor = env->GetMethodID(s_bpCls, "<init>", "(III)V");
            JniOk(env);
        }
    }
    std::string ibs = Mapper::Get("net/minecraft/block/state/IBlockState");
    if (!ibs.empty()) {
        Klass* k = g_Instance->FindClass(ibs.c_str());
        if (k) s_ibsCls = (jclass)env->NewGlobalRef((jclass)k);
    }
}

static int GetBlockIdAt(JNIEnv* env, jobject world, int x, int y, int z) {
    if (!world || y < 0 || y > 255) return 0;
    if (env->PushLocalFrame(8) != JNI_OK) return 0;

    jclass worldC = env->GetObjectClass(world);
    if (!worldC) {
        env->PopLocalFrame(nullptr);
        return 0;
    }

    std::string blockSig = Mapper::Get("net/minecraft/block/Block", 2);
    std::string getBlock = Mapper::Get("getBlock");
    if (getBlock.empty()) getBlock = "getBlock";

    jobject block = nullptr;
    if (!s_iiiTried && !blockSig.empty()) {
        s_iiiTried = true;
        s_getBlockIII = env->GetMethodID(worldC, getBlock.c_str(), ("(III)" + blockSig).c_str());
        JniOk(env);
    }
    if (s_getBlockIII) {
        block = env->CallObjectMethod(world, s_getBlockIII, x, y, z);
        JniOk(env);
    }

    if (!block && s_bpCls && s_bpCtor) {
        if (!s_getBlockState) {
            std::string bpName = Mapper::Get("net/minecraft/util/BlockPos");
            std::string ibs = Mapper::Get("net/minecraft/block/state/IBlockState");
            if (!bpName.empty() && !ibs.empty()) {
                std::string sig = "(L" + bpName + ";)L" + ibs + ";";
                s_getBlockState = env->GetMethodID(worldC, "getBlockState", sig.c_str());
                JniOk(env);
            }
        }
        jobject bp = env->NewObject(s_bpCls, s_bpCtor, x, y, z);
        JniOk(env);
        if (bp && s_getBlockState) {
            jobject state = env->CallObjectMethod(world, s_getBlockState, bp);
            JniOk(env);
            if (state) {
                if (!s_stateGetBlock && !blockSig.empty()) {
                    jclass stC = env->GetObjectClass(state);
                    s_stateGetBlock = env->GetMethodID(stC, getBlock.c_str(), ("()" + blockSig).c_str());
                    JniOk(env);
                }
                if (s_stateGetBlock) {
                    block = env->CallObjectMethod(state, s_stateGetBlock);
                    JniOk(env);
                }
            }
        }
    }

    int id = 0;
    if (block)
        id = ((Block*)block)->GetID(env);
    JniOk(env);
    env->PopLocalFrame(nullptr);
    return id;
}

static int TypeIndex(int id, const int* types, int n) {
    for (int t = 0; t < n; t++)
        if (types[t] == id) return t;
    return -1;
}

static void MakeHit(BlockHit& h, int x, int y, int z, int id, const float* col) {
    h.x = x; h.y = y; h.z = z; h.id = id;
    h.color[0] = col[0]; h.color[1] = col[1]; h.color[2] = col[2]; h.color[3] = col[3];
}

static void ScanVolume(JNIEnv* env, jobject world,
    int x0, int x1, int y0, int y1, int z0, int z1,
    const int* types, int nTypes, const float cols[][4],
    int limit, std::vector<BlockHit>& out)
{
    int found = 0;
    for (int x = x0; x <= x1 && found < limit; x++) {
        for (int z = z0; z <= z1 && found < limit; z++) {
            for (int y = y0; y <= y1 && found < limit; y++) {
                int id = GetBlockIdAt(env, world, x, y, z);
                int ti = TypeIndex(id, types, nTypes);
                if (ti < 0) continue;
                BlockHit h{};
                MakeHit(h, x, y, z, id, cols[ti]);
                out.push_back(h);
                found++;
            }
        }
    }
}

static void ReplaceRegion(int x0, int x1, int y0, int y1, int z0, int z1,
    std::vector<BlockHit>&& fresh)
{
    std::lock_guard<std::mutex> lock(s_mu);
    s_draw.erase(std::remove_if(s_draw.begin(), s_draw.end(),
        [&](const BlockHit& h) {
            return h.x >= x0 && h.x <= x1 && h.y >= y0 && h.y <= y1 && h.z >= z0 && h.z <= z1;
        }), s_draw.end());
    s_draw.insert(s_draw.end(), fresh.begin(), fresh.end());
}

static void KeepOnlyTypes(const int* types, int nTypes) {
    std::lock_guard<std::mutex> lock(s_mu);
    s_draw.erase(std::remove_if(s_draw.begin(), s_draw.end(),
        [&](const BlockHit& h) { return TypeIndex(h.id, types, nTypes) < 0; }),
        s_draw.end());
    for (auto& h : s_draw) {
        int ti = TypeIndex(h.id, types, nTypes);
        if (ti < 0) continue;
        // colors passed separately in Run
        (void)ti;
    }
}

static void ClearHits() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_draw.clear();
}

static unsigned TypesKey() {
    unsigned k = 0;
    for (int i = 0; i < 32; i++)
        k = k * 16777619u ^ (unsigned)BlockEspSettings::ids[i];
    return k;
}

static void DrawBox3D(float x, float y, float z) {
    const float x2 = x + 1.f, y2 = y + 1.f, z2 = z + 1.f;
    glBegin(GL_LINES);
    glVertex3f(x, y, z); glVertex3f(x2, y, z);
    glVertex3f(x2, y, z); glVertex3f(x2, y, z2);
    glVertex3f(x2, y, z2); glVertex3f(x, y, z2);
    glVertex3f(x, y, z2); glVertex3f(x, y, z);
    glVertex3f(x, y2, z); glVertex3f(x2, y2, z);
    glVertex3f(x2, y2, z); glVertex3f(x2, y2, z2);
    glVertex3f(x2, y2, z2); glVertex3f(x, y2, z2);
    glVertex3f(x, y2, z2); glVertex3f(x, y2, z);
    glVertex3f(x, y, z); glVertex3f(x, y2, z);
    glVertex3f(x2, y, z); glVertex3f(x2, y2, z);
    glVertex3f(x2, y, z2); glVertex3f(x2, y2, z2);
    glVertex3f(x, y, z2); glVertex3f(x, y2, z2);
    glEnd();
}

void BlockEsp::Run(JNIEnv* env) {
    static bool s_wasOn = false;
    static unsigned s_typesKey = 0;
    static int s_walk = 0;
    static int s_pcx = 0x7fffffff, s_pcz = 0x7fffffff;
    static size_t s_check = 0;

    if (!env || !enabled) {
        if (s_wasOn) {
            ClearHits();
            s_wasOn = false;
            s_walk = 0;
        }
        Sleep(30);
        return;
    }

    if (!s_wasOn) {
        ClearHits();
        s_walk = 0;
        s_pcx = 0x7fffffff;
        s_typesKey = 0;
        s_wasOn = true;
    }

    if (Overlay::isOpen) {
        Sleep(15);
        return;
    }

    Ensure(env);

    int nTypes = 0;
    int types[32];
    float cols[32][4];
    for (int i = 0; i < 32; i++) {
        if (BlockEspSettings::ids[i] <= 0) continue;
        types[nTypes] = BlockEspSettings::ids[i];
        cols[nTypes][0] = BlockEspSettings::colors[i][0];
        cols[nTypes][1] = BlockEspSettings::colors[i][1];
        cols[nTypes][2] = BlockEspSettings::colors[i][2];
        cols[nTypes][3] = BlockEspSettings::colors[i][3];
        nTypes++;
    }

    const unsigned key = TypesKey();
    if (key != s_typesKey) {
        s_typesKey = key;
        KeepOnlyTypes(types, nTypes);
        s_walk = 0;
    }

    if (nTypes <= 0) {
        ClearHits();
        Sleep(30);
        return;
    }

    jobject playerObj = Minecraft::GetThePlayer(env);
    jobject world = Minecraft::GetTheWorld(env);
    JniOk(env);
    if (!playerObj || !world) {
        if (playerObj) env->DeleteLocalRef(playerObj);
        if (world) env->DeleteLocalRef(world);
        Sleep(15);
        return;
    }

    Vec3D pos = ((Player*)playerObj)->GetPos(env);
    JniOk(env);
    const int px = (int)floor(pos.x);
    const int py = (int)floor(pos.y);
    const int pz = (int)floor(pos.z);
    const int pcx = FloorDiv16(px);
    const int pcz = FloorDiv16(pz);
    const int range = (std::max)(1, (std::min)(8, BlockEspSettings::rangeChunks));
    const int limit = (std::max)(8, (std::min)(256, BlockEspSettings::limitPerChunk));
    const int y0 = (std::max)(0, py - 8);
    const int y1 = (std::min)(255, py + 8);
    const int ny0 = (std::max)(0, py - 4);
    const int ny1 = (std::min)(255, py + 4);

    if (s_pcx != pcx || s_pcz != pcz) {
        s_walk = 0;
        s_pcx = pcx;
        s_pcz = pcz;
    }

    {
        std::vector<BlockHit> snap;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            snap = s_draw;
        }
        if (!snap.empty()) {
            const size_t n = snap.size();
            const size_t batch = (std::min)(n, (size_t)64);
            for (size_t k = 0; k < batch; k++) {
                size_t i = (s_check + k) % n;
                BlockHit& h = snap[i];
                int id = GetBlockIdAt(env, world, h.x, h.y, h.z);
                if (TypeIndex(id, types, nTypes) < 0)
                    h.id = -1;
                else
                    h.id = id;
            }
            s_check += batch;
            std::lock_guard<std::mutex> lock(s_mu);
            for (size_t k = 0; k < batch; k++) {
                size_t i = (s_check - batch + k) % n;
                if (i >= snap.size()) continue;
                const BlockHit& s = snap[i];
                if (s.id > 0) continue;
                s_draw.erase(std::remove_if(s_draw.begin(), s_draw.end(),
                    [&](const BlockHit& h) { return h.x == s.x && h.y == s.y && h.z == s.z; }),
                    s_draw.end());
            }
        }
    }

    const int lx0 = px - 5, lx1 = px + 5, lz0 = pz - 5, lz1 = pz + 5;
    std::vector<BlockHit> nearHits;
    ScanVolume(env, world, lx0, lx1, ny0, ny1, lz0, lz1, types, nTypes, cols, limit, nearHits);
    ReplaceRegion(lx0, lx1, ny0, ny1, lz0, lz1, std::move(nearHits));

    const int span = range * 2 + 1;
    const int idx = s_walk % (span * span);
    const int cx = pcx - range + (idx % span);
    const int cz = pcz - range + (idx / span);
    const int x0 = cx * 16;
    const int z0 = cz * 16;
    std::vector<BlockHit> chunkHits;
    ScanVolume(env, world, x0, x0 + 15, y0, y1, z0, z0 + 15, types, nTypes, cols, limit, chunkHits);
    ReplaceRegion(x0, x0 + 15, y0, y1, z0, z0 + 15, std::move(chunkHits));
    s_walk++;

    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(world);
}

void BlockEsp::OnRender(JNIEnv* env) {
    if (!enabled || !env) return;
    JniOk(env);

    {
        std::lock_guard<std::mutex> lock(s_mu);
        s_render.clear();
        s_render.reserve(s_draw.size());
        for (const auto& h : s_draw) {
            if (!BlockEsp_Has(h.id)) continue;
            BlockHit c = h;
            const float* col = BlockEsp_Color(h.id);
            c.color[0] = col[0]; c.color[1] = col[1]; c.color[2] = col[2]; c.color[3] = col[3];
            s_render.push_back(c);
        }
    }
    if (s_render.empty()) return;

    jobject rmObj = Minecraft::GetRenderManager(env);
    if (!rmObj) return;
    std::vector<float> proj = ActiveRenderInfo::GetProjection(env);
    std::vector<float> mv = ActiveRenderInfo::GetModelView(env);
    if (proj.size() < 16 || mv.size() < 16) return;

    Vec3D cam = ((RenderManager*)rmObj)->GetRenderPos(env);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(proj.data());
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(mv.data());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glLineWidth((std::max)(0.5f, BlockEspSettings::outlineWidth));

    for (const auto& h : s_render) {
        float rx = (float)((double)h.x - cam.x);
        float ry = (float)((double)h.y - cam.y);
        float rz = (float)((double)h.z - cam.z);
        glColor4f(h.color[0], h.color[1], h.color[2], h.color[3]);
        DrawBox3D(rx, ry, rz);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopMatrix();
    glPopAttrib();
}
