#pragma once

#include <stddef.h>
#include <stdint.h>

namespace UpdateBundle {

constexpr uint8_t MAGIC[] = {'V', 'T', 'U', 'B', 'N', 'D', 'L', '1'};
constexpr uint8_t END_MAGIC[] = {'V', 'T', 'U', 'E', 'N', 'D', '0', '1'};
constexpr size_t MAGIC_SIZE = sizeof(MAGIC);
constexpr size_t END_MAGIC_SIZE = sizeof(END_MAGIC);
constexpr size_t HEADER_SIZE = MAGIC_SIZE + 2;
constexpr size_t MAX_MANIFEST_SIZE = 1024;
constexpr uint8_t MAX_ASSETS = 6;
constexpr size_t ASSET_NAME_SIZE = 24;
constexpr char STAGING_DIRECTORY[] = "/ota_stage";
constexpr char JOURNAL_PATH[] = "/ota_journal.json";
constexpr char const *ALLOWED_ASSETS[MAX_ASSETS] = {
    "index.html", "app.css", "app.js", "ota.js", "appicon.svg", "logo.svg"};

}  // namespace UpdateBundle
