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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_default_is_valid);
  RUN_TEST(test_static_network_is_valid);
  RUN_TEST(test_password_requires_valid_length);
  RUN_TEST(test_static_gateway_must_share_subnet);
  RUN_TEST(test_ssid_must_be_terminated);
  RUN_TEST(test_unknown_schema_is_rejected);
  return UNITY_END();
}
