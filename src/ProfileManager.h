#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

// Manages Wireless HART Profile Format (WHPF) JSON files stored in LittleFS
// under /profiles. A profile describes manufacturer-specific pages and widgets
// that the web UI renders dynamically. The firmware never hardcodes device
// behaviour - it only renders what a profile requests.
//
// Matching: when a device is identified (manufacturer id / device type /
// revision), the manager looks for a profile whose "match" block matches and
// makes it active. If none match, the device runs in generic mode.
class ProfileManager {
public:
  ProfileManager();
  bool begin();  // mounts LittleFS, ensures /profiles + generic.json

  // List installed profiles as a JSON array of metadata objects.
  String listJson();
  // Raw JSON of a stored profile by filename (within /profiles).
  String readProfile(const String &filename);
  bool deleteProfile(const String &filename);
  // Streaming upload helpers (LittleFS file kept open between chunks).
  bool uploadBegin(const String &filename);
  bool uploadWrite(const uint8_t *data, size_t len);
  bool uploadEnd();
  void uploadAbort();

  // Try to select a profile matching the identified device. Returns true if a
  // (new) manufacturer profile became active. Falls back to generic.
  bool matchDevice(uint16_t manufacturerId, uint16_t deviceType,
                   uint8_t deviceRevision);
  void clearActive();

  bool hasActiveProfile() const { return activeIsCustom; }
  const String &activeFilename() const { return activeFile; }
  // How the active profile was selected: "exact", "type", "lower", "higher",
  // "family", or "none" (generic). Surfaced to the Profiles UI.
  const String &matchQuality() const { return matchKind; }
  // Active profile JSON (the matched custom profile, or generic.json).
  String activeProfileJson();
  // Small status object for the Profiles page header.
  String statusJson();

  bool fsReady() const { return mounted; }
  size_t fsTotalBytes();
  size_t fsUsedBytes();

private:
  bool mounted;
  bool activeIsCustom;
  String activeFile;          // e.g. "rosemount_3051_rev7.json" or "generic.json"
  String matchKind;           // "exact"/"type"/"lower"/"higher"/"family"/"none"
  File uploadFile;
  bool uploading;

  void ensureGeneric();
};

extern ProfileManager profiles;

#endif  // PROFILE_MANAGER_H
