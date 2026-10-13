// No Updates v2 - freezes block behaviour on Minecraft Bedrock 26.52.3 (arm64)
// by neutralising block event handlers. One switch controls three groups:
//   0 Physics   : sand/gravel/concrete powder, water, lava
//   1 Redstone  : redstone wire/torch/repeater/comparator/doors/pistons...
//   2 Ticks     : scheduled + random block ticks (plants, fire, ladders...)
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
#include <string_view>
#include <utility>

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

void onToggle(std::string_view, bool enabled) {
  for (int i = 0; i < 3; ++i)
    gFrozen[i].store(enabled);
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

    return pl::modmenu::ModuleBuilder(kModuleId, "No Updates")
        .modId(mSelf.getId())
        .description("Freezes block updates: sand, gravel, water, lava, "
                     "redstone, plants, fire, ladders and more stay frozen.")
        .defaultEnabled(false)
        .onToggle(onToggle)
        .registerModule();
  }

  bool disable() {
    for (int i = 0; i < 3; ++i)
      gFrozen[i].store(false);
    pl::modmenu::unregisterModule(kModuleId);
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
