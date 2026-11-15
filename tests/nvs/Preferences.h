#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct FakeNvs { static std::map<std::string, std::vector<uint8_t>>& m() { static std::map<std::string, std::vector<uint8_t>> x; return x; } };
struct Preferences {
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  bool clear() { FakeNvs::m().clear(); return true; }
  bool isKey(const char* k) { return FakeNvs::m().count(k) > 0; }
  void remove(const char* k) { FakeNvs::m().erase(k); }
  template <class T> size_t putT(const char* k, T v) { std::vector<uint8_t> b(sizeof(T)); memcpy(b.data(), &v, sizeof(T)); FakeNvs::m()[k] = b; return sizeof(T); }
  template <class T> T getT(const char* k, T d) { auto it = FakeNvs::m().find(k); if (it == FakeNvs::m().end() || it->second.size() != sizeof(T)) return d; T v; memcpy(&v, it->second.data(), sizeof(T)); return v; }
  size_t putUInt(const char* k, uint32_t v) { return putT<uint32_t>(k, v); }
  size_t putInt(const char* k, int32_t v) { return putT<int32_t>(k, v); }
  size_t putUShort(const char* k, uint16_t v) { return putT<uint16_t>(k, v); }
  size_t putUChar(const char* k, uint8_t v) { return putT<uint8_t>(k, v); }
  size_t putBool(const char* k, bool v) { return putT<uint8_t>(k, v ? 1 : 0); }
  uint32_t getUInt(const char* k, uint32_t d) { return getT<uint32_t>(k, d); }
  int32_t getInt(const char* k, int32_t d) { return getT<int32_t>(k, d); }
  uint16_t getUShort(const char* k, uint16_t d) { return getT<uint16_t>(k, d); }
  uint8_t getUChar(const char* k, uint8_t d) { return getT<uint8_t>(k, d); }
  bool getBool(const char* k, bool d) { return getT<uint8_t>(k, d ? 1 : 0) != 0; }
  size_t putString(const char* k, const char* s) { size_t n = strlen(s); std::vector<uint8_t> b(s, s + n); FakeNvs::m()[k] = b; return n ? n : 1; }
  size_t getString(const char* k, char* out, size_t cap) { auto it = FakeNvs::m().find(k); if (it == FakeNvs::m().end()) { if (cap) out[0] = 0; return 0; } size_t n = it->second.size() < cap - 1 ? it->second.size() : cap - 1; if (n) memcpy(out, it->second.data(), n); out[n] = 0; return n; }
  size_t putBytes(const char* k, const void* p, size_t n) { FakeNvs::m()[k] = std::vector<uint8_t>((const uint8_t*)p, (const uint8_t*)p + n); return n; }
  size_t getBytes(const char* k, void* out, size_t n) { auto it = FakeNvs::m().find(k); if (it == FakeNvs::m().end()) return 0; size_t m = it->second.size() < n ? it->second.size() : n; memcpy(out, it->second.data(), m); return m; }
  size_t getBytesLength(const char* k) { auto it = FakeNvs::m().find(k); return it == FakeNvs::m().end() ? 0 : it->second.size(); }
};
