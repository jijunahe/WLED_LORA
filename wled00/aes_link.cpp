#include "wled.h"
#include "aes_link.h"

__attribute__((weak)) bool wledAesKeyActive() {
  return false;
}

__attribute__((weak)) bool wledAesAccept(JsonDocument& doc, bool applyState) {
  (void)doc;
  (void)applyState;
  return true;
}

__attribute__((weak)) bool wledAesAllowHttp(const String& url, const char* headerB64) {
  (void)url;
  (void)headerB64;
  return true;
}
