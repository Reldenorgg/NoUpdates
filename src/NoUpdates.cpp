// No Updates - freezes block physics (gravity blocks, liquids) on
// Minecraft Bedrock 26.52.3 (arm64) by neutralising block event handlers.
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>

namespace {

struct Target {
  const char *name;
  uintptr_t rva;
};

constexpr Target kTargets[] = {
    {"FallingBlock::QueuedTick", 0x100c8b38},
    {"FallingBlock::Place", 0x100c8a88},
    {"ConcretePowder::QueuedTick", 0x10107638},
    {"ConcretePowder::Place", 0x10107588},
    {"LiquidBlock::QueuedTick", 0x100a30cc},
    {"LiquidBlock::RandomTick", 0x100a317c},
    {"LiquidBlockBase::Place", 0x100a3344},
    {"LiquidBlockDynamic::Place", 0x100a3480},
    {"LiquidBlockDynamic::QueuedTick", 0x100a3530},
};
constexpr size_t kCount = sizeof(kTargets) / sizeof(kTargets[0]);

constexpr unsigned char kExpected[16] = {0x02, 0x20, 0x41, 0xa9, 0x09, 0x04,
                                         0x40, 0xf9, 0x20, 0x05, 0x88, 0x8b,
                                         0x68, 0x00, 0x00, 0x36};

constexpr const char *ModuleId = "no_updates.main";

using Fn = uintptr_t (*)(void *, void *, void *, void *, void *, void *,
                         void *, void *);

std::atomic<bool> gFrozen{false};
Fn gOrig[kCount]{};

template <size_t I>
uintptr_t detour(void *a, void *b, void *c, void *d, void *e, void *f,
                 void *g, void *h) {
  if (gFrozen.load(std::memory_order_relaxed))
    return 0;
  return gOrig[I](a, b, c, d, e, f, g, h);
}

template <size_t... I>
std::array<void *, sizeof...(I)> makeDetours(std::index_sequence<I...>) {
  return {reinterpret_cast<void *>(&detour<I>)...};
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

void onToggle(std::string_view, bool enabled) { gFrozen.store(enabled); }

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
    static const auto detours = makeDetours(std::make_index_sequence<kCount>{});
    int ok = 0;
    for (size_t i = 0; i < kCount; ++i) {
      auto *addr = reinterpret_cast<void *>(base + kTargets[i].rva);
      if (std::memcmp(addr, kExpected, sizeof(kExpected)) != 0) {
        log.info("{}: bytes mismatch (other game version?) - skipped",
                 kTargets[i].name);
        continue;
      }
      mHooks[i] = pl::memory::HookHandle(
          addr, detours[i], reinterpret_cast<void **>(&gOrig[i]),
          pl::memory::HookPriority::Normal);
      if (mHooks[i].installed())
        ++ok;
      else
        log.info("{}: hook failed", kTargets[i].name);
    }
    log.info("{}/{} hooks installed", ok, static_cast<int>(kCount));
    if (ok == 0)
      return false;

    return pl::modmenu::ModuleBuilder(ModuleId, "No Updates")
        .modId(mSelf.getId())
        .description("Freezes block physics: sand, gravel, concrete powder, "
                     "water and lava stop falling/flowing.")
        .defaultEnabled(false)
        .onToggle(onToggle)
        .registerModule();
  }

  bool disable() {
    gFrozen.store(false);
    pl::modmenu::unregisterModule(ModuleId);
    for (auto &h : mHooks)
      h.reset();
    return true;
  }

  bool unload() { return true; }

private:
  ll::mod::NativeMod &mSelf;
  std::array<pl::memory::HookHandle, kCount> mHooks;
};

PL_REGISTER_MOD(NoUpdatesMod, NoUpdatesMod::instance())
