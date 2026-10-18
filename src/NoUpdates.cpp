// Axiom for Levi v0.1 - Minecraft Bedrock 26.52.3 (arm64)
//  * No Updates: freezes block events (physics, redstone, ticks)
//  * Select: one button arms selection; next two block taps set Pos1 and Pos2
//  * Test modules: Input Log and Command Log (to locate game functions)
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <sys/uio.h>
#include <unistd.h>

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
constexpr const char *kButtonId = "axiom.select";
constexpr const char *kLogId = "no_updates.inputlog";
constexpr const char *kCmdLogId = "axiom.cmdlog";

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

std::string gLogPath;
void logLine(const std::string &line) {
  std::ofstream f(gLogPath, std::ios::app);
  if (f.good())
    f << line << "\n";
}

// libc++ (NDK) std::string, read from another address
bool readStdString(const void *p, std::string &out) {
  unsigned char raw[24];
  if (!safeRead(p, raw, sizeof(raw)))
    return false;
  if ((raw[0] & 1) == 0) {
    size_t len = raw[0] >> 1;
    if (len == 0 || len > 22)
      return false;
    out.assign(reinterpret_cast<char *>(raw) + 1, len);
  } else {
    uint64_t size;
    const char *data;
    std::memcpy(&size, raw + 8, 8);
    std::memcpy(&data, raw + 16, 8);
    if (size == 0 || size > 300)
      return false;
    out.resize(size);
    if (!safeRead(data, out.data(), size))
      return false;
  }
  for (char ch : out)
    if (static_cast<unsigned char>(ch) < 32 ||
        static_cast<unsigned char>(ch) > 126)
      return false;
  return true;
}

// ------------------------------------------------------- selection (Axiom)
std::atomic<bool> gArmed{false};
std::mutex gSelMu;
int gPos[2][3];
bool gHave[2];
int gNext = 0;
bool gToolsOn = true;

void updateHud() {
  bool a = gArmed.load(), h0, h1;
  {
    std::lock_guard<std::mutex> lk(gSelMu);
    h0 = gHave[0];
    h1 = gHave[1];
  }
  auto img = [](bool on) { return on ? "axiom.red" : "axiom.gray"; };
  const std::vector<pl::modmenu::DrawCommand> commands = {
      {.type = pl::modmenu::DrawCommandType::Image, .x = 24.0f, .y = 140.0f,
       .w = 28.0f, .h = 28.0f, .imageId = img(a)},
      {.type = pl::modmenu::DrawCommandType::Image, .x = 64.0f, .y = 140.0f,
       .w = 28.0f, .h = 28.0f, .imageId = img(h0)},
      {.type = pl::modmenu::DrawCommandType::Image, .x = 104.0f, .y = 140.0f,
       .w = 28.0f, .h = 28.0f, .imageId = img(h1)},
  };
  pl::modmenu::submitDrawCommands(kToolsId, commands);
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
    gArmed.store(false); // both points set: back to normal taps
  char buf[96];
  snprintf(buf, sizeof(buf), "SELECT pos%d = (%d,%d,%d)", idx + 1, pos[0],
           pos[1], pos[2]);
  logLine(buf);
  updateHud();
}

void onButton(std::string_view, pl::modmenu::ButtonEvent, float) {
  static std::atomic<long long> last{0};
  long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
  if (now - last.load() < 300)
    return;
  last.store(now);
  bool arm = !gArmed.load();
  if (arm) {
    std::lock_guard<std::mutex> lk(gSelMu);
    gHave[0] = gHave[1] = false;
    gNext = 0;
  }
  gArmed.store(arm);
  updateHud();
}

// ------------------------------------- GameMode virtuals (tap/break/place)
struct Lab {
  uintptr_t rva;
  const char *hex; // first 16 bytes expected at rva
  const char *name;
};
constexpr Lab kLab[] = {
    {0xf89fe18, "ff8301d1fd7b01a9f91300f9f85f03a9", "slot2 startDestroy?"},
    {0xf8a03ac, "ff0302d1fd7b04a9f85f05a9f65706a9", "slot3 destroy?"},
    {0xf8a073c, "ff8303d1e923076dfd7b08a9fc6f09a9", "slot4 continueDestroy?"},
    {0xf8a112c, "fd7bbea9f30b00f9fd030091f30300aa", "slot5 stopDestroy?"},
    {0xf8a1168, "080040f91f4000b9e4031f2a051d40f9", "slot6 startBuild?"},
    {0xf8a117c, "ffc304d1fd7b0da9fc7300f9fa670fa9", "slot7 build?"},
    {0xf8a16ec, "ff0305d1e9230d6dfd7b0ea9fc6f0fa9", "slot8 continueBuild?"},
};
constexpr size_t kLabCount = sizeof(kLab) / sizeof(kLab[0]);
constexpr size_t kBuildIdx = 5; // slot7: block tap / place

std::atomic<bool> gLog{false};
std::atomic<int> gLogCount{0};
std::atomic<uint64_t> gLastKey[kLabCount];
Fn gLabOrig[kLabCount]{};

template <size_t I>
uintptr_t labDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                    void *g, void *h) {
  if constexpr (I == kBuildIdx) {
    if (gArmed.load(std::memory_order_relaxed)) {
      handleTap(b);
      return 0; // swallow the tap: no block placed
    }
  }
  if (gLog.load(std::memory_order_relaxed) && gLogCount.load() < 400) {
    int pos[3] = {0, 0, 0};
    const bool ok = safeRead(b, pos, sizeof(pos));
    const unsigned face =
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(c) & 0xff);
    uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(pos[0])) * 73856093ull) ^
        (static_cast<uint64_t>(static_cast<uint32_t>(pos[1])) * 19349663ull) ^
        (static_cast<uint64_t>(static_cast<uint32_t>(pos[2])) * 83492791ull) ^
        face ^ (ok ? 0 : 0x5555);
    if (gLastKey[I].exchange(key) != key) {
      char buf[256];
      snprintf(buf, sizeof(buf), "[%d] %s pos=(%d,%d,%d)%s face=%u c=%p d=%p",
               gLogCount.fetch_add(1), kLab[I].name, pos[0], pos[1], pos[2],
               ok ? "" : "?", face, c, d);
      logLine(buf);
    }
  }
  return gLabOrig[I](a, b, c, d, e, f, g, h);
}

template <size_t... I>
std::array<void *, sizeof...(I)> makeLabDetours(std::index_sequence<I...>) {
  return {reinterpret_cast<void *>(&labDetour<I>)...};
}

// --------------- ClientInstanceScreenModel virtuals (chat / commands test)
struct SmSlot {
  uintptr_t rva;
  const char *hex;
  int slot;
};
constexpr SmSlot kSm[] = {
    {0xa148dd8, "fd7bbea9f30b00f9fd030091082440f9", 2},
    {0xa1399f0, "ff0302d1fd7b05a9f65706a9f44f07a9", 5},
    {0xa138824, "fd7bbba9fc0b00f9f85f02a9f65703a9", 6},
    {0xa138d14, "ffc301d1fd7b03a9f85f04a9f65705a9", 7},
    {0xa133734, "ff4302d1fd7b04a9f92b00f9f85f06a9", 8},
    {0xa15d704, "fd7bbea9f30b00f9fd030091082440f9", 12},
    {0xa146768, "ff0303d1fd7b09a9f6570aa9f44f0ba9", 13},
    {0xa133b5c, "fd7bbea9f44f01a9fd030091083040f9", 15},
    {0xa154f2c, "fd7bbea9f30b00f9fd030091082440f9", 17},
    {0xa141d70, "ffc301d1fd7b03a9f72300f9f65705a9", 18},
    {0xa15eab4, "ff4301d1fd7b02a9f51b00f9f44f04a9", 24},
    {0xa16f6ac, "fd7bbea9f30b00f9fd030091082440f9", 26},
    {0xa1708ec, "ffc301d1fd7b04a9f52b00f9f44f06a9", 27},
};
constexpr size_t kSmCount = sizeof(kSm) / sizeof(kSm[0]);

std::atomic<bool> gCmdLog{false};
std::atomic<int> gCmdCount{0};
std::atomic<uint64_t> gSmLast[kSmCount];
std::atomic<void *> gSmThis{nullptr};
Fn gSmOrig[kSmCount]{};

template <size_t I>
uintptr_t smDetour(void *a, void *b, void *c, void *d, void *e, void *f,
                   void *g, void *h) {
  if (gCmdLog.load(std::memory_order_relaxed) && gCmdCount.load() < 300) {
    std::string s;
    if (readStdString(b, s)) {
      uint64_t key = std::hash<std::string>{}(s);
      if (gSmLast[I].exchange(key) != key) {
        gSmThis.store(a);
        char buf[400];
        snprintf(buf, sizeof(buf), "[%d] SM slot%d this=%p str='%.200s'",
                 gCmdCount.fetch_add(1), kSm[I].slot, a, s.c_str());
        logLine(buf);
      }
    }
  }
  return gSmOrig[I](a, b, c, d, e, f, g, h);
}

template <size_t... I>
std::array<void *, sizeof...(I)> makeSmDetours(std::index_sequence<I...>) {
  return {reinterpret_cast<void *>(&smDetour<I>)...};
}

// ------------------------------------------------------------------ misc
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

void onToggle(std::string_view id, bool enabled) {
  if (id == kModuleId) {
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(enabled);
    saveState(enabled);
  } else if (id == kLogId) {
    gLog.store(enabled);
  } else if (id == kCmdLogId) {
    gCmdLog.store(enabled);
  } else if (id == kToolsId) {
    gToolsOn = enabled;
    if (!enabled)
      gArmed.store(false);
    updateHud();
  }
}

std::vector<unsigned char> solid(unsigned char r, unsigned char g,
                                 unsigned char b, unsigned char a) {
  std::vector<unsigned char> px(8 * 8 * 4);
  for (size_t i = 0; i < 64; ++i) {
    px[i * 4] = r;
    px[i * 4 + 1] = g;
    px[i * 4 + 2] = b;
    px[i * 4 + 3] = a;
  }
  return px;
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

    // ---- log file location (new name: avoids old file owned by another app)
    {
      const char *cands[] = {"/sdcard/Download/axiom_log2.txt",
                             "/sdcard/axiom_log2.txt"};
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
        gLogPath = d.empty() ? "/dev/null" : d + "axiom_log2.txt";
      }
    }
    logLine("--- axiom log start (v0.2) --- path=" + gLogPath);

    // ---- GameMode hooks
    static const auto labDetours =
        makeLabDetours(std::make_index_sequence<kLabCount>{});
    int labOk = 0;
    for (size_t i = 0; i < kLabCount; ++i) {
      auto *addr = reinterpret_cast<void *>(base + kLab[i].rva);
      if (!matchHex(addr, kLab[i].hex))
        continue;
      mLabHooks[i] = pl::memory::HookHandle(
          addr, labDetours[i], reinterpret_cast<void **>(&gLabOrig[i]),
          pl::memory::HookPriority::Normal);
      if (mLabHooks[i].installed())
        ++labOk;
    }
    log.info("{}/{} input hooks installed", labOk, static_cast<int>(kLabCount));
    mTapOk = mLabHooks[kBuildIdx].installed();

    // ---- ScreenModel hooks (command test)
    static const auto smDetours =
        makeSmDetours(std::make_index_sequence<kSmCount>{});
    int smOk = 0;
    for (size_t i = 0; i < kSmCount; ++i) {
      auto *addr = reinterpret_cast<void *>(base + kSm[i].rva);
      if (!matchHex(addr, kSm[i].hex))
        continue;
      mSmHooks[i] = pl::memory::HookHandle(
          addr, smDetours[i], reinterpret_cast<void **>(&gSmOrig[i]),
          pl::memory::HookPriority::Normal);
      if (mSmHooks[i].installed())
        ++smOk;
    }
    log.info("{}/{} screen model hooks installed", smOk,
             static_cast<int>(kSmCount));

    logLine("hooks: noupdates=" + std::to_string(ok) + " gamemode=" +
            std::to_string(labOk) + " screenmodel=" + std::to_string(smOk) +
            " tapHook=" + std::string(mTapOk ? "yes" : "no"));

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
               .description("Select button: tap it, then tap two blocks "
                            "(Pos1, Pos2).")
               .defaultEnabled(true)
               .onToggle(onToggle)
               .registerModule();
      logLine("step: Axiom Tools done");
    }
    if (labOk > 0) {
      logLine("step: registering Input Log");
      r &= pl::modmenu::ModuleBuilder(kLogId, "Input Log (test)")
               .modId(mSelf.getId())
               .description("Test: block taps to the log file")
               .defaultEnabled(false)
               .onToggle(onToggle)
               .registerModule();
      logLine("step: Input Log done");
    }
    if (smOk > 0) {
      logLine("step: registering Command Log");
      r &= pl::modmenu::ModuleBuilder(kCmdLogId, "Command Log (test)")
               .modId(mSelf.getId())
               .description("Test: type in chat while on; strings go to the "
                            "log file")
               .defaultEnabled(false)
               .onToggle(onToggle)
               .registerModule();
      logLine("step: Command Log done");
    }
    if (mTapOk) {
      // button last: if the launcher rejects it, everything else exists
      logLine("step: registering images");
      pl::modmenu::registerImage("axiom.red", solid(230, 40, 40, 230), 8, 8);
      pl::modmenu::registerImage("axiom.gray", solid(90, 90, 90, 140), 8, 8);
      logLine("step: registe