cd /mnt/user-data/outputs && cp NoUpdates.cpp /tmp/NoUpdates_v10.cpp && python3 - <<'EOF'
s=open('NoUpdates.cpp').read()

# ---- header comment
s=s.replace('''//  * DEL        : HUD button; removes every block of the selection through
//                 the server-side GameMode::destroyBlock (works out of reach).
//                 The job runs when you break any block (server thread).''','''//  * DEL        : HUD button; removes every block of the selection through
//                 the server-side GameMode::destroyBlock (works out of reach).
//  * FILL       : HUD button; fills the AIR cells of the selection with the
//                 block in your hand through the server-side GameMode
//                 ::useItemOn. Both jobs run on the server thread when you
//                 break any block (the trigger).''',1)

# ---- new constants after getBlock pattern
anchor='using Fn = uintptr_t (*)(void *, void *, void *, void *, void *, void *,'
consts=r'''// Held item / placement helpers (addresses verified by byte patterns)
constexpr uintptr_t kHandItemRva = 0xf57a04c;
constexpr int kHandItemPat[] = {0x28, 0x1C, 0x00, 0x12, 0x1F, 0x05, 0x00, 0x71,
                                -1,   -1,   -1,   -1,   0x08, 0x00, 0x40, 0xF9,
                                0x01, 0x39, 0x41, 0xF9, 0x20, 0x00, 0x1F, 0xD6};
constexpr uintptr_t kItemBlockTypeRva = 0xfc74c70;
constexpr int kItemBlockTypePat[] = {
    0xFD, 0x7B, 0xBE, 0xA9, 0xF4, 0x4F, 0x01, 0xA9, 0xFD, 0x03, 0x00, 0x91,
    0xF4, 0x03, 0x00, 0xAA, -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    0xE0, 0x03, 0x14, 0xAA, -1,   -1,   -1,   -1,   0xF3, 0x03, 0x00, 0xAA};
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

'''
s=s.replace(anchor,consts+anchor,1)

# ---- job section
a=s.index('// -------------------------------------------------------------- DEL job')
b=s.index('// ------------------------------------------------------------------ HUD')
job=r'''// ------------------------------------------------------------- DEL / FILL job
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
  gJobMode = mode;
  gDelMsg = (mode == kDel)
                ? "DEL ready: break any block to run"
                : "FILL ready: hold a block, then break any block to run";
  gDelActive.store(true);
  gHudDirty.store(true);
  char buf[160];
  snprintf(buf, sizeof(buf), "%s start box (%d,%d,%d)-(%d,%d,%d) cells=%lld",
           mode == kDel ? "DEL" : "FILL", gDelMin[0], gDelMin[1], gDelMin[2],
           gDelMax[0], gDelMax[1], gDelMax[2], vol);
  logLine(buf);
}

'''
s=s[:a]+job+s[b:]

# ---- HUD: geometry + buttons
s=s.replace('''  float dy = 0;                 // DEL button (same x and size)''','''  float dy = 0;                 // DEL button (same x and size)
  float fy = 0;                 // FILL button''',1)
s=s.replace('''  const float dy = by + s * 1.15f;
  {
    std::lock_guard<std::mutex> lk(gGeoMu);
    gGeo.bx = bx;
    gGeo.by = by;
    gGeo.bs = s;
    gGeo.dy = dy;
  }''','''  const float dy = by + s * 1.15f;
  const float fy = by + s * 2.3f;
  {
    std::lock_guard<std::mutex> lk(gGeoMu);
    gGeo.bx = bx;
    gGeo.by = by;
    gGeo.bs = s;
    gGeo.dy = dy;
    gGeo.fy = fy;
  }''',1)
s=s.replace('''  bool delOn;
  long long dTotal, dDone, dRemoved;
  std::string dMsg;
  {
    std::lock_guard<std::mutex> lk(gDelMu);
    delOn = gDelActive.load();
    dTotal = gDelTotal;
    dDone = gDelDone;
    dRemoved = gDelRemoved;
    dMsg = gDelMsg;
  }''','''  bool delOn;
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
  }''',1)
s=s.replace('''  cmds.push_back(makeRect(bx, dy, s, s, delOn ? 0xE6D32F2F : 0xB0303030,
                          s * 0.2f));
  cmds.push_back(makeText(delOn ? "STOP" : "DEL", bx + s * 0.5f,
                          dy + s * 0.5f + s * 0.09f, s * 0.26f, 0xFFFFFFFF,
                          -2.0f));''','''  const bool delRun = delOn && mode == kDel;
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
                          -2.0f));''',1)
s=s.replace('''  if (delOn) {
    snprintf(buf, sizeof(buf), "DEL %lld/%lld  removed %lld", dDone, dTotal,
             dRemoved);
    line(buf);
  }''','''  if (delOn) {
    snprintf(buf, sizeof(buf), "%s %lld/%lld  %s %lld",
             mode == kDel ? "DEL" : "FILL", dDone, dTotal,
             mode == kDel ? "removed" : "placed", dRemoved);
    line(buf);
    if (mode == kFill && dFailed > 0) {
      snprintf(buf, sizeof(buf), "skipped (no support/refused): %lld", dFailed);
      line(buf);
    }
  }''',1)

# ---- touch: 3 buttons
a=s.index('bool onTouch(const pl::input::TouchEvent &ev) {')
b=s.index('// ------------------------------------------- game hooks')
touch=r'''bool onTouch(const pl::input::TouchEvent &ev) {
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

'''
s=s[:a]+touch+s[b:]

# ---- slice runner + destroy detour
a=s.index('// Runs on the SERVER thread (inside the server GameMode::destroyBlock hook).')
b=s.index('void onToggle(std::string_view id, bool enabled) {')
run=r'''// Runs on the SERVER thread (inside the server GameMode::destroyBlock hook).
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

'''
s=s[:a]+run+s[b:]

# ---- enable(): resolve helpers
s=s.replace('''    logLine("hooks: noupdates=" + std::to_string(ok) +''','''    auto *hiAddr = reinterpret_cast<void *>(base + kHandItemRva);
    auto *btAddr = reinterpret_cast<void *>(base + kItemBlockTypeRva);
    auto *uoAddr = reinterpret_cast<void *>(base + kUseItemOnRva);
    const bool hiOk =
        matchPattern(hiAddr, kHandItemPat, sizeof(kHandItemPat) / sizeof(int));
    const bool btOk = matchPattern(btAddr, kItemBlockTypePat,
                                   sizeof(kItemBlockTypePat) / sizeof(int));
    const bool uoOk = matchHex(uoAddr, kUseItemOnHex);
    gHandItem = hiOk ? reinterpret_cast<HandItemFn>(hiAddr) : nullptr;
    gItemBlockType = btOk ? reinterpret_cast<ItemBlockTypeFn>(btAddr) : nullptr;
    gFillOk = hiOk && btOk && uoOk && gbOk;
    logLine("fill helpers: handItem=" + std::to_string(hiOk) +
            " itemBlockType=" + std::to_string(btOk) +
            " useItemOn=" + std::to_string(uoOk));
    logLine("hooks: noupdates=" + std::to_string(ok) +''',1)
s=s.replace('''                            "DEL: removes the selection (needs a creative "
                            "world; break a block to run it).")''','''                            "DEL: removes the selection. FILL: fills the empty "
                            "cells with the block in your hand. Break any "
                            "block to run a job (creative world).")''',1)
s=s.replace('axiom v1.0 start','axiom v1.1 start',1)
open('NoUpdates.cpp','w').write(s)
EOF
g++ -std=c++20 -fsyntax-only -Wall -Wextra -I/tmp/stub NoUpdates.cpp 2>&1 | head -30; echo checked; grep -n "runDelSlice\|startOrCancelDel" NoUpdates.cpp | head