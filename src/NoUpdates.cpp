// Axiom for Levi v0.3 - Minecraft Bedrock 26.52.3 (arm64)
//  * No Updates: freezes block events (physics, redstone, ticks)
//  * Select: HUD button arms selection; next two block taps set Pos1 / Pos2
// HUD and touch follow the pattern used by other Levi mods: own draw
// commands + pl::input touch callback (no ButtonBuilder).
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
#include <functional>
#include <mutex>
#include <set>
#include <tuple>
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

// GameMode::buildBlock (vtable slot 7), 26.52.3
constexpr uintptr_t kBuildRva = 0xf8a117c;
constexpr const char *kBuildHex = "ffc304d1fd7b0da9fc7300f9fa670fa9";

using Fn = uintptr_t (*)(void *, void *, void *, void *, void *, void *,
                         void *, void *);

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
bool safeRead(const void *p, void *out, size_t n) {
  if (!p)
    return false;
  struct iovec l{out, n};
  struct iovec r{const_cast<void *>(p), n};
  return process_vm_readv(getpid(), &l, 1, &r, 1, 0) ==
         static_cast<ssize_t>(n);
}

bool matchHex(const void *p, const char *hex) {
  auto nib = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
  const unsigned char *b = static_cast<const unsigned char *>(p);
  for (int i = 0; i < 16; ++i)
    if (b[i] != ((nib(hex[2 * i]) << 4) | nib(hex[2 * i + 1])))
      return false;
  return true;
}

uintptr_t gBase = 0;
std::string gLogPath = "/dev/null";
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
  float bx = 0, by = 0, bs = 0;
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

// pl::modmenu::getHudSurfaceSize() returns two floats (width, height).
// Not declared in SDK 0.2.2, so it is looked up in the launcher at runtime.
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

void handleTap(void *posPtr) {
  int pos[3];
  if (!safeRead(posPtr, pos, sizeof(pos)))
    return;
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
  snprintf(buf, sizeof(buf), "SELECT pos%d = (%d,%d,%d)", idx + 1, pos[0],
           pos[1], pos[2]);
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
  const float by = H * 0.28f;
  {
    std::lock_guard<std::mutex> lk(gGeoMu);
    gGeo.bx = bx;
    gGeo.by = by;
    gGeo.bs = s;
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

  std::vector<pl::modmenu::DrawCommand> cmds;
  const uint32_t fill = armed ? 0xE6D32F2F : 0xB0303030;
  cmds.push_back(makeRect(bx, by, s, s, fill, s * 0.2f));
  const char *label = armed ? (next == 0 ? "P1" : "P2") : "SEL";
  cmds.push_back(makeText(label, bx + s * 0.5f, by + s * 0.5f + s * 0.09f,
                          s * 0.26f, 0xFFFFFFFF, -2.0f));

  const float ts = s * 0.2f;
  float ty = by + ts;
  auto line = [&](const std::string &t) {
    const float w = static_cast<float>(t.size()) * ts * 0.58f + ts;
    cmds.push_back(makeRect(bx - w - 10.0f, ty - ts * 1.05f, w, ts * 1.5f,
                            0x99000000, ts * 0.3f));
    cmds.push_back(makeText(t, bx - 10.0f - ts * 0.5f, ty, ts, 0xFFFFFFFF, -1.0f));
    ty += ts * 1.7f;
  };
  char buf[80];
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
    if (gHudDirty.exchange(false) || changed || nowMs() - last > 1500) {
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
  const bool inside = ev.x >= g.bx && ev.x < g.bx + g.bs && ev.y >= g.by &&
                      ev.y < g.by + g.bs;
  switch (ev.action) {
  case 0:
  case 5:
    if (ptr < 0 && inside) {
      ptr = ev.pointerId;
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
      ptr = -1;
      if (inside)
        toggleArmed();
      return true;
    }
    break;
  case 3:
    if (ptr >= 0) {
      ptr = -1;
      return true;
    }
    break;
  }
  return false;
}

// ----------------------------------------- GameMode::buildBlock (tap/place)
Fn gBuildOrig = nullptr;
std::atomic<bool> gWorldProbe{false};
void probeGameMode(void *gm, const char *tag);

uintptr_t buildDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                      void *g, void *h) {
  if (gArmed.load(std::memory_order_relaxed)) {
    handleTap(b);
    return 0; // swallow the tap: no block placed
  }
  if (gWorldProbe.load(std::memory_order_relaxed))
    probeGameMode(a, "build");
  return gBuildOrig(a, b, c, d, e, f, g, h);
}

// ---------------------------------------- world probe E0/E1 (read-only)
// Goal: find out whether a client and a server GameMode/BlockSource exist in
// this process, on which threads, and whether getBlock agrees on both.
constexpr uintptr_t kGmTickRva = 0xf8a38f0; // GameMode vtable slot 10
constexpr const char *kGmTickHex = "fd7bbea9f30b00f9fd030091f30300aa";
constexpr uintptr_t kGmDestroyRva = 0xf8a03ac; // GameMode vtable slot 3
constexpr const char *kGmDestroyHex = "ff0302d1fd7b04a9f85f05a9f65706a9";
constexpr uintptr_t kLpTickRva = 0xaac7dd8; // LocalPlayer vtable slot 25
constexpr const char *kLpTickHex = "e80f19fcfd7b01a9fc6f02a9fa6703a9";
constexpr uintptr_t kGetBlockRva = 0xfb91918; // BlockSource::getBlock (3rd party)
constexpr uintptr_t kGmPlayerOff = 0x8;       // GameMode -> Player
constexpr uintptr_t kActorDimOff = 448;       // Actor -> Dimension
constexpr uintptr_t kDimBsOff = 208;          // Dimension -> BlockSource
constexpr const char *kProbeId = "axiom.worldprobe";

// BlockSource::getBlock prologue (-1 = wildcard)
constexpr int kGetBlockPat[] = {
    -1, -1, -1, 0xD1, -1, -1, -1, 0xA9, -1, -1, -1, 0xA9, -1, -1, -1, 0xA9,
    -1, -1, -1, 0x91, 0x56, 0xD0, 0x3B, 0xD5, -1, -1, -1, 0xF9, -1, -1, -1,
    0xF9, -1, -1, -1, 0xB9, -1, -1, -1, 0x79, 0x1F, 0x01, 0x09, 0x6B};

bool matchPattern(const void *p, const int *pat, size_t n) {
  const unsigned char *b = static_cast<const unsigned char *>(p);
  for (size_t i = 0; i < n; ++i)
    if (pat[i] >= 0 && b[i] != pat[i])
      return false;
  return true;
}

using GetBlockFn = void *(*)(void *region, const int *pos);

std::atomic<int> gWorldCount{0};
std::atomic<uintptr_t> gLocalPlayer{0};
GetBlockFn gGetBlock = nullptr;
Fn gGmTickOrig = nullptr;
Fn gGmDestroyOrig = nullptr;
Fn gLpTickOrig = nullptr;

long tid() { return syscall(SYS_gettid); }

bool rdp(uintptr_t addr, uintptr_t &out) {
  return addr > 0x10000 && safeRead(reinterpret_cast<void *>(addr), &out, 8);
}

uintptr_t vptrRva(uintptr_t obj) {
  uintptr_t vp = 0;
  if (!rdp(obj, vp) || vp < gBase)
    return 0;
  return vp - gBase;
}

void probeGameMode(void *gm, const char *tag) {
  struct Seen {
    uintptr_t gm;
    int n;
  };
  static Seen seen[16];
  static std::mutex mu;
  int n;
  {
    std::lock_guard<std::mutex> lk(mu);
    Seen *slot = nullptr;
    for (auto &s : seen)
      if (s.gm == reinterpret_cast<uintptr_t>(gm)) {
        slot = &s;
        break;
      }
    if (!slot)
      for (auto &s : seen)
        if (!s.gm) {
          s.gm = reinterpret_cast<uintptr_t>(gm);
          slot = &s;
          break;
        }
    if (!slot)
      return;
    n = ++slot->n;
  }
  if (n > 3 || gWorldCount.load() > 80)
    return;
  uintptr_t player = 0, dim = 0, bs = 0;
  rdp(reinterpret_cast<uintptr_t>(gm) + kGmPlayerOff, player);
  if (player)
    rdp(player + kActorDimOff, dim);
  if (dim)
    rdp(dim + kDimBsOff, bs);
  void *gb = nullptr;
  uintptr_t gbVp = 0;
  bool have;
  int pos[3];
  {
    std::lock_guard<std::mutex> lk(gSelMu);
    have = gHave[0];
    std::memcpy(pos, gPos[0], sizeof(pos));
  }
  if (gGetBlock && bs && have) {
    gb = gGetBlock(reinterpret_cast<void *>(bs), pos);
    if (gb)
      gbVp = vptrRva(reinterpret_cast<uintptr_t>(gb));
  }
  char buf[420];
  snprintf(buf, sizeof(buf),
           "E0 %s #%d gm=%p tid=%ld player=%p local=%d plVp=0x%lx dim=%p bs=%p "
           "bsVp=0x%lx pos1=%d,%d,%d getBlock=%p blkVp=0x%lx",
           tag, n, gm, tid(), reinterpret_cast<void *>(player),
           player && player == gLocalPlayer.load() ? 1 : 0,
           static_cast<unsigned long>(vptrRva(player)),
           reinterpret_cast<void *>(dim), reinterpret_cast<void *>(bs),
           static_cast<unsigned long>(vptrRva(bs)), have ? pos[0] : 0,
           have ? pos[1] : 0, have ? pos[2] : 0, gb,
           static_cast<unsigned long>(gbVp));
  gWorldCount.fetch_add(1);
  logLine(buf);
}

// ---- server-thread tracer: records BlockSource / destroy callees only
// while the server GameMode::destroyBlock is running (read-only).
struct TraceTarget {
  uintptr_t rva;
  const char *hex;
  const char *name;
};
constexpr TraceTarget kTrace[] = {
    {0xfb8e8bc, "fd7bbea9f44f01a9fd03009108a44aa9", "BS0"},
    {0xfb8e9c0, "fd7bbea9f30b00f9fd030091f30300aa", "BS1"},
    {0xfb91918, "ff0301d1fd7b01a9f65702a9f44f03a9", "BS2"},
    {0xfb91ad8, "ff0301d1fd7b01a9f65702a9f44f03a9", "BS4"},
    {0xfb91b58, "ff0301d1fd7b01a9f65702a9f44f03a9", "BS5"},
    {0xfb91c18, "fd7bbda9f65701a9f44f02a9fd030091", "BS6"},
    {0xfb91cf0, "fd7bbea9f30b00f9fd030091080040f9", "BS7"},
    {0xfb99330, "ff8303d1fd7b0aa9f75b00f9f6570ca9", "BS9"},
    {0xfb9a468, "ff8301d1fd7b02a9f71b00f9f65704a9", "BS10"},
    {0xfb9a55c, "ff4302d1fd7b03a9fc6f04a9fa6705a9", "BS11"},
    {0xfb93464, "fd7bbfa9fd030091080040f9080940f9", "BS12"},
    {0xfb9bd44, "ff0301d1fd7b02a9f31b00f9fd830091", "BS13"},
    {0xfb92c10, "ff4301d1fd7b02a9f51b00f9f44f04a9", "BS14"},
    {0xfb90f34, "fd7bbea9f30b00f9fd030091080040f9", "BS15"},
    {0xfb90f80, "ffc301d1fd7b01a9fc6f02a9fa6703a9", "BS16"},
    {0xfb911ec, "ffc301d1fd7b01a9fc6f02a9fa6703a9", "BS17"},
    {0xfb910d8, "ff4302d1fd7b03a9fc6f04a9fa6705a9", "BS18"},
    {0xfb96d78, "ff8304d1ea5300fde9230b6dfd7b0ca9", "BS21"},
    {0xfb973d4, "ff4301d1fd7b03a9f44f04a9fdc30091", "BS22"},
    {0xfb97b5c, "ff4301d1fd7b03a9f44f04a9fdc30091", "BS23"},
    {0xfb98514, "ff4304d1eb2b096de9230a6dfd7b0ba9", "BS24"},
    {0xfb92f9c, "fd7bbda9f50b00f9f44f02a9fd030091", "BS25"},
    {0xfb9c244, "fd7bbda9f50b00f9f44f02a9fd030091", "BS26"},
    {0xfb90b24, "fd7bbba9f90b00f9f85f02a9f65703a9", "BS27"},
    {0xfb8f22c, "fd7bbda9f50b00f9f44f02a9fd030091", "BS28"},
    {0xfb94dcc, "ffc301d1fd7b05a9f33300f9fd430191", "BS29"},
    {0xfb95848, "ff8305d1fd7b10a9fc6f11a9fa6712a9", "BS30"},
    {0xfb91de0, "ff4301d1fd7b02a9f51b00f9f44f04a9", "BS31"},
    {0xfb9274c, "ff0302d1fd7b02a9fb1b00f9fa6704a9", "BS32"},
    {0xfb92bb4, "fd7bbda9f50b00f9f44f02a9fd030091", "BS33"},
    {0xfb9af3c, "ffc300d1fd7b01a9f44f02a9fd430091", "BS39"},
    {0xfb8f2a0, "ffc300d1fd7b01a9f31300f9fd430091", "BS40"},
    {0xfb90c64, "ff4301d1fd7b02a9f65703a9f44f04a9", "BS41"},
    {0xfb9bb68, "ff0301d1fd7b02a9f31b00f9fd830091", "BS45"},
    {0xfb93158, "ff8301d1fd7b01a9f91300f9f85f03a9", "BS46"},
    {0xfb93290, "ff8301d1fd7b01a9f91300f9f85f03a9", "BS47"},
    {0xfb93238, "ff0301d1fd7b02a9f31b00f9fd830091", "BS48"},
    {0xfb96c3c, "fd7bbea9f30b00f9fd030091089840f9", "BS49"},
    {0xfb96c7c, "ff4302d1fd7b03a9fb2300f9fa6705a9", "BS50"},
    {0xfb994a0, "e923b96dfd7b01a9fc6f02a9fa6703a9", "BS51"},
    {0xfb9944c, "ffc300d1fd7b02a9fd830091290840a9", "BS52"},
    {0xfb93630, "090040f9230840b92808402924dd40f9", "BS54"},
    {0xfb935d0, "ff0301d1fd7b02a9f31b00f9fd830091", "BS55"},
    {0xfb914f8, "ffc301d1fd7b01a9fc6f02a9fa6703a9", "BS56"},
    {0xfb936b0, "ff0304d1e84b00fdfd7b0aa9fc6f0ba9", "BS57"},
    {0xfb90df4, "fd7bbea9f44f01a9fd030091080040f9", "BS58"},
    {0xfb90ea0, "fd7bbea9f44f01a9fd030091080040f9", "BS59"},
    {0xfb8f748, "ffc300d1fd7b01a9f44f02a9fd430091", "BS60"},
    {0xfb8ea14, "ff0302d1fd7b02a9fc6f03a9fa6704a9", "BS61"},
    {0xfb8edcc, "ff4302d1fd7b03a9fc6f04a9fa6705a9", "BS62"},
    {0xfb904c0, "fd7bbca9f70b00f9f65702a9f44f03a9", "BS63"},
    {0xfb9ad38, "fd7bbda9f50b00f9f44f02a9fd030091", "BS65"},
    {0xfb94cd0, "ffc301d1fd7b02a9fa6703a9f85f04a9", "BS66"},
    {0xf560600, "fd7bbea9f44f01a9fd030091f30300aa", "D0xf560600"},
    {0xf58e894, "fd7bbea9f44f01a9fd030091f40300aa", "D0xf58e894"},
    {0xf9ecfd4, "ff8301d1fd7b03a9f52300f9f44f05a9", "D0xf9ecfd4"},
    {0x8529384, "3f2303d5fd7bbea9fc4f01a9fd030091", "D0x8529384"},
    {0xf8a0190, "fd7bbea9f30b00f9fd030091f30300aa", "D0xf8a0190"},
    {0xf56a660, "fd7bbea9f44f01a9fd030091f30300aa", "D0xf56a660"},
    {0xf8a0288, "ff8303d1fd7b0aa9f85f0ba9f6570ca9", "D0xf8a0288"},
    {0x8634428, "fd7bbea9f44f01a9fd030091140840f9", "D0x8634428"},
    {0xfa01c74, "fd7bbea9f30b00f9fd030091f30300aa", "D0xfa01c74"},
    {0xff7ff54, "fd7bbda9f50b00f9f44f02a9fd030091", "D0xff7ff54"},
    {0xf56cd18, "fd7bbea9f30b00f9fd030091080840f9", "D0xf56cd18"},
    {0xf8a0e34, "e80f1dfcfd7b01a9f44f02a9fd430091", "D0xf8a0e34"},
    {0x102cb7f0, "ff8302d1fd7b04a9fb2b00f9fa6706a9", "D0x102cb7f0"},
    {0x102cb1e8, "fd7bbaa9fc6f01a9fa6702a9f85f03a9", "D0x102cb1e8"},
};
constexpr size_t kTraceCount = sizeof(kTrace) / sizeof(kTrace[0]);

struct TraceEntry {
  uint32_t id;
  int32_t tid;
  uintptr_t a, b, c, d;
  int32_t pos[3];
  int32_t posOk;
  uintptr_t cvp;
};
constexpr size_t kRing = 4096;
TraceEntry gRing[kRing];
std::atomic<uint32_t> gRingN{0};
std::atomic<bool> gWin{false};
std::atomic<int> gTraceWindows{0};
long gPid = 0;
Fn gTraceOrig[kTraceCount]{};

// raw syscalls (no libc calls, so registers of the hooked function survive)
#if defined(__aarch64__)
inline long rawRead(const void *p, void *out, unsigned long n) {
  unsigned long l[2];
  unsigned long r[2];
  l[0] = reinterpret_cast<unsigned long>(out);
  l[1] = n;
  r[0] = reinterpret_cast<unsigned long>(p);
  r[1] = n;
  register long x0 asm("x0") = gPid;
  register long x1 asm("x1") = reinterpret_cast<long>(l);
  register long x2 asm("x2") = 1;
  register long x3 asm("x3") = reinterpret_cast<long>(r);
  register long x4 asm("x4") = 1;
  register long x5 asm("x5") = 0;
  register long x8 asm("x8") = 270; // process_vm_readv
  asm volatile("svc #0"
               : "+r"(x0)
               : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
               : "memory", "cc");
  return x0;
}
inline long rawTid() {
  register long x0 asm("x0");
  register long x8 asm("x8") = 178; // gettid
  asm volatile("svc #0" : "=r"(x0) : "r"(x8) : "memory", "cc");
  return x0;
}
#else
inline long rawRead(const void *, void *, unsigned long) { return -1; }
inline long rawTid() { return 0; }
#endif

template <size_t I>
uintptr_t traceDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                      void *g, void *h) {
  if (__builtin_expect(gWin.load(std::memory_order_relaxed), 0)) {
    const uint32_t n = gRingN.fetch_add(1, std::memory_order_relaxed);
    if (n < kRing) {
      TraceEntry &t = gRing[n];
      t.id = I;
      t.tid = static_cast<int32_t>(rawTid());
      t.a = reinterpret_cast<uintptr_t>(a);
      t.b = reinterpret_cast<uintptr_t>(b);
      t.c = reinterpret_cast<uintptr_t>(c);
      t.d = reinterpret_cast<uintptr_t>(d);
      t.posOk = rawRead(b, t.pos, 12) == 12;
      uintptr_t cv = 0;
      t.cvp = rawRead(c, &cv, 8) == 8 ? cv : 0;
    }
  }
  return gTraceOrig[I](a, b, c, d, e, f, g, h);
}

template <size_t... I>
std::array<void *, sizeof...(I)> makeTraceDetours(std::index_sequence<I...>) {
  return {reinterpret_cast<void *>(&traceDetour<I>)...};
}

void flushTrace(const int *dpos) {
  const long myTid = tid();
  const uint32_t n = std::min<uint32_t>(gRingN.load(), kRing);
  const int w = gTraceWindows.fetch_add(1);
  char buf[300];
  snprintf(buf, sizeof(buf), "TRACE#%d destroy pos=(%d,%d,%d) entries=%u", w,
           dpos[0], dpos[1], dpos[2], n);
  logLine(buf);
  std::set<std::tuple<uint32_t, int, int, int, uintptr_t>> seen;
  int lines = 0;
  for (uint32_t i = 0; i < n; ++i) {
    const TraceEntry &e = gRing[i];
    if (e.tid != myTid)
      continue;
    if (!seen.insert({e.id, e.pos[0], e.pos[1], e.pos[2], e.cvp}).second)
      continue;
    if (++lines > 220)
      break;
    const bool match = e.posOk && e.pos[0] == dpos[0] &&
                       e.pos[1] == dpos[1] && e.pos[2] == dpos[2];
    snprintf(buf, sizeof(buf),
             "T%u %-12s%s pos=(%d,%d,%d)%s cvp=0x%lx d=%p", i,
             kTrace[e.id].name, match ? " *POS*" : "", e.pos[0], e.pos[1],
             e.pos[2], e.posOk ? "" : "?",
             static_cast<unsigned long>(e.cvp ? e.cvp - gBase : 0),
             reinterpret_cast<void *>(e.d));
    logLine(buf);
  }
}

constexpr uintptr_t kSpTickRva = 0xec45ffc; // ServerPlayer vtable slot 25
constexpr const char *kSpTickHex = "ff8303d1fd7b09a9f95300f9f85f0ba9";
Fn gSpTickOrig = nullptr;

void probeServerPlayer(void *player) {
  static std::atomic<int> cnt{0};
  const int n = cnt.fetch_add(1);
  if (n >= 3)
    return;
  uintptr_t dim = 0, bs = 0;
  rdp(reinterpret_cast<uintptr_t>(player) + kActorDimOff, dim);
  if (dim)
    rdp(dim + kDimBsOff, bs);
  void *gb = nullptr;
  bool have;
  int pos[3];
  {
    std::lock_guard<std::mutex> lk(gSelMu);
    have = gHave[0];
    std::memcpy(pos, gPos[0], sizeof(pos));
  }
  if (gGetBlock && bs && have)
    gb = gGetBlock(reinterpret_cast<void *>(bs), pos);
  char buf[300];
  snprintf(buf, sizeof(buf),
           "E0 sptick #%d player=%p tid=%ld dim=%p bs=%p getBlock=%p", n + 1,
           player, tid(), reinterpret_cast<void *>(dim),
           reinterpret_cast<void *>(bs), gb);
  logLine(buf);
}

uintptr_t spTickDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                       void *g, void *h) {
  if (gWorldProbe.load(std::memory_order_relaxed))
    probeServerPlayer(a);
  return gSpTickOrig(a, b, c, d, e, f, g, h);
}

uintptr_t gmTickDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                       void *g, void *h) {
  if (gWorldProbe.load(std::memory_order_relaxed))
    probeGameMode(a, "tick");
  return gGmTickOrig(a, b, c, d, e, f, g, h);
}

uintptr_t gmDestroyDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                          void *g, void *h) {
  const bool probe = gWorldProbe.load(std::memory_order_relaxed);
  bool trace = false;
  int tpos[3] = {0, 0, 0};
  if (probe) {
    probeGameMode(a, "destroy");
    if (gTraceWindows.load() < 6) {
      uintptr_t pl = 0;
      rdp(reinterpret_cast<uintptr_t>(a) + kGmPlayerOff, pl);
      if (pl && pl != gLocalPlayer.load() && safeRead(b, tpos, sizeof(tpos)))
        trace = true; // server-side destroy
    }
  }
  if (trace) {
    gRingN.store(0);
    gWin.store(true);
  }
  const uintptr_t r = gGmDestroyOrig(a, b, c, d, e, f, g, h);
  if (trace) {
    gWin.store(false);
    flushTrace(tpos);
  }
  return r;
}

uintptr_t lpTickDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                       void *g, void *h) {
  gLocalPlayer.store(reinterpret_cast<uintptr_t>(a), std::memory_order_relaxed);
  return gLpTickOrig(a, b, c, d, e, f, g, h);
}

void onToggle(std::string_view id, bool enabled) {
  if (id == kModuleId) {
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(enabled);
    saveState(enabled);
  } else if (id == kProbeId) {
    gWorldProbe.store(enabled);
  } else if (id == kToolsId) {
    gToolsOn.store(enabled);
    if (!enabled)
      gArmed.store(false);
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
    logLine("--- axiom v0.3 start --- path=" + gLogPath);

    gBase = base;
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

    // ---- tap hook
    {
      auto *addr = reinterpret_cast<void *>(base + kBuildRva);
      if (matchHex(addr, kBuildHex)) {
        mBuildHook = pl::memory::HookHandle(
            addr, reinterpret_cast<void *>(&buildDetour),
            reinterpret_cast<void **>(&gBuildOrig),
            pl::memory::HookPriority::Normal);
      }
      mTapOk = mBuildHook.installed();
    }
    logLine("hooks: noupdates=" + std::to_string(ok) +
            " tapHook=" + std::string(mTapOk ? "yes" : "no"));

    // ---- menu
    logLine("step: registering No Updates");
    bool r = pl::modmenu::ModuleBuilder(kModuleId, "No Updates")
                 .modId(mSelf.getId())
                 .description("Freezes block updates: sand, gravel, water, "
                              "lava, redstone, plants, fire, ladders...")
                 .defaultEnabled(savedOn)
                 .onToggle(onToggle)
                 .registerModule();
    logLine("step: No Updates done");

    if (mTapOk) {
      logLine("step: registering Axiom Tools");
      r &= pl::modmenu::ModuleBuilder(kToolsId, "Axiom Tools")
               .modId(mSelf.getId())
               .description("HUD button SEL: tap it, then tap two blocks "
                            "(Pos1, Pos2).")
               .defaultEnabled(true)
               .onToggle(onToggle)
               .registerModule();
      logLine("step: Axiom Tools done");

      findJni();
      logLine(std::string("hud size fn: ") + (gSizeFn ? "found" : "MISSING") +
              ", hud fn: " + (gHudFn ? "found" : "missing"));
      if (!gTouchRegistered.exchange(true)) {
        pl::input::registerTouchCallback(
            [](const pl::input::TouchEvent &ev) { return onTouch(ev); });
        logLine("step: touch callback registered");
      }
      gStop.store(false);
      gToolsOn.store(true);
      if (!gLoopRunning.exchange(true))
        std::thread([] { uiLoop(); }).detach();
      gHudDirty.store(true);
      logLine("step: HUD loop started");
    }

    // ---- world probe hooks + module (read-only test)
    {
      auto install = [&](uintptr_t rva, const char *hex, void *detour, Fn *orig,
                         pl::memory::HookHandle &h) {
        auto *addr = reinterpret_cast<void *>(base + rva);
        if (!matchHex(addr, hex))
          return false;
        h = pl::memory::HookHandle(addr, detour,
                                   reinterpret_cast<void **>(orig),
                                   pl::memory::HookPriority::Normal);
        return h.installed();
      };
      const bool t1 = install(kGmTickRva, kGmTickHex,
                              reinterpret_cast<void *>(&gmTickDetour),
                              &gGmTickOrig, mGmTickHook);
      const bool t2 = install(kGmDestroyRva, kGmDestroyHex,
                              reinterpret_cast<void *>(&gmDestroyDetour),
                              &gGmDestroyOrig, mGmDestroyHook);
      const bool t3 = install(kLpTickRva, kLpTickHex,
                              reinterpret_cast<void *>(&lpTickDetour),
                              &gLpTickOrig, mLpTickHook);
      const bool t4 = install(kSpTickRva, kSpTickHex,
                              reinterpret_cast<void *>(&spTickDetour),
                              &gSpTickOrig, mSpTickHook);
      gPid = getpid();
      static const auto trD =
          makeTraceDetours(std::make_index_sequence<kTraceCount>{});
      int trOk = 0;
      for (size_t i = 0; i < kTraceCount; ++i) {
        auto *taddr = reinterpret_cast<void *>(base + kTrace[i].rva);
        if (!matchHex(taddr, kTrace[i].hex))
          continue;
        mTraceHooks[i] = pl::memory::HookHandle(
            taddr, trD[i], reinterpret_cast<void **>(&gTraceOrig[i]),
            pl::memory::HookPriority::Normal);
        if (mTraceHooks[i].installed())
          ++trOk;
      }
      logLine("trace hooks: " + std::to_string(trOk) + "/" +
              std::to_string(kTraceCount) + " spTick=" + std::to_string(t4));
      auto *gbAddr = reinterpret_cast<void *>(base + kGetBlockRva);
      const bool gbOk = matchPattern(gbAddr, kGetBlockPat,
                                     sizeof(kGetBlockPat) / sizeof(int));
      gGetBlock = gbOk ? reinterpret_cast<GetBlockFn>(gbAddr) : nullptr;
      logLine("world probe hooks: gmTick=" + std::to_string(t1) +
              " gmDestroy=" + std::to_string(t2) +
              " lpTick=" + std::to_string(t3) +
              " getBlockSig=" + std::to_string(gbOk));
      if (t1) {
        logLine("step: registering World Probe");
        r &= pl::modmenu::ModuleBuilder(kProbeId, "World Probe (test)")
                 .modId(mSelf.getId())
                 .description("Test (read-only): turn on, tap a block with "
                              "SEL (Pos1), walk and break one block, turn off.")
                 .defaultEnabled(false)
                 .onToggle(onToggle)
                 .registerModule();
        logLine("step: World Probe done");
      }
    }
    return r;
  }

  bool disable() {
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(false);
    gArmed.store(false);
    gToolsOn.store(false);
    gStop.store(true);
    gWorldProbe.store(false);
    pl::modmenu::unregisterModule(kProbeId);
    pl::modmenu::unregisterModule(kToolsId);
    pl::modmenu::unregisterModule(kModuleId);
    mBuildHook.reset();
    mGmTickHook.reset();
    mGmDestroyHook.reset();
    mLpTickHook.reset();
    mSpTickHook.reset();
    for (auto &h : mTraceHooks)
      h.reset();
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
  pl::memory::HookHandle mGmTickHook;
  pl::memory::HookHandle mGmDestroyHook;
  pl::memory::HookHandle mLpTickHook;
  pl::memory::HookHandle mSpTickHook;
  std::array<pl::memory::HookHandle, kTraceCount> mTraceHooks;
};

PL_REGISTER_MOD(NoUpdatesMod, NoUpdatesMod::instance())
