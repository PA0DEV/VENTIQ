#include <unity.h>
#include "MqttLogic.h"

using namespace MqttLogic;

void setUp() {}
void tearDown() {}

static void test_topic_basic() {
  char t[TOPIC_MAX];
  TEST_ASSERT_TRUE(buildTopic(t, sizeof(t), "ventiq/core", "availability"));
  TEST_ASSERT_EQUAL_STRING("ventiq/core/availability", t);
}

static void test_topic_trailing_slash_and_overflow() {
  char t[TOPIC_MAX];
  TEST_ASSERT_TRUE(buildTopic(t, sizeof(t), "a/b/", "status/x"));
  TEST_ASSERT_EQUAL_STRING("a/b/status/x", t);
  char small[8];
  TEST_ASSERT_FALSE(buildTopic(small, sizeof(small), "ventiq/core", "availability"));
  TEST_ASSERT_EQUAL_STRING("", small);
  TEST_ASSERT_FALSE(buildTopic(t, sizeof(t), "", "x"));
  TEST_ASSERT_FALSE(buildTopic(t, sizeof(t), "p", ""));
  TEST_ASSERT_FALSE(buildTopic(t, sizeof(t), nullptr, "x"));
}

static void test_topic_max_prefix_fits() {
  char prefix[MqttConfig::TOPIC_PREFIX_MAX + 1];
  memset(prefix, 'a', MqttConfig::TOPIC_PREFIX_MAX);
  prefix[MqttConfig::TOPIC_PREFIX_MAX] = '\0';
  char t[TOPIC_MAX];
  TEST_ASSERT_TRUE(buildTopic(t, sizeof(t), prefix, "status/device/free_heap"));
}

static void test_client_id() {
  char id[MqttConfig::CLIENT_ID_MAX + 1];
  TEST_ASSERT_TRUE(deriveClientId(id, sizeof(id), "", 0x00ABCDEF));
  TEST_ASSERT_EQUAL_STRING("ventiq-abcdef", id);
  TEST_ASSERT_TRUE(deriveClientId(id, sizeof(id), nullptr, 0xFF000001));
  TEST_ASSERT_EQUAL_STRING("ventiq-000001", id);
  TEST_ASSERT_TRUE(deriveClientId(id, sizeof(id), "mine", 1));
  TEST_ASSERT_EQUAL_STRING("mine", id);
  char tiny[5];
  TEST_ASSERT_FALSE(deriveClientId(tiny, sizeof(tiny), "", 1));
}

static void test_backoff() {
  TEST_ASSERT_EQUAL_UINT32(0, backoffMs(0));
  TEST_ASSERT_EQUAL_UINT32(5000, backoffMs(1));
  TEST_ASSERT_EQUAL_UINT32(10000, backoffMs(2));
  TEST_ASSERT_EQUAL_UINT32(20000, backoffMs(3));
  TEST_ASSERT_EQUAL_UINT32(40000, backoffMs(4));
  TEST_ASSERT_EQUAL_UINT32(60000, backoffMs(5));
  TEST_ASSERT_EQUAL_UINT32(60000, backoffMs(255));
}

static void test_time_reached_wraps() {
  TEST_ASSERT_TRUE(timeReached(100, 100));
  TEST_ASSERT_FALSE(timeReached(99, 100));
  TEST_ASSERT_TRUE(timeReached(5, 0xFFFFFFF0u));
  TEST_ASSERT_FALSE(timeReached(0xFFFFFFF0u, 5));
}

static void test_may_connect() {
  MqttConfig c;
  TEST_ASSERT_FALSE(mayConnect(c, true, true));
  c.enabled = true;
  TEST_ASSERT_FALSE(mayConnect(c, true, true));
  strcpy(c.host, "192.168.1.2");
  TEST_ASSERT_TRUE(mayConnect(c, true, true));
  TEST_ASSERT_FALSE(mayConnect(c, true, false));
  TEST_ASSERT_FALSE(mayConnect(c, false, true));
}

static void test_same_config() {
  MqttConfig a, b;
  TEST_ASSERT_TRUE(sameConfig(a, b));
  strcpy(b.host, "h");
  TEST_ASSERT_FALSE(sameConfig(a, b));
  strcpy(a.host, "h");
  b.publishIntervalSec = 5;
  TEST_ASSERT_FALSE(sameConfig(a, b));
  b.publishIntervalSec = a.publishIntervalSec;
  strcpy(b.password, "x");
  TEST_ASSERT_FALSE(sameConfig(a, b));
}

static void test_state_names() {
  TEST_ASSERT_EQUAL_STRING("refused_credentials", stateName(4));
  TEST_ASSERT_EQUAL_STRING("connection_timeout", stateName(-4));
  TEST_ASSERT_EQUAL_STRING("unknown", stateName(42));
}

static void test_incoming_gate() {
  TEST_ASSERT_EQUAL((int)IncomingVerdict::RejectRetained, (int)checkIncoming(true, 3));
  TEST_ASSERT_EQUAL((int)IncomingVerdict::RejectEmpty, (int)checkIncoming(false, 0));
  TEST_ASSERT_EQUAL((int)IncomingVerdict::RejectOversize, (int)checkIncoming(false, PAYLOAD_MAX + 1));
  TEST_ASSERT_EQUAL((int)IncomingVerdict::Accept, (int)checkIncoming(false, PAYLOAD_MAX));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_topic_basic);
  RUN_TEST(test_topic_trailing_slash_and_overflow);
  RUN_TEST(test_topic_max_prefix_fits);
  RUN_TEST(test_client_id);
  RUN_TEST(test_backoff);
  RUN_TEST(test_time_reached_wraps);
  RUN_TEST(test_may_connect);
  RUN_TEST(test_same_config);
  RUN_TEST(test_state_names);
  RUN_TEST(test_incoming_gate);
  return UNITY_END();
}
