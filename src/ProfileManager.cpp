#include "ProfileManager.h"

#include <ArduinoJson.h>

#include "SystemStatus.h"

ProfileManager profiles;

static const char *PROFILE_DIR = "/profiles";
static const char *GENERIC_NAME = "generic.json";

// Minimal built-in generic profile. The static HART Device / Maintenance pages
// cover generic functionality, so this carries no dynamic pages by default.
static const char GENERIC_JSON[] PROGMEM = R"JSON({
  "manufacturer":"Generic",
  "device":"HART Device",
  "revision":0,
  "version":"1.0",
  "author":"Wireless HART 67",
  "match":{"manufacturerId":0,"deviceType":0},
  "pages":[]
})JSON";

ProfileManager::ProfileManager()
    : mounted(false), activeIsCustom(false), activeFile(GENERIC_NAME),
      uploading(false) {}

bool ProfileManager::begin() {
  if (!LittleFS.begin(true)) {  // format on first boot if needed
    systemStatus.log("[FS] LittleFS mount failed");
    mounted = false;
    return false;
  }
  mounted = true;
  if (!LittleFS.exists(PROFILE_DIR)) {
    LittleFS.mkdir(PROFILE_DIR);
  }
  ensureGeneric();
  systemStatus.log("[FS] LittleFS ready (" + String(fsUsedBytes() / 1024) +
                   "/" + String(fsTotalBytes() / 1024) + " KB)");
  return true;
}

void ProfileManager::ensureGeneric() {
  String path = String(PROFILE_DIR) + "/" + GENERIC_NAME;
  if (LittleFS.exists(path)) {
    return;
  }
  File f = LittleFS.open(path, "w");
  if (f) {
    f.print((const __FlashStringHelper *)GENERIC_JSON);
    f.close();
  }
}

static String baseName(const String &path) {
  int slash = path.lastIndexOf('/');
  return (slash >= 0) ? path.substring(slash + 1) : path;
}

size_t ProfileManager::fsTotalBytes() {
  return mounted ? LittleFS.totalBytes() : 0;
}
size_t ProfileManager::fsUsedBytes() {
  return mounted ? LittleFS.usedBytes() : 0;
}

String ProfileManager::listJson() {
  String out = "[";
  if (!mounted) {
    return out + "]";
  }
  File root = LittleFS.open(PROFILE_DIR);
  if (!root || !root.isDirectory()) {
    return out + "]";
  }
  bool first = true;
  File f = root.openNextFile();
  while (f) {
    String name = baseName(String(f.name()));
    if (name.endsWith(".json")) {
      // Parse a little metadata from each profile.
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, f);
      if (!first) {
        out += ",";
      }
      first = false;
      out += "{\"file\":\"" + name + "\",";
      out += "\"size\":" + String(f.size()) + ",";
      if (!err) {
        out += "\"manufacturer\":\"" +
               String(doc["manufacturer"] | "?") + "\",";
        out += "\"device\":\"" + String(doc["device"] | "?") + "\",";
        out += "\"revision\":" + String((int)(doc["revision"] | 0)) + ",";
        out += "\"version\":\"" + String(doc["version"] | "") + "\",";
        out += "\"author\":\"" + String(doc["author"] | "") + "\",";
        out += "\"pages\":" + String(doc["pages"].is<JsonArray>()
                                         ? doc["pages"].size()
                                         : 0) + ",";
        out += "\"valid\":true}";
      } else {
        out += "\"valid\":false}";
      }
    }
    f = root.openNextFile();
  }
  out += "]";
  return out;
}

String ProfileManager::readProfile(const String &filename) {
  String path = String(PROFILE_DIR) + "/" + baseName(filename);
  if (!mounted || !LittleFS.exists(path)) {
    return "{}";
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    return "{}";
  }
  String s = f.readString();
  f.close();
  return s;
}

bool ProfileManager::deleteProfile(const String &filename) {
  String name = baseName(filename);
  if (name == GENERIC_NAME) {
    return false;  // protect the fallback
  }
  String path = String(PROFILE_DIR) + "/" + name;
  if (!mounted || !LittleFS.exists(path)) {
    return false;
  }
  bool ok = LittleFS.remove(path);
  if (ok && activeFile == name) {
    clearActive();
  }
  return ok;
}

bool ProfileManager::uploadBegin(const String &filename) {
  if (!mounted) {
    return false;
  }
  String name = baseName(filename);
  if (!name.endsWith(".json")) {
    name += ".json";
  }
  String path = String(PROFILE_DIR) + "/" + name;
  uploadFile = LittleFS.open(path, "w");
  uploading = (bool)uploadFile;
  return uploading;
}

bool ProfileManager::uploadWrite(const uint8_t *data, size_t len) {
  if (!uploading) {
    return false;
  }
  return uploadFile.write(data, len) == len;
}

bool ProfileManager::uploadEnd() {
  if (!uploading) {
    return false;
  }
  uploadFile.close();
  uploading = false;
  return true;
}

void ProfileManager::uploadAbort() {
  if (uploading) {
    uploadFile.close();
    uploading = false;
  }
}

bool ProfileManager::matchDevice(uint16_t manufacturerId, uint16_t deviceType,
                                 uint8_t deviceRevision) {
  if (!mounted) {
    return false;
  }
  File root = LittleFS.open(PROFILE_DIR);
  if (!root || !root.isDirectory()) {
    return false;
  }
  String found;
  File f = root.openNextFile();
  while (f) {
    String name = baseName(String(f.name()));
    if (name != GENERIC_NAME && name.endsWith(".json")) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        JsonObject m = doc["match"];
        if (!m.isNull()) {
          uint16_t mid = m["manufacturerId"] | 0xFFFF;
          uint16_t dt = m["deviceType"] | 0xFFFF;
          bool revOk = true;
          if (m["revision"].is<int>()) {
            revOk = ((uint8_t)(m["revision"] | 0) == deviceRevision);
          }
          if (mid == manufacturerId && dt == deviceType && revOk) {
            found = name;
          }
        }
      }
    }
    f = root.openNextFile();
  }

  if (found.length()) {
    if (!activeIsCustom || activeFile != found) {
      activeFile = found;
      activeIsCustom = true;
      systemStatus.log("[PROFILE] Active: " + found);
    }
    return true;
  }
  clearActive();
  return false;
}

void ProfileManager::clearActive() {
  if (activeIsCustom) {
    systemStatus.log("[PROFILE] Generic mode");
  }
  activeIsCustom = false;
  activeFile = GENERIC_NAME;
}

String ProfileManager::activeProfileJson() {
  return readProfile(activeFile);
}

String ProfileManager::statusJson() {
  String j = "{";
  j += "\"fsReady\":" + String(mounted ? "true" : "false") + ",";
  j += "\"custom\":" + String(activeIsCustom ? "true" : "false") + ",";
  j += "\"active\":\"" + activeFile + "\",";
  j += "\"fsTotal\":" + String((uint32_t)fsTotalBytes()) + ",";
  j += "\"fsUsed\":" + String((uint32_t)fsUsedBytes());
  j += "}";
  return j;
}
