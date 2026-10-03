#pragma once

#include "src/dependencies/json/ArduinoJson-v6.h"
#include <WString.h>

// Implemented by usermods/lora_rx when that usermod is linked.
// Weak defaults in aes_link.cpp allow other environments to build.

bool wledAesKeyActive();

// When a key is stored, a state or config document must be {"aes":"<base64 frame>"}.
// Read-only {"v":true} and {"lv":...} stay in clear. On success the document
// is replaced by the decrypted JSON.
// applyState is false for /json/cfg so a role change can still be saved.
bool wledAesAccept(JsonDocument& doc, bool applyState);

// HTTP /win. headerB64 is the same AES-128-GCM frame, whose plaintext is the URL.
bool wledAesAllowHttp(const String& url, const char* headerB64);
