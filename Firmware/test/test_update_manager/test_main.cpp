#include <unity.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "FirmwareMarker.h"
#include "UpdateManager.h"

namespace {

const char kMagic[16] = {'V', 'E', 'N', 'T', 'I', 'Q', '-', 'F', 'W', '-', 'M', 'E', 'T', 'A', '0', '1'};

FirmwareMarker makeMarker(const char *hw, uint8_t ma, uint8_t mi, uint8_t pa) {
  FirmwareMarker m;
  memset(&m, 0, sizeof(m));
  memcpy(m.magic, kMagic, sizeof(m.magic));
  strncpy(m.hw, hw, sizeof(m.hw) - 1);
  m.major = ma;
  m.minor = mi;
  m.patch = pa;
  return m;
}

// Fake-Plattform: deterministischer "Hash" (kein echtes SHA-256, der echte Hash laeuft nur auf dem Geraet).
class FakePlatform : public IUpdatePlatform {
 public:
  uint32_t now = 1000;
  uint32_t space = 0x100000;
  bool beginOk = true;
  size_t writeLimit = SIZE_MAX;  // maximal akzeptierte Bytes gesamt
  bool running = false;
  bool committed = false;
  bool aborted = false;
  int beginCalls = 0;
  int restarts = 0;
  std::vector<uint8_t> flash;

  uint8_t st[32];
  uint32_t idx = 0;

  uint32_t millis() override { return now; }
  void restart() override { restarts++; }
  uint32_t freeSpace() override { return space; }
  bool flashBegin(uint32_t) override {
    beginCalls++;
    if (!beginOk) return false;
    running = true;
    committed = aborted = false;
    flash.clear();
    return true;
  }
  size_t flashWrite(const uint8_t *d, size_t n) override {
    if (flash.size() + n > writeLimit) return 0;
    flash.insert(flash.end(), d, d + n);
    return n;
  }
  bool flashEnd(bool commit) override {
    running = false;
    if (commit) committed = true; else aborted = true;
    return true;
  }
  bool flashRunning() override { return running; }

  void hashBegin() override { memset(st, 0, 32); idx = 0; }
  void hashUpdate(const uint8_t *d, size_t n) override {
    for (size_t i = 0; i < n; i++, idx++) st[idx % 32] = (uint8_t)(st[idx % 32] * 31 + d[i] + idx);
  }
  void hashFinal(uint8_t out[32]) override { memcpy(out, st, 32); }
};

class FakeGuard : public ICoolingGuard {
 public:
  bool allow = true;
  int requests = 0;
  int restores = 0;
  bool requestUpdateSafeState() override { requests++; return allow; }
  void restoreNormal() override { restores++; }
};

std::string fakeHashHex(const std::vector<uint8_t> &img) {
  FakePlatform p;
  p.hashBegin();
  p.hashUpdate(img.data(), img.size());
  uint8_t out[32];
  p.hashFinal(out);
  std::string s;
  char b[3];
  for (int i = 0; i < 32; i++) { snprintf(b, sizeof(b), "%02x", out[i]); s += b; }
  return s;
}

// Image mit Fuellbytes (ohne zufaellige Magic-Treffer) und Marker an Offset.
std::vector<uint8_t> makeImage(const FirmwareMarker &m, size_t total = 600, size_t at = 100) {
  std::vector<uint8_t> img(total, 0xA5);
  img[0] = 0xE9;
  memcpy(&img[at], &m, sizeof(m));
  return img;
}

const FirmwareMarker kSelf = makeMarker("VENTIQ_CORE_REVA", 1, 2, 3);

FakePlatform plat;
FakeGuard guard;
UpdateManager mgr;

bool feed(const std::vector<uint8_t> &img, size_t chunk) {
  for (size_t i = 0; i < img.size(); i += chunk) {
    size_t n = (img.size() - i < chunk) ? img.size() - i : chunk;
    if (!mgr.write(img.data() + i, n)) return false;
  }
  return true;
}

// Kompletter Ablauf; gibt finish()-Ergebnis zurueck.
bool runUpdate(const std::vector<uint8_t> &img, const std::string &hash, size_t chunk = 64) {
  if (!mgr.start(hash.c_str())) return false;
  if (!feed(img, chunk)) return false;
  return mgr.finish();
}

}  // namespace

void setUp() {
  plat = FakePlatform();
  guard = FakeGuard();
  mgr = UpdateManager();
  mgr.begin(&plat, &guard, kSelf);
}
void tearDown() {}

static void expectFailed(UpdateError e) {
  TEST_ASSERT_EQUAL(UpdateState::Failed, mgr.status().state);
  TEST_ASSERT_EQUAL_STRING(updateErrorName(e), updateErrorName(mgr.status().error));
  TEST_ASSERT_FALSE(plat.committed);
  TEST_ASSERT_FALSE(plat.running);
}

static void test_valid_update_commits_and_reboots_after_delay() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 1, 3, 0));
  TEST_ASSERT_TRUE(runUpdate(img, fakeHashHex(img)));
  TEST_ASSERT_EQUAL(UpdateState::RebootPending, mgr.status().state);
  TEST_ASSERT_TRUE(plat.committed);
  TEST_ASSERT_EQUAL_UINT32(img.size(), mgr.status().received);
  TEST_ASSERT_TRUE(plat.flash == img);
  TEST_ASSERT_EQUAL(1, guard.requests);

  mgr.tick();
  TEST_ASSERT_EQUAL(0, plat.restarts);
  plat.now += UpdateManager::kRebootDelayMs;
  mgr.tick();
  TEST_ASSERT_EQUAL(1, plat.restarts);
}

static void test_marker_split_across_chunks() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 1, 2, 4));
  for (size_t chunk : {1u, 3u, 7u, 39u, 40u, 41u, 100u, 1000u}) {
    setUp();
    TEST_ASSERT_TRUE_MESSAGE(runUpdate(img, fakeHashHex(img), chunk), "marker must be found");
  }
}

static void test_marker_at_start_and_end_of_image() {
  auto m = makeMarker("VENTIQ_CORE_REVA", 2, 0, 0);
  auto a = makeImage(m, 200, 0);
  TEST_ASSERT_TRUE(runUpdate(a, fakeHashHex(a), 16));
  setUp();
  auto b = makeImage(m, 200, 200 - sizeof(FirmwareMarker));
  TEST_ASSERT_TRUE(runUpdate(b, fakeHashHex(b), 16));
}

static void test_same_version_allowed() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 1, 2, 3));
  TEST_ASSERT_TRUE(runUpdate(img, fakeHashHex(img)));
}

static void test_uppercase_hash_accepted() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 1, 2, 4));
  std::string h = fakeHashHex(img);
  for (auto &c : h) c = (char)toupper(c);
  TEST_ASSERT_TRUE(runUpdate(img, h));
}

static void test_downgrade_rejected() {
  for (auto v : {makeMarker("VENTIQ_CORE_REVA", 1, 2, 2), makeMarker("VENTIQ_CORE_REVA", 1, 1, 9),
                 makeMarker("VENTIQ_CORE_REVA", 0, 9, 9)}) {
    setUp();
    auto img = makeImage(v);
    TEST_ASSERT_FALSE(runUpdate(img, fakeHashHex(img)));
    expectFailed(UpdateError::Downgrade);
    TEST_ASSERT_TRUE(plat.aborted);
    TEST_ASSERT_EQUAL(1, guard.restores);
  }
}

static void test_wrong_hardware_rejected() {
  auto img = makeImage(makeMarker("OTHER_BOARD", 9, 9, 9));
  TEST_ASSERT_FALSE(runUpdate(img, fakeHashHex(img)));
  expectFailed(UpdateError::WrongHardware);
}

static void test_hardware_prefix_not_accepted() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REV", 9, 9, 9));
  TEST_ASSERT_FALSE(runUpdate(img, fakeHashHex(img)));
  expectFailed(UpdateError::WrongHardware);
}

static void test_foreign_image_without_marker_rejected() {
  std::vector<uint8_t> img(500, 0x11);
  img[0] = 0xE9;
  TEST_ASSERT_FALSE(runUpdate(img, fakeHashHex(img)));
  expectFailed(UpdateError::NoMarker);
}

static void test_two_markers_rejected() {
  auto m = makeMarker("VENTIQ_CORE_REVA", 5, 0, 0);
  auto img = makeImage(m, 600, 50);
  memcpy(&img[300], &m, sizeof(m));
  TEST_ASSERT_FALSE(runUpdate(img, fakeHashHex(img)));
  expectFailed(UpdateError::NoMarker);
}

static void test_hash_mismatch_rejected() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  auto other = img;
  other[10] ^= 0xFF;
  TEST_ASSERT_FALSE(runUpdate(img, fakeHashHex(other)));
  expectFailed(UpdateError::HashMismatch);
  TEST_ASSERT_TRUE(plat.aborted);
  TEST_ASSERT_EQUAL(0, plat.restarts);
}

static void test_bad_hash_parameters() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  std::string good = fakeHashHex(img);
  std::string tooShort = good.substr(0, 63);
  std::string tooLong = good + "0";
  std::string nonHex = good;
  nonHex[5] = 'g';
  for (const char *h : {"", tooShort.c_str(), tooLong.c_str(), nonHex.c_str()}) {
    setUp();
    TEST_ASSERT_FALSE(mgr.start(h));
    expectFailed(UpdateError::BadHashParam);
    TEST_ASSERT_EQUAL(0, plat.beginCalls);
    TEST_ASSERT_EQUAL(0, guard.requests);
  }
  setUp();
  TEST_ASSERT_FALSE(mgr.start(nullptr));
  expectFailed(UpdateError::BadHashParam);
}

static void test_empty_upload_rejected() {
  std::vector<uint8_t> none;
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(none).c_str()));
  TEST_ASSERT_FALSE(mgr.finish());
  expectFailed(UpdateError::Empty);
}

static void test_cooling_guard_denies() {
  guard.allow = false;
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_FALSE(mgr.start(fakeHashHex(img).c_str()));
  expectFailed(UpdateError::NotAllowed);
  TEST_ASSERT_EQUAL(0, plat.beginCalls);
  TEST_ASSERT_EQUAL(0, guard.restores);
}

static void test_guard_restored_on_failure() {
  auto img = makeImage(makeMarker("OTHER", 5, 0, 0));
  runUpdate(img, fakeHashHex(img));
  TEST_ASSERT_EQUAL(1, guard.restores);
}

static void test_no_space() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  plat.space = 0;
  TEST_ASSERT_FALSE(mgr.start(fakeHashHex(img).c_str()));
  expectFailed(UpdateError::NoSpace);
  TEST_ASSERT_EQUAL(1, guard.restores);

  setUp();
  plat.space = 0xFFF;  // kleiner als ein Sektor
  TEST_ASSERT_FALSE(mgr.start(fakeHashHex(img).c_str()));
  expectFailed(UpdateError::NoSpace);

  setUp();
  plat.beginOk = false;
  TEST_ASSERT_FALSE(mgr.start(fakeHashHex(img).c_str()));
  expectFailed(UpdateError::NoSpace);
}

static void test_write_failure() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  plat.writeLimit = 200;
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(img).c_str()));
  TEST_ASSERT_FALSE(feed(img, 64));
  expectFailed(UpdateError::WriteFailed);
  TEST_ASSERT_TRUE(plat.aborted);
  TEST_ASSERT_EQUAL(1, guard.restores);
}

static void test_second_start_rejected_while_busy() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  std::string h = fakeHashHex(img);
  TEST_ASSERT_TRUE(mgr.start(h.c_str()));
  TEST_ASSERT_FALSE(mgr.start(h.c_str()));
  TEST_ASSERT_EQUAL(UpdateState::Receiving, mgr.status().state);
  TEST_ASSERT_EQUAL(1, plat.beginCalls);
  TEST_ASSERT_EQUAL(1, guard.requests);
  TEST_ASSERT_TRUE(mgr.isBusy());
  // laufender Vorgang bleibt intakt
  TEST_ASSERT_TRUE(feed(img, 64));
  TEST_ASSERT_TRUE(mgr.finish());
}

static void test_start_rejected_while_reboot_pending() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(runUpdate(img, fakeHashHex(img)));
  TEST_ASSERT_FALSE(mgr.start(fakeHashHex(img).c_str()));
  TEST_ASSERT_EQUAL(UpdateState::RebootPending, mgr.status().state);
}

static void test_idle_timeout() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(img).c_str()));
  TEST_ASSERT_TRUE(mgr.write(img.data(), 10));
  plat.now += UpdateManager::kIdleTimeoutMs;
  mgr.tick();
  TEST_ASSERT_EQUAL(UpdateState::Receiving, mgr.status().state);
  plat.now += 1;
  mgr.tick();
  expectFailed(UpdateError::Timeout);
  TEST_ASSERT_TRUE(plat.aborted);
  TEST_ASSERT_EQUAL(1, guard.restores);
}

static void test_activity_resets_timeout() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(img).c_str()));
  plat.now += UpdateManager::kIdleTimeoutMs - 1;
  TEST_ASSERT_TRUE(mgr.write(img.data(), 10));
  plat.now += UpdateManager::kIdleTimeoutMs - 1;
  mgr.tick();
  TEST_ASSERT_EQUAL(UpdateState::Receiving, mgr.status().state);
}

static void test_timeout_survives_millis_wraparound() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  plat.now = 0xFFFFFF00u;
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(img).c_str()));
  plat.now += UpdateManager::kIdleTimeoutMs + 1;  // laeuft ueber 0
  mgr.tick();
  expectFailed(UpdateError::Timeout);
}

static void test_abort_by_client() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(img).c_str()));
  TEST_ASSERT_TRUE(mgr.write(img.data(), 50));
  mgr.abort();
  expectFailed(UpdateError::Aborted);
  TEST_ASSERT_TRUE(plat.aborted);
  TEST_ASSERT_EQUAL(1, guard.restores);
  TEST_ASSERT_FALSE(mgr.write(img.data(), 10));
}

static void test_abort_in_idle_or_after_success_is_noop() {
  mgr.abort();
  TEST_ASSERT_EQUAL(UpdateState::Idle, mgr.status().state);
  TEST_ASSERT_EQUAL(0, guard.restores);

  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(runUpdate(img, fakeHashHex(img)));
  mgr.abort();
  TEST_ASSERT_EQUAL(UpdateState::RebootPending, mgr.status().state);
  TEST_ASSERT_TRUE(plat.committed);
}

static void test_write_and_finish_ignored_when_not_receiving() {
  uint8_t b[4] = {1, 2, 3, 4};
  TEST_ASSERT_FALSE(mgr.write(b, 4));
  TEST_ASSERT_FALSE(mgr.finish());
  TEST_ASSERT_EQUAL(UpdateState::Idle, mgr.status().state);
  TEST_ASSERT_EQUAL(0, plat.flash.size());
}

static void test_retry_after_failure() {
  auto bad = makeImage(makeMarker("OTHER", 5, 0, 0));
  TEST_ASSERT_FALSE(runUpdate(bad, fakeHashHex(bad)));
  auto good = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(runUpdate(good, fakeHashHex(good)));
  TEST_ASSERT_EQUAL(UpdateState::RebootPending, mgr.status().state);
  TEST_ASSERT_EQUAL(UpdateError::None, mgr.status().error);
  TEST_ASSERT_TRUE(plat.flash == good);
}

static void test_progress_reported() {
  auto img = makeImage(makeMarker("VENTIQ_CORE_REVA", 5, 0, 0));
  TEST_ASSERT_TRUE(mgr.start(fakeHashHex(img).c_str()));
  TEST_ASSERT_EQUAL_UINT32(0x100000, mgr.status().capacity);
  TEST_ASSERT_TRUE(mgr.write(img.data(), 123));
  TEST_ASSERT_EQUAL_UINT32(123, mgr.status().received);
}

static void test_state_and_error_names() {
  TEST_ASSERT_EQUAL_STRING("idle", updateStateName(UpdateState::Idle));
  TEST_ASSERT_EQUAL_STRING("failed", updateStateName(UpdateState::Failed));
  TEST_ASSERT_EQUAL_STRING("downgrade_rejected", updateErrorName(UpdateError::Downgrade));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_update_commits_and_reboots_after_delay);
  RUN_TEST(test_marker_split_across_chunks);
  RUN_TEST(test_marker_at_start_and_end_of_image);
  RUN_TEST(test_same_version_allowed);
  RUN_TEST(test_uppercase_hash_accepted);
  RUN_TEST(test_downgrade_rejected);
  RUN_TEST(test_wrong_hardware_rejected);
  RUN_TEST(test_hardware_prefix_not_accepted);
  RUN_TEST(test_foreign_image_without_marker_rejected);
  RUN_TEST(test_two_markers_rejected);
  RUN_TEST(test_hash_mismatch_rejected);
  RUN_TEST(test_bad_hash_parameters);
  RUN_TEST(test_empty_upload_rejected);
  RUN_TEST(test_cooling_guard_denies);
  RUN_TEST(test_guard_restored_on_failure);
  RUN_TEST(test_no_space);
  RUN_TEST(test_write_failure);
  RUN_TEST(test_second_start_rejected_while_busy);
  RUN_TEST(test_start_rejected_while_reboot_pending);
  RUN_TEST(test_idle_timeout);
  RUN_TEST(test_activity_resets_timeout);
  RUN_TEST(test_timeout_survives_millis_wraparound);
  RUN_TEST(test_abort_by_client);
  RUN_TEST(test_abort_in_idle_or_after_success_is_noop);
  RUN_TEST(test_write_and_finish_ignored_when_not_receiving);
  RUN_TEST(test_retry_after_failure);
  RUN_TEST(test_progress_reported);
  RUN_TEST(test_state_and_error_names);
  return UNITY_END();
}
