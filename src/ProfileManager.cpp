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
  "author":"Hart Communicator 675",
  "match":{"manufacturerId":0,"deviceType":0},
  "pages":[]
})JSON";

ProfileManager::ProfileManager()
    : mounted(false), activeIsCustom(false), activeFile(GENERIC_NAME),
      matchKind("none"), uploading(false) {}

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

// Revision-aware matching with graceful fallback. For a connected device
// (manufacturerId / deviceType / deviceRevision) the best installed profile is
// chosen in this priority order:
//   1. exact  – manufacturerId + deviceType + revision all match
//   2. type   – manufacturerId + deviceType match, profile omits a revision
//               (revision-agnostic device profile)
//   3. lower  – same manufacturerId + deviceType, nearest revision below
//   4. higher – same manufacturerId + deviceType, nearest revision above
//   5. family – manufacturerId only (profile omits deviceType or uses 0xFFFF)
//   6. none   – no candidate; fall back to generic mode
bool ProfileManager::matchDevice(uint16_t manufacturerId, uint16_t deviceType,
                                 uint8_t deviceRevision) {
  if (!mounted) {
    return false;
  }
  File root = LittleFS.open(PROFILE_DIR);
  if (!root || !root.isDirectory()) {
    return false;
  }

  String exactMatch, typeMatch, familyMatch;
  String lowerMatch, higherMatch;
  int lowerRev = -1;      // highest revision strictly below deviceRevision
  int higherRev = 0x10000;  // lowest revision strictly above deviceRevision

  File f = root.openNextFile();
  while (f) {
    String name = baseName(String(f.name()));
    if (name != GENERIC_NAME && name.endsWith(".json")) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        JsonObject m = doc["match"];
        if (!m.isNull()) {
          uint16_t mid = m["manufacturerId"] | 0xFFFF;
          if (mid == manufacturerId) {
            bool hasDevType = m["deviceType"].is<int>();
            uint16_t dt = hasDevType ? (uint16_t)(int)m["deviceType"] : 0xFFFF;

            if (hasDevType && dt != 0xFFFF && dt == deviceType) {
              if (m["revision"].is<int>()) {
                int rev = (int)(m["revision"] | -1);
                if (rev == (int)deviceRevision) {
                  exactMatch = name;
                } else if (rev >= 0 && rev < (int)deviceRevision &&
                           rev > lowerRev) {
                  lowerRev = rev;
                  lowerMatch = name;
                } else if (rev > (int)deviceRevision && rev < higherRev) {
                  higherRev = rev;
                  higherMatch = name;
                }
              } else if (typeMatch.isEmpty()) {
                typeMatch = name;  // rev-agnostic profile for this device type
              }
            } else if ((!hasDevType || dt == 0xFFFF) && familyMatch.isEmpty()) {
              familyMatch = name;  // manufacturer-level fallback
            }
          }
        }
      }
    }
    f = root.openNextFile();
  }

  String found;
  String kind;
  if (exactMatch.length()) {
    found = exactMatch;  kind = "exact";
  } else if (typeMatch.length()) {
    found = typeMatch;   kind = "type";
  } else if (lowerMatch.length()) {
    found = lowerMatch;  kind = "lower";
  } else if (higherMatch.length()) {
    found = higherMatch; kind = "higher";
  } else if (familyMatch.length()) {
    found = familyMatch; kind = "family";
  }

  if (found.length()) {
    if (!activeIsCustom || activeFile != found || matchKind != kind) {
      activeFile = found;
      activeIsCustom = true;
      matchKind = kind;
      systemStatus.log("[PROFILE] Active (" + kind + "): " + found);
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
  matchKind = "none";
}

String ProfileManager::activeProfileJson() {
  return readProfile(activeFile);
}

String ProfileManager::statusJson() {
  String j = "{";
  j += "\"fsReady\":" + String(mounted ? "true" : "false") + ",";
  j += "\"custom\":" + String(activeIsCustom ? "true" : "false") + ",";
  j += "\"active\":\"" + activeFile + "\",";
  j += "\"matchQuality\":\"" + matchKind + "\",";
  j += "\"fsTotal\":" + String((uint32_t)fsTotalBytes()) + ",";
  j += "\"fsUsed\":" + String((uint32_t)fsUsedBytes());
  j += "}";
  return j;
}
