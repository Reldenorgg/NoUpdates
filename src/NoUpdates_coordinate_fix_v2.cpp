// Axiom for Levi v1.0 - Minecraft Bedrock 26.52.3 (arm64)
//  * No Updates : freezes block events (physics, redstone, ticks)
//  * SEL        : HUD button; next two block taps set Pos1 / Pos2
//  * DEL        : HUD button; removes every block of the selection through
//                 the server-side GameMode::destroyBlock (works out of reach).
//                 The job runs when you break any block (server thread).
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

// Coordinate-fix test: use the BlockPos received from buildBlock unchanged.
// Keep diagnostic logs so the in-game result can be checked against the raw input.
void handleTap(void *posPtr, void *faceArg) {
  int pos[3];
  if (!safeRead(posPtr, pos, sizeof(pos))) {
    logLine("DIAG_BUILD_CAPTURE_V2 safeRead_failed");
    return;
  }
  const int raw[3] = {pos[0], pos[1], pos[2]};
  const unsigned face =
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(faceArg) & 0xff);
  char diag[192];
  snprintf(diag, sizeof(diag),
           "DIAG_BUILD_CAPTURE_V2 raw=(%d,%d,%d) face=%u posPtr=%p faceArg=%p",
           raw[0], raw[1], raw[2], face, posPtr, faceArg);
  logLine(diag);
  // Intentionally do not offset by the face normal: store the exact BlockPos
  // passed to buildBlock, avoiding the previous one-block coordinate shift.
  snprintf(diag, sizeof(diag),
           "DIAG_STORED_V2 raw=(%d,%d,%d) stored=(%d,%d,%d) face=%u",
           raw[0], raw[1], raw[2], pos[0], pos[1], pos[2], face);
  logLine(diag);
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

// -------------------------------------------------------------- DEL job
constexpr long long kMaxCells = 4000000; // refuse bigger boxes
constexpr long long kBudgetMs = 200;     // server-thread time per trigger

std::mutex gDelMu;
std::atomic<bool> gDelActive{false};
int gDelMin[3], gDelMax[3], gDelCur[3];
long long gDelTotal = 0, gDelDone = 0, gDelRemoved = 0;
void *gDelAir = nullptr;
std::string gDelMsg;

void startOrCancelDel() {
  if (gDelActive.load()) { // tap again = cancel
    std::lock_guard<std::mutex> lk(gDelMu);
    gDelActive.store(false);
    gDelMsg = "DEL cancelled";
    gHudDirty.store(true);
    return;
  }
  int a[3], b[3];
  bool ok;
  {
    std::lock_guard<std::mutex> lk(gSelMu);
    ok = gHave[0] && gHave[1];
    std::memcpy(a, gPos[0], sizeof(a));
    std::memcpy(b, gPos[1], sizeof(b));
  }
  std::lock_guard<std::mutex> lk(gDelMu);
  if (!ok) {
    gDelMsg = "DEL: select Pos1 and Pos2 first";
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
    gDelMsg = "DEL: selection too large (max 4,000,000)";
    gHudDirty.store(true);
    return;
  }
  gDelCur[0] = gDelMin[0];
  gDelCur[1] = gDelMax[1]; // top to bottom
  gDelCur[2] = gDelMin[2];
  gDelTotal = vol;
  gDelDone = gDelRemoved = 0;
  gDelAir = nullptr;
  gDelMsg = "DEL ready: break any block to run";
  gDelActive.store(true);
  gHudDirty.store(true);
  char buf[140];
  snprintf(buf, sizeof(buf), "DEL start box (%d,%d,%d)-(%d,%d,%d) cells=%lld",
           gDelMin[0], gDelMin[1], gDelMin[2], gDelMax[0], gDelMax[1],
           gDelMax[2], vol);
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
  {
    std::lock_guard<std::mutex> lk(gGeoMu);
    gGeo.bx = bx;
    gGeo.by = by;
    gGeo.bs = s;
    gGeo.dy = dy;
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
  long long dTotal, dDone, dRemoved;
  std::string dMsg;
  {
    std::lock_guard<std::mutex> lk(gDelMu);
    delOn = gDelActive.load();
    dTotal = gDelTotal;
    dDone = gDelDone;
    dRemoved = gDelRemoved;
    dMsg = gDelMsg;
  }

  std::vector<pl::modmenu::DrawCommand> cmds;
  cmds.push_back(makeRect(bx, by, s, s, armed ? 0xE6D32F2F : 0xB0303030,
                          s * 0.2f));
  const char *label = armed ? (next == 0 ? "P1" : "P2") : "SEL";
  cmds.push_back(makeText(label, bx + s * 0.5f, by + s * 0.5f + s * 0.09f,
                          s * 0.26f, 0xFFFFFFFF, -2.0f));
  cmds.push_back(makeRect(bx, dy, s, s, delOn ? 0xE6D32F2F : 0xB0303030,
                          s * 0.2f));
  cmds.push_back(makeText(delOn ? "STOP" : "DEL", bx + s * 0.5f,
                          dy + s * 0.5f + s * 0.09f, s * 0.26f, 0xFFFFFFFF,
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
    snprintf(buf, sizeof(buf), "DEL %lld/%lld  removed %lld", dDone, dTotal,
             dRemoved);
    line(buf);
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
  static int which = -1; // 0 = SEL, 1 = DEL
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
  auto inside = [&](float top) {
    return ev.x >= g.bx && ev.x < g.bx + g.bs && ev.y >= top &&
           ev.y < top + g.bs;
  };
  const bool inSel = inside(g.by);
  const bool inDel = inside(g.dy);
  switch (ev.action) {
  case 0:
  case 5:
    if (ptr < 0 && (inSel || inDel)) {
      ptr = ev.pointerId;
      which = inSel ? 0 : 1;
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
      ptr = -1;
      which = -1;
      if (w == 0 && inSel)
        toggleArmed();
      else if (w == 1 && inDel)
        startOrCancelDel();
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

// Runs on the SERVER thread (inside the server GameMode::destroyBlock hook).
void runDelSlice(void *gm, void *face) {
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
      gDelMsg = "DEL: could not read the world";
      gDelActive.store(false);
      gHudDirty.store(true);
      return;
    }
  }
  const long long t0 = nowMs();
  unsigned n = 0;
  while (gDelActive.load()) {
    int pos[3] = {gDelCur[0], gDelCur[1], gDelCur[2]};
    void *blk = gGetBlock(B, pos);
    if (blk && blk != gDelAir) {
      gGmDestroyOrig(gm, pos, face, nullptr, nullptr, nullptr, nullptr,
                     nullptr);
      ++gDelRemoved;
    }
    ++gDelDone;
    // advance: x, then z, then y downwards
    if (++gDelCur[0] > gDelMax[0]) {
      gDelCur[0] = gDelMin[0];
      if (++gDelCur[2] > gDelMax[2]) {
        gDelCur[2] = gDelMin[2];
        if (--gDelCur[1] < gDelMin[1]) {
          gDelActive.store(false);
          char buf[80];
          snprintf(buf, sizeof(buf), "DEL done: %lld blocks removed",
                   gDelRemoved);
          gDelMsg = buf;
          logLine(buf);
          break;
        }
      }
    }
    if ((++n & 63u) == 0 && nowMs() - t0 > kBudgetMs) {
      gDelMsg = "DEL paused: break a block to continue";
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
      runDelSlice(a, c);
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
    logLine("--- axiom v1.0 start --- path=" + gLogPath);
    logLine("DIAG_VERSION=selection-coordinates-v2 (stores raw BlockPos; no face offset)");

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
                            "DEL: removes the selection (needs a creative "
                            "world; break a block to run it).")
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
