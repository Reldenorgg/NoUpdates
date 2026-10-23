// Axiom for Levi v1.0 - Minecraft Bedrock 26.52.3 (arm64)
//  * No Updates : freezes block events (physics, redstone, ticks)
//  * SEL        : HUD button; next two block taps set Pos1 / Pos2
//  * DEL        : HUD button; removes every block of the selection through
//                 the server-side GameMode::destroyBlock (works out of reach).
//  * FILL       : HUD button; fills the AIR cells of the selection with the
//                 block in your hand through the server-side GameMode
//                 ::useItemOn. Both jobs run on the server thread when you
//                 break any block (the trigger).
#include <pl/Input.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>

#include <dlfcn.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Target {
  uintptr_t rva; // operator() thunk of a block event handler
  int group;
};

constexpr Target kTargets[] = {
    {0x100a30cc,0}, {0x100a317c,0}, {0x100a3344,0}, {0x100a3480,0}, {0x100a3530,0},
    {0x100c8a88,0}, {0x100c8b38,0}, {0x10107588,0}, {0x10107638,0}, {0x1007f188,1},
    {0x1007f2e8,1}, {0x1007f4f8,1}, {0x1007fa8c,1}, {0x100937f0,1}, {0x100b6594,1},
    {0x100c8f58,1}, {0x100db684,1}, {0x100db944,1}, {0x100dbb54,1}, {0x100eab90,1},
    {0x100ecfc4,1}, {0x10106fb8,1}, {0x10107278,1}, {0x101fffa4,1}, {0x10200734,1},
    {0x107745ec,1}, {0x10774a0c,1}, {0x10784f58,1}, {0x1079c314,1}, {0x94f8e30,2},
    {0x94f9024,2}, {0x94f9218,2}, {0x94fa8e8,2}, {0x94faadc,2}, {0x94fad38,2},
    {0x1006daa8,2}, {0x1006de98,2}, {0x1006e010,2}, {0x1007e820,2}, {0x1007e8d0,2},
    {0x1007ea30,2}, {0x1007ee30,2}, {0x1007ef90,2}, {0x1007f238,2}, {0x1007f448,2},
    {0x1007f92c,2}, {0x10093320,2}, {0x100933d0,2}, {0x10093480,2}, {0x10093530,2},
    {0x100935e0,2}, {0x10093690,2}, {0x100938a0,2}, {0x10093a00,2}, {0x10093b60,2},
    {0x10093c10,2}, {0x10093cc0,2}, {0x10093e20,2}, {0x10093f80,2}, {0x10094030,2},
    {0x100a2898,2}, {0x100a2b58,2}, {0x100a301c,2}, {0x100a4234,2}, {0x100a42e4,2},
    {0x100a4394,2}, {0x100a4444,2}, {0x100b5c2c,2}, {0x100b5cdc,2}, {0x100b5e3c,2},
    {0x100b6644,2}, {0x100b6af8,2}, {0x100b6fc8,2}, {0x100b7078,2}, {0x100b7128,2},
    {0x100b71d8,2}, {0x100b7338,2}, {0x100c8348,2}, {0x100c83f8,2}, {0x100c8be8,2},
    {0x100c8c98,2}, {0x100c8ea8,2}, {0x100c90b8,2}, {0x100c9350,2}, {0x100c94b0,2},
    {0x100c9610,2}, {0x100c9838,2}, {0x100c9998,2}, {0x100c9af8,2}, {0x100db5d4,2},
    {0x100db894,2}, {0x100db9f4,2}, {0x100dc54c,2}, {0x100eaae0,2}, {0x100eb0b0,2},
    {0x100eb210,2}, {0x100eb500,2}, {0x100eb8a0,2}, {0x100ec994,2}, {0x100ecaf4,2},
    {0x100ecdb4,2}, {0x100ecf14,2}, {0x100ed748,2}, {0x100edb04,2}, {0x100edbb4,2},
    {0x10106098,2}, {0x10106358,2}, {0x101064b8,2}, {0x101066c8,2}, {0x10106778,2},
    {0x10106ae8,2}, {0x10106b98,2}, {0x10106c48,2}, {0x10106cf8,2}, {0x10106da8,2},
    {0x10106e58,2}, {0x10107118,2}, {0x10107488,2}, {0x10121694,2}, {0x101217f4,2},
    {0x10121954,2}, {0x10121a04,2}, {0x10121de8,2}, {0x10121e98,2}, {0x10121f48,2},
    {0x10121ff8,2}, {0x10122418,2}, {0x101ff910,2}, {0x101ff9c0,2}, {0x101ffb20,2},
    {0x101ffd38,2}, {0x101ffef4,2}, {0x10200474,2}, {0x10200684,2}, {0x10228c68,2},
    {0x10228dc8,2}, {0x1073bce8,2}, {0x1073bd98,2}, {0x1073c594,2}, {0x10772f5c,2},
    {0x1077300c,2}, {0x1077321c,2}, {0x1077337c,2}, {0x1077474c,2}, {0x107747fc,2},
    {0x10774b6c,2}, {0x10774ccc,2}, {0x10774e2c,2}, {0x1077503c,2}, {0x10783b18,2},
    {0x10783c78,2}, {0x10783dd8,2}, {0x10783e88,2}, {0x10784720,2}, {0x10784930,2},
    {0x107849e0,2}, {0x10784be8,2}, {0x1079b138,2}, {0x1079b434,2}, {0x1079bef4,2},
    {0x1079bfa4,2}, {0x1079c104,2}, {0x1079c264,2}, {0x1079c3c4,2}, {0x1079e178,2},
};
constexpr size_t kCount = sizeof(kTargets) / sizeof(kTargets[0]);

constexpr unsigned char kExpected[16] = {0x02, 0x20, 0x41, 0xa9, 0x09, 0x04,
                                         0x40, 0xf9, 0x20, 0x05, 0x88, 0x8b,
                                         0x68, 0x00, 0x00, 0x36};

constexpr const char *kModuleId = "no_updates.main";
constexpr const char *kToolsId = "axiom.tools";

// ---- game functions (26.52.3) -- every address is verified at load time
constexpr uintptr_t kBuildRva = 0xf8a117c; // GameMode vtable slot 7
constexpr const char *kBuildHex = "ffc304d1fd7b0da9fc7300f9fa670fa9";
constexpr uintptr_t kGmDestroyRva = 0xf8a03ac; // GameMode vtable slot 3
constexpr const char *kGmDestroyHex = "ff0302d1fd7b04a9f85f05a9f65706a9";
constexpr uintptr_t kLpTickRva = 0xaac7dd8; // LocalPlayer vtable slot 25
constexpr const char *kLpTickHex = "e80f19fcfd7b01a9fc6f02a9fa6703a9";
constexpr uintptr_t kGetBlockRva = 0xfb91918; // BlockSource vtable slot 2
constexpr uintptr_t kGmPlayerOff = 0x8;       // GameMode -> Player
constexpr uintptr_t kActorDimOff = 448;       // Actor -> Dimension
constexpr uintptr_t kDimBsOff = 208;          // Dimension -> BlockSource

constexpr int kGetBlockPat[] = {
    -1, -1, -1, 0xD1, -1, -1, -1, 0xA9, -1, -1, -1, 0xA9, -1, -1, -1, 0xA9,
    -1, -1, -1, 0x91, 0x56, 0xD0, 0x3B, 0xD5, -1, -1, -1, 0xF9, -1, -1, -1,
    0xF9, -1, -1, -1, 0xB9, -1, -1, -1, 0x79, 0x1F, 0x01, 0x09, 0x6B};

// Held item / placement helpers (addresses verified by byte patterns)
constexpr uintptr_t kHandItemRva = 0xf57a04c;
constexpr int kHandItemPat[] = {0x28, 0x1C, 0x00, 0x12, 0x1F, 0x05, 0x00, 0x71,
                                -1,   -1,   -1,   -1,   0x08, 0x00, 0x40, 0xF9,
                                0x01, 0x39, 0x41, 0xF9, 0x20, 0x00, 0x1F, 0xD6};
constexpr uintptr_t kItemBlockTypeRva = 0xfc74c70;
constexpr int kItemBlockTypePat[] = {
    0xFD, 0x7B, 0xBE, 0xA9, 0xF4, 0x4F, 0x01, 0xA9, 0xFD, 0x03, 0x00, 0x91,
    0xF4, 0x03, 0x00, 0xAA, -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    0xE0, 0x03, 0x14, 0xAA, -1,   -1,   -1,   -1,   0xF3, 0x03, 0x00, 0xAA};
// BlockSource::setBlock = vtable slot 32 (offset 0x100) per the Levi devs
constexpr int kSetBlockSlot = 32;
constexpr const char *kSetBlockHex = "ff0302d1fd7b02a9fb1b00f9fa6704a9";
constexpr uintptr_t kBsVtRva = 0x13060d50; // BlockSource vtable
constexpr int kUseItemOnSlot = 13; // GameMode::useItemOn (virtual)
constexpr const char *kUseItemOnHex = "ffc303d1fd7b0ba9f76300f9f6570da9";
constexpr uintptr_t kUseItemOnRva = 0xf8a33ec;

struct BTPair {
  void *blockType;
  uint64_t extra;
};
struct BigRet { // generous return buffer: safe if the callee returns by sret
  uintptr_t w[4];
};
using HandItemFn = const void *(*)(void *player, uint8_t hand);
using ItemBlockTypeFn = BTPair (*)(const void *stack);
using UseItemOnFn = BigRet (*)(void *gm, void *item, const int *pos,
                               unsigned char face, const float *hit,
                               void *block);

using Fn = uintptr_t (*)(void *, void *, void *, void *, void *, void *,
                         void *, void *);
using GetBlockFn = void *(*)(void *region, const int *pos);

std::atomic<bool> gFrozen[3];
Fn gOrig[kCount]{};

template <size_t I>
uintptr_t detour(void *a, void *b, void *c, void *d, void *e, void *f,
                 void *g, void *h) {
  if (gFrozen[kTargets[I].group].load(std::memory_order_relaxed))
    return 0;
  return gOrig[I](a, b, c, d, e, f, g, h);
}

template <size_t... I>
std::array<void *, sizeof...(I)> makeDetours(std::index_sequence<I...>) {
  return {reinterpret_cast<void *>(&detour<I>)...};
}

// ---------------------------------------------------------------- helpers
uintptr_t gBase = 0;
std::string gLogPath = "/dev/null";

bool safeRead(const void *p, void *out, size_t n) {
  if (!p)
    return false;
  struct iovec l{out, n};
  struct iovec r{const_cast<void *>(p), n};
  return process_vm_readv(getpid(), &l, 1, &r, 1, 0) ==
         static_cast<ssize_t>(n);
}

bool rdp(uintptr_t addr, uintptr_t &out) {
  return addr > 0x10000 && safeRead(reinterpret_cast<void *>(addr), &out, 8);
}

bool matchHex(const void *p, const char *hex) {
  auto nib = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
  const unsigned char *b = static_cast<const unsigned char *>(p);
  for (int i = 0; i < 16; ++i)
    if (b[i] != ((nib(hex[2 * i]) << 4) | nib(hex[2 * i + 1])))
      return false;
  return true;
}

bool matchPattern(const void *p, const int *pat, size_t n) {
  const unsigned char *b = static_cast<const unsigned char *>(p);
  for (size_t i = 0; i < n; ++i)
    if (pat[i] >= 0 && b[i] != pat[i])
      return false;
  return true;
}

void logLine(const std::string &line) {
  std::ofstream f(gLogPath, std::ios::app);
  if (f.good())
    f << line << "\n";
}

uintptr_t findBase() {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  while (std::getline(maps, line)) {
    if (line.find("libminecraftpe.so") == std::string::npos)
      continue;
    size_t sp1 = line.find(' ');
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos)
      continue;
    unsigned long long off = strtoull(line.c_str() + sp2 + 1, nullptr, 16);
    if (off == 0)
      return static_cast<uintptr_t>(strtoull(line.c_str(), nullptr, 16));
  }
  return 0;
}

std::string filesDir() {
  std::string pkg;
  {
    std::ifstream f("/proc/self/cmdline");
    std::getline(f, pkg, '\0');
  }
  if (pkg.empty())
    pkg = "com.mojang.minecraftpe";
  const char *dirs[] = {"/data/data/", "/storage/emulated/0/Android/data/"};
  for (const char *d : dirs) {
    std::string base = std::string(d) + pkg + "/files/";
    std::ofstream w(base + "no_updates.state", std::ios::app);
    if (w.good())
      return base;
  }
  return "";
}

bool loadState() {
  std::string d = filesDir();
  if (d.empty())
    return false;
  std::ifstream f(d + "no_updates.state");
  char c = '0';
  f >> c;
  return c == '1';
}

void saveState(bool on) {
  std::string d = filesDir();
  if (d.empty())
    return;
  std::ofstream f(d + "no_updates.state", std::ios::trunc);
  f << (on ? '1' : '0');
}

long long nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// ------------------------------------------------------ selection (Axiom)
std::atomic<bool> gToolsOn{true};
std::atomic<bool> gArmed{false};
std::atomic<bool> gHudDirty{true};
std::atomic<bool> gStop{false};
std::atomic<bool> gLoopRunning{false};
std::atomic<bool> gTouchRegistered{false};
std::mutex gSelMu;
int gPos[2][3];
bool gHave[2];
int gNext = 0;

struct Geo {
  float bx = 0, by = 0, bs = 0; // SEL button
  float dy = 0;                 // DEL button (same x and size)
  float fy = 0;                 // FILL button
} gGeo;
std::mutex gGeoMu;

using JniBoolFn = unsigned char (*)(void *, void *);
JniBoolFn gHudFn = nullptr, gMenuFn = nullptr, gPauseFn = nullptr;

void *findSym(const char *name) {
  if (void *p = dlsym(RTLD_DEFAULT, name))
    return p;
  if (void *h = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD))
    return dlsym(h, name);
  return nullptr;
}

// pl::modmenu::getHudSurfaceSize() returns two floats (width, height); it is
// not declared in SDK 0.2.2, so it is resolved in the launcher at runtime.
struct HudSize {
  float width;
  float height;
};
using HudSizeFn = HudSize (*)();
HudSizeFn gSizeFn = nullptr;

HudSize hudSize() {
  if (!gSizeFn)
    return HudSize{0.0f, 0.0f};
  return gSizeFn();
}

void findJni() {
  gSizeFn = reinterpret_cast<HudSizeFn>(
      findSym("_ZN2pl7modmenu17getHudSurfaceSizeEv"));
  gHudFn = reinterpret_cast<JniBoolFn>(findSym(
      "Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsHudScreenOpen"));
  gMenuFn = reinterpret_cast<JniBoolFn>(findSym(
      "Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsShowingMenu"));
  gPauseFn = reinterpret_cast<JniBoolFn>(findSym(
      "Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsPauseMenuOpen"));
}

bool gameHudVisible() {
  if (!gHudFn || !gMenuFn || !gPauseFn)
    return true;
  return gHudFn(nullptr, nullptr) && !gMenuFn(nullptr, nullptr) &&
         !gPauseFn(nullptr, nullptr);
}

// buildBlock(pos, face): pos is the CLICKED block (the new block would go on
// the neighbouring cell given by face). The selection uses the clicked block.
void handleTap(void *posPtr, void *faceArg) {
  int pos[3];
  if (!safeRead(posPtr, pos, sizeof(pos)))
    return;
  const unsigned face =
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(faceArg) & 0xff);
  int idx;
  {
    std::lock_guard<std::mutex> lk(gSelMu);
    idx = gNext;
    std::memcpy(gPos[idx], pos, sizeof(pos));
    gHave[idx] = true;
    gNext = (gNext + 1) % 2;
  }
  if (idx == 1)
    gArmed.store(false); // both points set: taps behave normally again
  char buf[96];
  snprintf(buf, sizeof(buf), "SELECT pos%d = (%d,%d,%d) face=%u", idx + 1,
           pos[0], pos[1], pos[2], face);
  logLine(buf);
  gHudDirty.store(true);
}

void toggleArmed() {
  bool arm = !gArmed.load();
  if (arm) {
    std::lock_guard<std::mutex> lk(gSelMu);
    gHave[0] = gHave[1] = false;
    gNext = 0;
  }
  gArmed.store(arm);
  gHudDirty.store(true);
}

// ------------------------------------------------------------- DEL / FILL job
constexpr long long kMaxCells = 4000000; // refuse bigger boxes
constexpr long long kBudgetMs = 250;     // server-thread time per trigger
enum JobMode { kNone = 0, kDel = 1, kFill = 2 };

std::mutex gDelMu;
std::atomic<bool> gDelActive{false}; // true while any job is running
int gJobMode = kNone;
int gDelMin[3], gDelMax[3], gDelCur[3];
long long gDelTotal = 0, gDelDone = 0, gDelRemoved = 0, gDelFailed = 0;
void *gDelAir = nullptr;
std::string gDelMsg;
bool gFillOk = false; // helper functions resolved
int gFillP1[3];            // Pos1 = the block type to fill with
int gFillStrategy = 0;     // 0 = not found yet, 1 setBlock, 2 useItemOn, 3 buildBlock
uintptr_t gFillRegion = 0; // BlockSource that takes setBlock writes
uintptr_t gFillReadBs = 0; // BlockSource used to read the result back
int gFillFlags = 3;
void *gFillMat = nullptr;  // Block* of the material

HandItemFn gHandItem = nullptr;
ItemBlockTypeFn gItemBlockType = nullptr;

bool stackEmpty(const void *stack) {
  if (!stack)
    return true;
  const uintptr_t st = reinterpret_cast<uintptr_t>(stack);
  unsigned char count = 0;
  if (!safeRead(reinterpret_cast<void *>(st + 0x23), &count, 1))
    return true;
  if (count == 0)
    return true;
  uintptr_t w = 0, v = 0;
  if (!rdp(st + 8, w) || !w)
    return true;
  return !rdp(w, v) || !v;
}

bool isBlockItem(const void *stack) {
  if (!stack || !gItemBlockType || stackEmpty(stack))
    return false;
  const uintptr_t bt = reinterpret_cast<uintptr_t>(gItemBlockType(stack).blockType);
  return bt > 0x10000;
}

void startOrCancelJob(int mode) {
  std::lock_guard<std::mutex> lk(gDelMu);
  if (gDelActive.load()) {
    if (gJobMode == mode) { // tap the same button again = cancel
      gDelActive.store(false);
      gDelMsg = "Job cancelled";
    } else {
      gDelMsg = "Finish or cancel the current job first";
    }
    gHudDirty.store(true);
    return;
  }
  if (mode == kFill && !gFillOk) {
    gDelMsg = "FILL unavailable on this game version";
    gHudDirty.store(true);
    return;
  }
  int a[3], b[3];
  bool ok;
  {
    std::lock_guard<std::mutex> sk(gSelMu);
    ok = gHave[0] && gHave[1];
    std::memcpy(a, gPos[0], sizeof(a));
    std::memcpy(b, gPos[1], sizeof(b));
  }
  if (!ok) {
    gDelMsg = "Select Pos1 and Pos2 first";
    gHudDirty.store(true);
    return;
  }
  long long vol = 1;
  for (int i = 0; i < 3; ++i) {
    gDelMin[i] = std::min(a[i], b[i]);
    gDelMax[i] = std::max(a[i], b[i]);
    vol *= (gDelMax[i] - gDelMin[i] + 1);
  }
  if (vol > kMaxCells) {
    gDelMsg = "Selection too large (max 4,000,000)";
    gHudDirty.store(true);
    return;
  }
  gDelCur[0] = gDelMin[0];
  gDelCur[1] = (mode == kDel) ? gDelMax[1] : gDelMin[1]; // DEL top-down
  gDelCur[2] = gDelMin[2];
  gDelTotal = vol;
  gDelDone = gDelRemoved = gDelFailed = 0;
  gDelAir = nullptr;
  if (mode == kFill) {
    std::memcpy(gFillP1, a, sizeof(a));
    gFillStrategy = 0;
    gFillRegion = gFillReadBs = 0;
    gFillMat = nullptr;
  }
  gJobMode = mode;
  gDelMsg = (mode == kDel)
                ? "DEL ready: break any block to run"
                : "FILL ready (Pos1 block = material): break any block to run";
  gDelActive.store(true);
  gHudDirty.store(true);
  char buf[160];
  snprintf(buf, sizeof(buf), "%s start box (%d,%d,%d)-(%d,%d,%d) cells=%lld",
           mode == kDel ? "DEL" : "FILL", gDelMin[0], gDelMin[1], gDelMin[2],
           gDelMax[0], gDelMax[1], gDelMax[2], vol);
  logLine(buf);
}

// ------------------------------------------------------------------ HUD
pl::modmenu::DrawCommand makeRect(float x, float y, float w, float h,
                                  uint32_t color, float radius) {
  pl::modmenu::DrawCommand d;
  d.type = pl::modmenu::DrawCommandType::RectFilled;
  d.x = x;
  d.y = y;
  d.w = w;
  d.h = h;
  d.x3 = radius;
  d.color = color;
  return d;
}

// align: 0 left, -2 center, -1 right (anchor is x)
pl::modmenu::DrawCommand makeText(const std::string &s, float x, float baseY,
                                  float size, uint32_t color, float align) {
  pl::modmenu::DrawCommand d;
  d.type = pl::modmenu::DrawCommandType::Text;
  d.x = x;
  d.y = baseY;
  d.w = align;
  d.size = size;
  d.color = color;
  d.text = s;
  return d;
}

bool gHudShown = false;

void renderHud() {
  if (!gToolsOn.load() || !gameHudVisible()) {
    if (gHudShown) {
      pl::modmenu::submitDrawCommands(kToolsId,
                                      std::vector<pl::modmenu::DrawCommand>{});
      gHudShown = false;
    }
    std::lock_guard<std::mutex> lk(gGeoMu);
    gGeo.bs = 0; // not touchable while hidden
    return;
  }
  const HudSize surf = hudSize();
  if (surf.width <= 0.0f || surf.height <= 0.0f)
    return;
  const float W = surf.width;
  const float H = surf.height;
  const float s = std::clamp(H * 0.13f, 84.0f, 150.0f);
  const float bx = W - s - W * 0.03f;
  const float by = H * 0.22f;
  const float dy = by + s * 1.15f;
  const float fy = by + s * 2.3f;
  {
    std::lock_guard<std::mutex> lk(gGeoMu);
    gGeo.bx = bx;
    gGeo.by = by;
    gGeo.bs = s;
    gGeo.dy = dy;
    gGeo.fy = fy;
  }

  bool armed = gArmed.load(), h0, h1;
  int p0[3], p1[3], next;
  {
    std::lock_guard<std::mutex> lk(gSelMu);
    h0 = gHave[0];
    h1 = gHave[1];
    std::memcpy(p0, gPos[0], sizeof(p0));
    std::memcpy(p1, gPos[1], sizeof(p1));
    next = gNext;
  }
  bool delOn;
  int mode;
  long long dTotal, dDone, dRemoved, dFailed;
  std::string dMsg;
  {
    std::lock_guard<std::mutex> lk(gDelMu);
    delOn = gDelActive.load();
    mode = gJobMode;
    dTotal = gDelTotal;
    dDone = gDelDone;
    dRemoved = gDelRemoved;
    dFailed = gDelFailed;
    dMsg = gDelMsg;
  }

  std::vector<pl::modmenu::DrawCommand> cmds;
  cmds.push_back(makeRect(bx, by, s, s, armed ? 0xE6D32F2F : 0xB0303030,
                          s * 0.2f));
  const char *label = armed ? (next == 0 ? "P1" : "P2") : "SEL";
  cmds.push_back(makeText(label, bx + s * 0.5f, by + s * 0.5f + s * 0.09f,
                          s * 0.26f, 0xFFFFFFFF, -2.0f));
  const bool delRun = delOn && mode == kDel;
  const bool fillRun = delOn && mode == kFill;
  cmds.push_back(makeRect(bx, dy, s, s, delRun ? 0xE6D32F2F : 0xB0303030,
                          s * 0.2f));
  cmds.push_back(makeText(delRun ? "STOP" : "DEL", bx + s * 0.5f,
                          dy + s * 0.5f + s * 0.09f, s * 0.26f, 0xFFFFFFFF,
                          -2.0f));
  cmds.push_back(makeRect(bx, fy, s, s, fillRun ? 0xE6D32F2F : 0xB0303030,
                          s * 0.2f));
  cmds.push_back(makeText(fillRun ? "STOP" : "FILL", bx + s * 0.5f,
                          fy + s * 0.5f + s * 0.09f, s * 0.26f, 0xFFFFFFFF,
                          -2.0f));

  const float ts = s * 0.2f;
  float ty = by + ts;
  auto line = [&](const std::string &t) {
    const float w = static_cast<float>(t.size()) * ts * 0.58f + ts;
    cmds.push_back(makeRect(bx - w - 10.0f, ty - ts * 1.05f, w, ts * 1.5f,
                            0x99000000, ts * 0.3f));
    cmds.push_back(makeText(t, bx - 10.0f - ts * 0.5f, ty, ts, 0xFFFFFFFF, -1.0f));
    ty += ts * 1.7f;
  };
  char buf[96];
  if (h0) {
    snprintf(buf, sizeof(buf), "Pos1: %d, %d, %d", p0[0], p0[1], p0[2]);
    line(buf);
  }
  if (h1) {
    snprintf(buf, sizeof(buf), "Pos2: %d, %d, %d", p1[0], p1[1], p1[2]);
    line(buf);
    const long long n = 1ll * (std::abs(p1[0] - p0[0]) + 1) *
                        (std::abs(p1[1] - p0[1]) + 1) *
                        (std::abs(p1[2] - p0[2]) + 1);
    snprintf(buf, sizeof(buf), "%lld blocks", n);
    line(buf);
  }
  if (delOn) {
    snprintf(buf, sizeof(buf), "%s %lld/%lld  %s %lld",
             mode == kDel ? "DEL" : "FILL", dDone, dTotal,
             mode == kDel ? "removed" : "placed", dRemoved);
    line(buf);
    if (mode == kFill && dFailed > 0) {
      snprintf(buf, sizeof(buf), "skipped (no support/refused): %lld", dFailed);
      line(buf);
    }
  }
  if (!dMsg.empty())
    line(dMsg);
  pl::modmenu::submitDrawCommands(kToolsId, cmds);
  gHudShown = true;
}

void uiLoop() {
  long long last = 0;
  bool lastVis = false;
  float lw = 0, lh = 0;
  while (!gStop.load()) {
    const bool vis = gameHudVisible();
    const HudSize surf = hudSize();
    const bool changed = vis != lastVis || surf.width != lw || surf.height != lh;
    const bool delOn = gDelActive.load();
    if (gHudDirty.exchange(false) || changed || nowMs() - last > 1500 ||
        (delOn && nowMs() - last > 300)) {
      renderHud();
      last = nowMs();
      lastVis = vis;
      lw = surf.width;
      lh = surf.height;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  gLoopRunning.store(false);
}

bool onTouch(const pl::input::TouchEvent &ev) {
  static int ptr = -1;
  static int which = -1; // 0 = SEL, 1 = DEL, 2 = FILL
  if (!gToolsOn.load() || !gameHudVisible()) {
    ptr = -1;
    return false;
  }
  Geo g;
  {
    std::lock_guard<std::mutex> lk(gGeoMu);
    g = gGeo;
  }
  if (g.bs <= 0)
    return false;
  auto hit = [&](float top) {
    return ev.x >= g.bx && ev.x < g.bx + g.bs && ev.y >= top &&
           ev.y < top + g.bs;
  };
  auto which_of = [&]() {
    if (hit(g.by))
      return 0;
    if (hit(g.dy))
      return 1;
    if (hit(g.fy))
      return 2;
    return -1;
  };
  switch (ev.action) {
  case 0:
  case 5:
    if (ptr < 0 && which_of() >= 0) {
      ptr = ev.pointerId;
      which = which_of();
      return true;
    }
    break;
  case 2:
    if (ptr == ev.pointerId)
      return true;
    break;
  case 1:
  case 6:
    if (ptr == ev.pointerId) {
      const int w = which;
      const bool same = which_of() == w;
      ptr = -1;
      which = -1;
      if (same) {
        if (w == 0)
          toggleArmed();
        else if (w == 1)
          startOrCancelJob(kDel);
        else if (w == 2)
          startOrCancelJob(kFill);
      }
      return true;
    }
    break;
  case 3:
    if (ptr >= 0) {
      ptr = -1;
      which = -1;
      return true;
    }
    break;
  }
  return false;
}

// ------------------------------------------- game hooks (server / client)
Fn gBuildOrig = nullptr;
Fn gGmDestroyOrig = nullptr;
Fn gLpTickOrig = nullptr;
GetBlockFn gGetBlock = nullptr;
std::atomic<uintptr_t> gLocalPlayer{0};

// Tap on a block: while SEL is armed the tap becomes a selection point.
uintptr_t buildDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                      void *g, void *h) {
  if (gArmed.load(std::memory_order_relaxed)) {
    handleTap(b, c);
    return 0; // swallow the tap: no block placed
  }
  return gBuildOrig(a, b, c, d, e, f, g, h);
}

uintptr_t lpTickDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                       void *g, void *h) {
  gLocalPlayer.store(reinterpret_cast<uintptr_t>(a), std::memory_order_relaxed);
  return gLpTickOrig(a, b, c, d, e, f, g, h);
}

uintptr_t blockSourceOf(void *gm) {
  uintptr_t player = 0, dim = 0, bs = 0;
  rdp(reinterpret_cast<uintptr_t>(gm) + kGmPlayerOff, player);
  if (player)
    rdp(player + kActorDimOff, dim);
  if (dim)
    rdp(dim + kDimBsOff, bs);
  return bs;
}

// ---------------------------------------------------------------- FILL
void failJob(const std::string &m) {
  gDelMsg = m;
  gDelActive.store(false);
  gHudDirty.store(true);
  logLine(m);
}

// Called with gDelMu held, on the server thread. B0 = dimension BlockSource.
// Replaces every cell of the box that is not the material (the block at Pos1).
// Placement methods, tried in order until one verifiably works:
//   1 BlockSource::setBlock   2 GameMode::useItemOn   3 GameMode::buildBlock
void fillSlice(void *gm, void *B0, uintptr_t bs0, void *face) {
  using SetFn = uintptr_t (*)(void *, const int *, void *, int, void *, void *);
  alignas(16) unsigned char ctx[256];
  std::memset(ctx, 0, sizeof(ctx));
  uintptr_t player = 0;
  rdp(reinterpret_cast<uintptr_t>(gm) + kGmPlayerOff, player);

  if (!gFillMat) {
    gFillMat = gGetBlock(B0, gFillP1);
    if (!gFillMat || gFillMat == gDelAir) {
      failJob("FILL: Pos1 must be a block of the wanted type (not air)");
      return;
    }
  }
  auto advance = [&]() -> bool { // true when the box is finished
    if (++gDelCur[0] > gDelMax[0]) {
      gDelCur[0] = gDelMin[0];
      if (++gDelCur[2] > gDelMax[2]) {
        gDelCur[2] = gDelMin[2];
        if (++gDelCur[1] > gDelMax[1])
          return true;
      }
    }
    return false;
  };
  auto finish = [&]() {
    gDelActive.store(false);
    char buf[110];
    snprintf(buf, sizeof(buf), "FILL done: %lld placed, %lld failed",
             gDelRemoved, gDelFailed);
    gDelMsg = buf;
    logLine(buf);
  };

  struct Nb {
    int dx, dy, dz;
    unsigned char face;
    float nx, ny, nz;
  };
  static const Nb nbs[6] = {{0, -1, 0, 1, 0, 1, 0},  {-1, 0, 0, 5, 1, 0, 0},
                            {1, 0, 0, 4, -1, 0, 0},  {0, 0, -1, 3, 0, 0, 1},
                            {0, 0, 1, 2, 0, 0, -1},  {0, 1, 0, 0, 0, -1, 0}};
  const void *stack = nullptr;
  UseItemOnFn useOn = nullptr;
  auto prepHeld = [&]() -> bool {
    if (stack && useOn)
      return true;
    stack = (player && gHandItem)
                ? gHandItem(reinterpret_cast<void *>(player), 0)
                : nullptr;
    if (!isBlockItem(stack))
      return false;
    uintptr_t vt = 0, fn = 0;
    if (rdp(reinterpret_cast<uintptr_t>(gm), vt) && vt)
      rdp(vt + 8 * kUseItemOnSlot, fn);
    useOn = reinterpret_cast<UseItemOnFn>(fn);
    return useOn != nullptr;
  };
  // methods 2 and 3: place against a solid neighbour of the target cell
  auto placeViaSupport = [&](const int *pos, int method) -> bool {
    for (const Nb &nb : nbs) {
      int c[3] = {pos[0] + nb.dx, pos[1] + nb.dy, pos[2] + nb.dz};
      void *cb = gGetBlock(B0, c);
      if (!cb || cb == gDelAir)
        continue;
      if (method == 2) {
        const float hit[3] = {c[0] + 0.5f + 0.5f * nb.nx,
                              c[1] + 0.5f + 0.5f * nb.ny,
                              c[2] + 0.5f + 0.5f * nb.nz};
        useOn(gm, const_cast<void *>(stack), c, nb.face, hit, cb);
      } else {
        gBuildOrig(gm, c, reinterpret_cast<void *>(static_cast<uintptr_t>(nb.face)),
                   nullptr, nullptr, nullptr, nullptr, nullptr);
      }
      if (gGetBlock(B0, pos) != gDelAir)
        return true;
    }
    return false;
  };

  // ---- find a working placement method on the first cell that needs work
  if (gFillStrategy == 0) {
    int probe[3];
    for (;;) {
      probe[0] = gDelCur[0];
      probe[1] = gDelCur[1];
      probe[2] = gDelCur[2];
      void *b = gGetBlock(B0, probe);
      if (b && b != gFillMat)
        break;
      ++gDelDone;
      if (advance()) {
        finish();
        return;
      }
    }
    std::string diag;
    // method 1: setBlock, on the dimension region and on regions found inside
    // the server player object
    uintptr_t vt = 0, fnAddr = 0;
    rdp(bs0, vt);
    if (vt)
      rdp(vt + 8 * kSetBlockSlot, fnAddr);
    if (fnAddr && matchHex(reinterpret_cast<void *>(fnAddr), kSetBlockHex)) {
      std::vector<uintptr_t> regs{bs0};
      for (uintptr_t off = 0; player && off < 0x1400; off += 8) {
        uintptr_t p = 0, vp = 0;
        if (!rdp(player + off, p) || p < 0x10000 || (p & 7))
          continue;
        if (!rdp(p, vp) || vp != gBase + kBsVtRva)
          continue;
        if (std::find(regs.begin(), regs.end(), p) == regs.end() &&
            regs.size() < 4)
          regs.push_back(p);
      }
      const int tries[2] = {3, 2};
      for (size_t i = 0; i < regs.size() && !gFillStrategy; ++i) {
        void *R = reinterpret_cast<void *>(regs[i]);
        for (int fl : tries) {
          reinterpret_cast<SetFn>(fnAddr)(R, probe, gFillMat, fl, nullptr, ctx);
          void *same = gGetBlock(R, probe);
          void *dim = gGetBlock(B0, probe);
          char t[100];
          snprintf(t, sizeof(t), "FILL S1 region%zu flags=%d same=%d dim=%d", i,
                   fl, same == gFillMat, dim == gFillMat);
          logLine(t);
          if (same == gFillMat || dim == gFillMat) {
            gFillStrategy = 1;
            gFillRegion = regs[i];
            gFillReadBs = (same == gFillMat) ? regs[i] : bs0;
            gFillFlags = fl;
            break;
          }
        }
      }
      if (!gFillStrategy)
        diag += "S1 ";
    } else {
      diag += "S1n/a ";
    }
    // methods 2 and 3 (the held block is used)
    for (int m = 2; m <= 3 && !gFillStrategy; ++m) {
      if (!prepHeld()) {
        diag += "hold-a-block ";
        break;
      }
      void *cb = gGetBlock(B0, probe);
      if (cb && cb != gDelAir)
        gGmDestroyOrig(gm, probe, face, nullptr, nullptr, nullptr, nullptr,
                       nullptr);
      if (placeViaSupport(probe, m))
        gFillStrategy = m;
      else
        diag += (m == 2 ? "S2 " : "S3 ");
    }
    if (!gFillStrategy) {
      failJob("FILL: no placement method worked (" + diag + "failed)");
      return;
    }
    char t[80];
    snprintf(t, sizeof(t), "FILL using method %d", gFillStrategy);
    logLine(t);
    ++gDelRemoved; // the probe cell
    ++gDelDone;
    if (advance()) {
      finish();
      return;
    }
  }

  // ---- fill the rest with the working method
  void *RD = (gFillStrategy == 1 && gFillReadBs)
                 ? reinterpret_cast<void *>(gFillReadBs)
                 : B0;
  void *RW = gFillRegion ? reinterpret_cast<void *>(gFillRegion) : B0;
  uintptr_t vt2 = 0, fn2 = 0;
  rdp(bs0, vt2);
  if (vt2)
    rdp(vt2 + 8 * kSetBlockSlot, fn2);
  if (gFillStrategy >= 2 && !prepHeld()) {
    failJob("FILL: hold a block in your hand");
    return;
  }
  const long long t0 = nowMs();
  unsigned n = 0;
  while (gDelActive.load()) {
    int pos[3] = {gDelCur[0], gDelCur[1], gDelCur[2]};
    void *curB = gGetBlock(RD, pos);
    if (curB && curB != gFillMat) {
      bool ok;
      if (gFillStrategy == 1) {
        ok = (reinterpret_cast<SetFn>(fn2)(RW, pos, gFillMat, gFillFlags,
                                           nullptr, ctx) &
              0xff) != 0;
      } else {
        if (curB != gDelAir)
          gGmDestroyOrig(gm, pos, face, nullptr, nullptr, nullptr, nullptr,
                         nullptr);
        ok = placeViaSupport(pos, gFillStrategy);
      }
      if (ok)
        ++gDelRemoved;
      else
        ++gDelFailed;
    }
    ++gDelDone;
    if (advance()) {
      finish();
      break;
    }
    if ((++n & 31u) == 0 && nowMs() - t0 > kBudgetMs) {
      gDelMsg = "Paused: break a block to continue";
      break;
    }
  }
  gHudDirty.store(true);
}

// Runs on the SERVER thread (inside the server GameMode::destroyBlock hook).
void runJobSlice(void *gm, void *face) {
  const uintptr_t bs = blockSourceOf(gm);
  if (!bs || !gGetBlock)
    return;
  void *B = reinterpret_cast<void *>(bs);
  std::lock_guard<std::mutex> lk(gDelMu);
  if (!gDelActive.load())
    return;
  if (!gDelAir) {
    int top[3] = {gDelCur[0], 319, gDelCur[2]};
    gDelAir = gGetBlock(B, top);
    if (!gDelAir) {
      gDelMsg = "Could not read the world";
      gDelActive.store(false);
      gHudDirty.store(true);
      return;
    }
  }
  if (gJobMode == kFill) {
    fillSlice(gm, B, bs, face);
    gHudDirty.store(true);
    return;
  }

  // (legacy FILL path below is no longer reached)
  // FILL needs the held block and the virtual useItemOn of this GameMode.
  const void *stack = nullptr;
  uintptr_t player = 0;
  UseItemOnFn useOn = nullptr;
  if (gJobMode == kFill) {
    rdp(reinterpret_cast<uintptr_t>(gm) + kGmPlayerOff, player);
    stack = (player && gHandItem) ? gHandItem(reinterpret_cast<void *>(player), 0)
                                  : nullptr;
    if (!isBlockItem(stack)) {
      gDelMsg = "FILL: hold a block in your hand, then break a block";
      gHudDirty.store(true);
      return;
    }
    uintptr_t vt = 0, fn = 0;
    if (rdp(reinterpret_cast<uintptr_t>(gm), vt) && vt)
      rdp(vt + 8 * kUseItemOnSlot, fn);
    if (!fn) {
      gDelMsg = "FILL: placement function not found";
      gDelActive.store(false);
      gHudDirty.store(true);
      return;
    }
    useOn = reinterpret_cast<UseItemOnFn>(fn);
  }

  struct Nb {
    int dx, dy, dz; // from the target cell to the supporting block
    unsigned char face;
    float nx, ny, nz; // normal of the clicked face (points to the target)
  };
  static const Nb nbs[6] = {{0, -1, 0, 1, 0, 1, 0},  {-1, 0, 0, 5, 1, 0, 0},
                            {1, 0, 0, 4, -1, 0, 0},  {0, 0, -1, 3, 0, 0, 1},
                            {0, 0, 1, 2, 0, 0, -1},  {0, 1, 0, 0, 0, -1, 0}};

  const long long t0 = nowMs();
  unsigned n = 0;
  long long consecutiveFail = 0;
  while (gDelActive.load()) {
    int pos[3] = {gDelCur[0], gDelCur[1], gDelCur[2]};
    void *cur = gGetBlock(B, pos);
    if (gJobMode == kDel) {
      if (cur && cur != gDelAir) {
        gGmDestroyOrig(gm, pos, face, nullptr, nullptr, nullptr, nullptr,
                       nullptr);
        ++gDelRemoved;
      }
    } else if (cur == gDelAir) { // FILL: only empty cells
      bool placed = false, hadSupport = false;
      for (const Nb &nb : nbs) {
        int c[3] = {pos[0] + nb.dx, pos[1] + nb.dy, pos[2] + nb.dz};
        void *cb = gGetBlock(B, c);
        if (!cb || cb == gDelAir)
          continue;
        hadSupport = true;
        const float hit[3] = {c[0] + 0.5f + 0.5f * nb.nx,
                              c[1] + 0.5f + 0.5f * nb.ny,
                              c[2] + 0.5f + 0.5f * nb.nz};
        useOn(gm, const_cast<void *>(stack), c, nb.face, hit, cb);
        if (gGetBlock(B, pos) != gDelAir) {
          placed = true;
          break;
        }
      }
      if (placed) {
        ++gDelRemoved;
        consecutiveFail = 0;
      } else {
        ++gDelFailed;
        ++consecutiveFail;
        (void)hadSupport;
        if (gDelRemoved == 0 && consecutiveFail >= 30) {
          gDelMsg = "FILL: the game refused to place (need a solid neighbour)";
          logLine(gDelMsg);
          gDelActive.store(false);
          break;
        }
      }
      if ((n & 63u) == 63u && stackEmpty(stack)) {
        gDelMsg = "FILL paused: out of blocks";
        break;
      }
    }
    ++gDelDone;
    // advance: x, then z, then y (DEL downwards, FILL upwards)
    if (++gDelCur[0] > gDelMax[0]) {
      gDelCur[0] = gDelMin[0];
      if (++gDelCur[2] > gDelMax[2]) {
        gDelCur[2] = gDelMin[2];
        bool finished;
        if (gJobMode == kDel)
          finished = --gDelCur[1] < gDelMin[1];
        else
          finished = ++gDelCur[1] > gDelMax[1];
        if (finished) {
          gDelActive.store(false);
          char buf[96];
          snprintf(buf, sizeof(buf), "%s done: %lld %s",
                   gJobMode == kDel ? "DEL" : "FILL", gDelRemoved,
                   gJobMode == kDel ? "blocks removed" : "blocks placed");
          gDelMsg = buf;
          logLine(buf);
          break;
        }
      }
    }
    if ((++n & 31u) == 0 && nowMs() - t0 > kBudgetMs) {
      gDelMsg = "Paused: break a block to continue";
      break;
    }
  }
  gHudDirty.store(true);
}

uintptr_t gmDestroyDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                          void *g, void *h) {
  const uintptr_t r = gGmDestroyOrig(a, b, c, d, e, f, g, h);
  if (gDelActive.load(std::memory_order_relaxed)) {
    uintptr_t player = 0;
    rdp(reinterpret_cast<uintptr_t>(a) + kGmPlayerOff, player);
    if (player && player != gLocalPlayer.load()) // server GameMode only
      runJobSlice(a, c);
  }
  return r;
}

void onToggle(std::string_view id, bool enabled) {
  if (id == kModuleId) {
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(enabled);
    saveState(enabled);
  } else if (id == kToolsId) {
    gToolsOn.store(enabled);
    if (!enabled) {
      gArmed.store(false);
      gDelActive.store(false);
    }
    gHudDirty.store(true);
  }
}

} // namespace

class NoUpdatesMod {
public:
  static NoUpdatesMod &instance() {
    static NoUpdatesMod mod;
    return mod;
  }

  NoUpdatesMod() : mSelf(*ll::mod::NativeMod::current()) {}

  bool load() { return true; }

  bool enable() {
    auto &log = mSelf.getLogger();
    uintptr_t base = findBase();
    if (!base) {
      log.info("libminecraftpe.so not found");
      return false;
    }
    gBase = base;
    {
      const char *cands[] = {"/sdcard/Download/axiom_log3.txt",
                             "/sdcard/axiom_log3.txt"};
      gLogPath.clear();
      for (const char *c : cands) {
        std::ofstream t(c, std::ios::app);
        if (t.good()) {
          gLogPath = c;
          break;
        }
      }
      if (gLogPath.empty()) {
        std::string d = filesDir();
        gLogPath = d.empty() ? "/dev/null" : d + "axiom_log3.txt";
      }
    }
    logLine("--- axiom v1.1 start --- path=" + gLogPath);

    const bool savedOn = loadState();
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(savedOn);

    // ---- No Updates hooks
    static const auto detours = makeDetours(std::make_index_sequence<kCount>{});
    int ok = 0, bad = 0;
    for (size_t i = 0; i < kCount; ++i) {
      auto *addr = reinterpret_cast<void *>(base + kTargets[i].rva);
      if (std::memcmp(addr, kExpected, sizeof(kExpected)) != 0) {
        ++bad;
        continue;
      }
      mHooks[i] = pl::memory::HookHandle(
          addr, detours[i], reinterpret_cast<void **>(&gOrig[i]),
          pl::memory::HookPriority::Normal);
      if (mHooks[i].installed())
        ++ok;
      else
        ++bad;
    }
    log.info("{}/{} hooks installed ({} skipped)", ok, static_cast<int>(kCount),
             bad);
    if (ok == 0)
      return false;

    // ---- game hooks (tap, destroy, local player) + getBlock
    auto install = [&](uintptr_t rva, const char *hex, void *detourFn, Fn *orig,
                       pl::memory::HookHandle &h) {
      auto *addr = reinterpret_cast<void *>(base + rva);
      if (!matchHex(addr, hex))
        return false;
      h = pl::memory::HookHandle(addr, detourFn,
                                 reinterpret_cast<void **>(orig),
                                 pl::memory::HookPriority::Normal);
      return h.installed();
    };
    const bool tapOk = install(kBuildRva, kBuildHex,
                               reinterpret_cast<void *>(&buildDetour),
                               &gBuildOrig, mBuildHook);
    const bool delOk = install(kGmDestroyRva, kGmDestroyHex,
                               reinterpret_cast<void *>(&gmDestroyDetour),
                               &gGmDestroyOrig, mGmDestroyHook);
    const bool lpOk = install(kLpTickRva, kLpTickHex,
                              reinterpret_cast<void *>(&lpTickDetour),
                              &gLpTickOrig, mLpTickHook);
    auto *gbAddr = reinterpret_cast<void *>(base + kGetBlockRva);
    const bool gbOk =
        matchPattern(gbAddr, kGetBlockPat, sizeof(kGetBlockPat) / sizeof(int));
    gGetBlock = gbOk ? reinterpret_cast<GetBlockFn>(gbAddr) : nullptr;
    auto *hiAddr = reinterpret_cast<void *>(base + kHandItemRva);
    auto *btAddr = reinterpret_cast<void *>(base + kItemBlockTypeRva);
    auto *uoAddr = reinterpret_cast<void *>(base + kUseItemOnRva);
    const bool hiOk =
        matchPattern(hiAddr, kHandItemPat, sizeof(kHandItemPat) / sizeof(int));
    const bool btOk = matchPattern(btAddr, kItemBlockTypePat,
                                   sizeof(kItemBlockTypePat) / sizeof(int));
    const bool uoOk = matchHex(uoAddr, kUseItemOnHex);
    gHandItem = hiOk ? reinterpret_cast<HandItemFn>(hiAddr) : nullptr;
    gItemBlockType = btOk ? reinterpret_cast<ItemBlockTypeFn>(btAddr) : nullptr;
    gFillOk = gbOk;
    logLine("fill helpers: handItem=" + std::to_string(hiOk) +
            " itemBlockType=" + std::to_string(btOk) +
            " useItemOn=" + std::to_string(uoOk));
    logLine("hooks: noupdates=" + std::to_string(ok) +
            " tap=" + std::to_string(tapOk) + " destroy=" +
            std::to_string(delOk) + " localPlayer=" + std::to_string(lpOk) +
            " getBlock=" + std::to_string(gbOk));
    mTapOk = tapOk;

    // ---- menu
    bool r = pl::modmenu::ModuleBuilder(kModuleId, "No Updates")
                 .modId(mSelf.getId())
                 .description("Freezes block updates: sand, gravel, water, "
                              "lava, redstone, plants, fire, ladders...")
                 .defaultEnabled(savedOn)
                 .onToggle(onToggle)
                 .registerModule();
    logLine("step: No Updates done");

    if (tapOk) {
      r &= pl::modmenu::ModuleBuilder(kToolsId, "Axiom Tools")
               .modId(mSelf.getId())
               .description("SEL: tap it, then tap two blocks (Pos1, Pos2). "
                            "DEL: removes the selection. FILL: fills the empty "
                            "cells with the block in your hand. Break any "
                            "block to run a job (creative world).")
               .defaultEnabled(true)
               .onToggle(onToggle)
               .registerModule();
      logLine("step: Axiom Tools done");

      findJni();
      if (!gTouchRegistered.exchange(true)) {
        pl::input::registerTouchCallback(
            [](const pl::input::TouchEvent &ev) { return onTouch(ev); });
      }
      gStop.store(false);
      gToolsOn.store(true);
      if (!gLoopRunning.exchange(true))
        std::thread([] { uiLoop(); }).detach();
      gHudDirty.store(true);
      logLine("step: HUD loop started");
    }
    return r;
  }

  bool disable() {
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(false);
    gArmed.store(false);
    gDelActive.store(false);
    gToolsOn.store(false);
    gStop.store(true);
    pl::modmenu::unregisterModule(kToolsId);
    pl::modmenu::unregisterModule(kModuleId);
    mGmDestroyHook.reset();
    mLpTickHook.reset();
    mBuildHook.reset();
    for (auto &h : mHooks)
      h.reset();
    return true;
  }

  bool unload() { return true; }

private:
  ll::mod::NativeMod &mSelf;
  bool mTapOk = false;
  std::array<pl::memory::HookHandle, kCount> mHooks;
  pl::memory::HookHandle mBuildHook;
  pl::memory::HookHandle mGmDestroyHook;
  pl::memory::HookHandle mLpTickHook;
};

PL_REGISTER_MOD(NoUpdatesMod, NoUpdatesMod::instance())
