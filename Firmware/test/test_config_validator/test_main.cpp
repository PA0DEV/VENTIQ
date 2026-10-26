#include <unity.h>

#include <string.h>

#include "ConfigValidator.h"
#include "Defaults.h"

void setUp() {}
void tearDown() {}

static AppConfig validStaticConfig() {
  AppConfig config = ConfigDefaults::makeAppConfig();
  strcpy(config.wifi.ssid, "VENTIQ");
  strcpy(config.wifi.password, "correct-horse");
  config.wifi.useStaticIp = true;
  config.wifi.ipAddress = 0xC0A80164UL;
  config.wifi.subnetMask = 0xFFFFFF00UL;
  config.wifi.gateway = 0xC0A80101UL;
  config.wifi.dnsServer = 0x08080808UL;
  return config;
}

static void test_empty_default_is_valid() {
  const AppConfig config = ConfigDefaults::makeAppConfig();
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(config));
}

static void test_static_network_is_valid() {
  const AppConfig config = validStaticConfig();
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(config));
}

static void test_password_requires_valid_length() {
  AppConfig config = validStaticConfig();
  strcpy(config.wifi.password, "short");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidPassword, validateConfig(config));
}

static void test_static_gateway_must_share_subnet() {
  AppConfig config = validStaticConfig();
  config.wifi.gateway = 0xC0A80201UL;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidStaticNetwork, validateConfig(config));
}

static void test_ssid_must_be_terminated() {
  AppConfig config = ConfigDefaults::makeAppConfig();
  memset(config.wifi.ssid, 'x', sizeof(config.wifi.ssid));
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidSsid, validateConfig(config));
}

static void test_unknown_schema_is_rejected() {
  AppConfig config = ConfigDefaults::makeAppConfig();
  config.schemaVersion++;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::UnsupportedSchema, validateConfig(config));
}

static AppConfig validMqtt() {
  AppConfig config = ConfigDefaults::makeAppConfig();
  config.mqtt.enabled = true;
  setMqttHost(config.mqtt, "broker.local");
  setMqttUsername(config.mqtt, "user");
  setMqttPassword(config.mqtt, "secret");
  return config;
}

static void test_mqtt_defaults() {
  const AppConfig c = ConfigDefaults::makeAppConfig();
  TEST_ASSERT_FALSE(c.mqtt.enabled);
  TEST_ASSERT_EQUAL_UINT16(1883, c.mqtt.port);
  TEST_ASSERT_EQUAL_STRING("ventiq/core", c.mqtt.topicPrefix);
  TEST_ASSERT_EQUAL_UINT16(10, c.mqtt.publishIntervalSec);
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
  TEST_ASSERT_EQUAL_UINT16(3, c.schemaVersion);
}

static void test_mqtt_valid_and_enabled_needs_host() {
  AppConfig c = validMqtt();
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
  setMqttHost(c.mqtt, "192.168.1.10");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
  setMqttHost(c.mqtt, "");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttHost, validateConfig(c));
  c.mqtt.enabled = false;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
  setMqttHost(c.mqtt, "bad host");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttHost, validateConfig(c));
}

static void test_mqtt_port_and_interval_bounds() {
  AppConfig c = validMqtt();
  c.mqtt.port = 0;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttPort, validateConfig(c));
  c = validMqtt();
  c.mqtt.publishIntervalSec = 0;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttPublishInterval, validateConfig(c));
  c.mqtt.publishIntervalSec = 3601;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttPublishInterval, validateConfig(c));
  c.mqtt.publishIntervalSec = 3600;
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
}

static void test_mqtt_topic_prefix_rules() {
  const char *bad[] = {"", "/a", "a/", "a//b", "a/#", "a/+/b", "a b", "$SYS/x"};
  for (const char *p : bad) {
    AppConfig c = validMqtt();
    TEST_ASSERT_TRUE(setMqttTopicPrefix(c.mqtt, p));
    TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttTopicPrefix, validateConfig(c));
  }
  AppConfig c = validMqtt();
  setMqttTopicPrefix(c.mqtt, "ventiq/rack-1/top");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
}

static void test_mqtt_client_id_and_credentials() {
  AppConfig c = validMqtt();
  setMqttClientId(c.mqtt, "bad id");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttClientId, validateConfig(c));
  setMqttClientId(c.mqtt, "ventiq-01");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::None, validateConfig(c));
  setMqttUsername(c.mqtt, "");
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttPassword, validateConfig(c));
}

static void test_mqtt_setters_reject_overlong_and_keep_value() {
  AppConfig c = validMqtt();
  char longText[100];
  memset(longText, 'a', sizeof(longText) - 1);
  longText[sizeof(longText) - 1] = '\0';
  TEST_ASSERT_FALSE(setMqttHost(c.mqtt, longText));
  TEST_ASSERT_FALSE(setMqttClientId(c.mqtt, "123456789012345678901234"));
  TEST_ASSERT_FALSE(setMqttPassword(c.mqtt, nullptr));
  TEST_ASSERT_EQUAL_STRING("broker.local", c.mqtt.host);
}

static void test_mqtt_unterminated_rejected() {
  AppConfig c = validMqtt();
  memset(c.mqtt.topicPrefix, 'a', sizeof(c.mqtt.topicPrefix));
  TEST_ASSERT_EQUAL_INT(ConfigValidationError::InvalidMqttTopicPrefix, validateConfig(c));
}

static void test_mqtt_public_snapshot_hides_password() {
  const AppConfig c = validMqtt();
  const MqttPublicConfig p = makeMqttPublicConfig(c.mqtt);
  TEST_ASSERT_TRUE(p.passwordSet);
  TEST_ASSERT_EQUAL_STRING("user", p.username);
  const AppConfig d = ConfigDefaults::makeAppConfig();
  TEST_ASSERT_FALSE(makeMqttPublicConfig(d.mqtt).passwordSet);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mqtt_defaults);
  RUN_TEST(test_mqtt_valid_and_enabled_needs_host);
  RUN_TEST(test_mqtt_port_and_interval_bounds);
  RUN_TEST(test_mqtt_topic_prefix_rules);
  RUN_TEST(test_mqtt_client_id_and_credentials);
  RUN_TEST(test_mqtt_setters_reject_overlong_and_keep_value);
  RUN_TEST(test_mqtt_unterminated_rejected);
  RUN_TEST(test_mqtt_public_snapshot_hides_password);
  RUN_TEST(test_empty_default_is_valid);
  RUN_TEST(test_static_network_is_valid);
  RUN_TEST(test_password_requires_valid_length);
  RUN_TEST(test_static_gateway_must_share_subnet);
  RUN_TEST(test_ssid_must_be_terminated);
  RUN_TEST(test_unknown_schema_is_rejected);
  return UNITY_END();
}
