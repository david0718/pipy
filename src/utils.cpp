/*
 *  Copyright (c) 2019 by flomesh.io
 *
 *  Unless prior written consent has been obtained from the copyright
 *  owner, the following shall not be allowed.
 *
 *  1. The distribution of any source codes, header files, make files,
 *     or libraries of the software.
 *
 *  2. Disclosure of any source codes pertaining to the software to any
 *     additional parties.
 *
 *  3. Alteration or removal of any notices in or on the software or
 *     within the documentation included within the software.
 *
 *  ALL SOURCE CODE AS WELL AS ALL DOCUMENTATION INCLUDED WITH THIS
 *  SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION, WITHOUT WARRANTY OF ANY
 *  KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "utils.hpp"

#include <cmath>
#include <cstring>
#include <chrono>
#include <random>
#include <limits>

namespace pipy {
namespace utils {

static int get_dec_uint8(const char **ptr) {
  auto *s = *ptr;
  if (*s == '0') {
    *ptr = s + 1;
    return 0;
  }
  int n = 0;
  for (int i = 0; i < 3; i++, s++) {
    auto c = *s;
    if (c < '0' || c > '9') break;
    n = (n * 10) + (c - '0');
  }
  *ptr = s;
  return n < 256 ? n : -1;
}

static int get_hex_uint16(const char **ptr) {
  auto *s = *ptr;
  int n = 0;
  for (int i = 0; i < 4; i++, s++) {
    auto c = *s;
    if ('0' <= c && c <= '9') n = (n << 4) | (c - '0'); else
    if ('a' <= c && c <= 'f') n = (n << 4) | (c - 'a' + 10); else
    if ('A' <= c && c <= 'F') n = (n << 4) | (c - 'A' + 10); else
    if (!i) return -1; else break;
  }
  *ptr = s;
  return n;
}

static int get_port_number(const char *str) {
  const char *p = str;
  int n = 0;
  for (int i = 0; *p && i < 5; i++) {
    auto c = *p++;
    if (c < '0' || c > '9') return -1;
    n = (n * 10) + (c - '0');
  }
  if (*p) return -1;
  if (n < 1 || n > 65535) return -1;
  return n;
}

auto to_string(double n) -> std::string {
  if (std::isnan(n)) return "NaN";
  if (std::isinf(n)) return n > 0 ? "Infinity" : "-Infinity";
  double i; std::modf(n, &i);
  if (std::modf(n, &i) == 0) return std::to_string(int64_t(i));
  return std::to_string(n);
}

auto now() -> double {
  auto t = std::chrono::system_clock::now().time_since_epoch();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
  return double(ms);
}

bool is_host_port(const std::string &str) {
  std::string host;
  int port;
  return get_host_port(str, host, port);
}

bool get_host_port(const std::string &str, std::string &host, int &port) {
  if (str.length() > 0 && str[0] == '[') {
    auto p = str.find_last_of(']');
    if (p == std::string::npos) return false;
    if (p + 1 >= str.length() || str[p + 1] != ':') return false;
    host = str.substr(1, p - 1);
    port = get_port_number(&str[p + 2]);
    uint16_t ip[8];
    return (port > 0 && get_ip_v6(host, ip));
  } else {
    auto p = str.find_last_of(':');
    if (p == std::string::npos) return false;
    host = str.substr(0, p);
    port = get_port_number(&str[p + 1]);
    return (port > 0);
  }
}

bool get_ip_v4(const std::string &str, uint8_t ip[]) {
  return get_ip_v4(str.c_str(), ip);
}

bool get_ip_v6(const std::string &str, uint8_t ip[]) {
  return get_ip_v6(str.c_str(), ip);
}

bool get_ip_v6(const std::string &str, uint16_t ip[]) {
  return get_ip_v6(str.c_str(), ip);
}

bool get_ip_v4(const char *str, uint8_t ip[]) {
  const char *p = str;
  for (int i = 0; i < 4; i++, p++) {
    auto n = get_dec_uint8(&p);
    if (n < 0 ||
      (i <= 2 && *p != '.') ||
      (i == 3 && *p != '\0')
    ) {
      return false;
    }
    ip[i] = n;
  }
  return true;
}

bool get_ip_v6(const char *str, uint8_t ip[]) {
  uint16_t buf[8];
  if (!get_ip_v6(str, buf)) return false;
  for (int i = 0; i < 8; i++) {
    ip[i*2+0] = buf[i] >> 8;
    ip[i*2+1] = buf[i];
  }
  return true;
}

bool get_ip_v6(const char *str, uint16_t ip[]) {
  const char *p = str;
  uint16_t head[8]; int head_len = 0;
  uint16_t tail[8]; int tail_len = 0;

  if (*p == '\0') return false;

  if (*p == ':') {
    p++; if (*p != ':') return false;
    p++;
  } else {
    for (int i = 0; i < 8; i++, p++) {
      if (*p == ':') { p++; break; }
      auto n = get_hex_uint16(&p);
      if (n < 0) return false;
      head[head_len++] = n;
      if (*p == '%' || !*p) break;
      if (*p != ':') return false;
    }
  }

  if (*p) {
    for (int i = 0; i < 8; i++, p++) {
      if (*p == ':') return false;
      auto n = get_hex_uint16(&p);
      if (n < 0) return false;
      tail[tail_len++] = n;
      if (*p == '%' || !*p) break;
      if (*p != ':') return false;
    }
  }

  auto zero_len = 8 - head_len - tail_len;
  if (zero_len < 0) return false;
  if (zero_len == 0 && head_len > 0 && tail_len > 0) return false;

  for (int i = 0; i < head_len; i++) ip[i] = head[i];
  for (int i = 0; i < zero_len; i++) ip[i+head_len] = 0;
  for (int i = 0; i < tail_len; i++) ip[i+head_len+zero_len] = tail[i];

  return true;
}

bool get_cidr(const std::string &str, uint8_t ip[], int &mask) {
  const char *p = str.c_str();
  for (int i = 0; i < 4; i++) {
    if (i > 0) p++;
    auto n = get_dec_uint8(&p);
    if (n < 0 ||
      (i <= 2 && *p != '.') ||
      (i == 3 && *p != '/' && *p != '\0')
    ) {
      return false;
    }
    ip[i] = n;
  }

  int m;
  if (*p == '/') {
    p++;
    m = get_dec_uint8(&p);
    if (m < 0 || m > 32 || *p != '\0') return false;
  } else {
    m = 32;
  }

  mask = m;
  return true;
}

auto get_size(const std::string &str, int thousand) -> double {
  if (!str.empty()) {
    const char *s = str.c_str();
    char *str_end = nullptr;
    auto n = std::strtod(s, &str_end);
    if (!*str_end) return n;
    switch (std::tolower(*str_end)) {
      case 't': n *= thousand;
      case 'g': n *= thousand;
      case 'm': n *= thousand;
      case 'k': n *= thousand; return n;
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

auto get_binary_size(const std::string &str) -> double {
  return get_size(str, 1024);
}

auto get_byte_size(const std::string &str) -> size_t {
  if (str.empty()) return 0;
  size_t n = std::atoi(str.c_str());
  switch (std::tolower(str.back())) {
    case 't': n *= 1024;
    case 'g': n *= 1024;
    case 'm': n *= 1024;
    case 'k': n *= 1024;
  }
  return n;
}

bool get_byte_size(const pjs::Value &val, size_t &out) {
  if (val.is_undefined()) return true;
  if (val.is_number()) {
    out = val.n();
    return true;
  }
  if (val.is_string()) {
    out = get_byte_size(val.s()->str());
    return true;
  }
  return false;
}

auto get_seconds(const std::string &str) -> double {
  if (!str.empty()) {
    const char *s = str.c_str();
    char *str_end = nullptr;
    auto n = std::strtod(s, &str_end);
    if (!*str_end) return n;
    switch (std::tolower(*str_end)) {
      case 'd': n *= 24;
      case 'h': n *= 60;
      case 'm': n *= 60;
      case 's': n *= 1; return n;
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

bool get_seconds(const pjs::Value &val, double &out) {
  if (val.is_undefined()) return true;
  if (val.is_number()) {
    out = val.n();
    return true;
  }
  if (val.is_string()) {
    out = get_seconds(val.s()->str());
    return true;
  }
  return false;
}

void gen_uuid_v4(std::string &str) {
  thread_local static std::random_device rd;
  thread_local static std::mt19937_64 rn1, rn2;
  thread_local static bool is_initialized = false;

  if (!is_initialized) {
    union {
      uint64_t u64;
      uint32_t u32[2];
    } s1, s2;
    s1.u32[0] = rd();
    s1.u32[1] = rd();
    s2.u32[0] = rd();
    s2.u32[1] = rd();
    rn1.seed(s1.u64);
    rn2.seed(s2.u64);
    is_initialized = true;
  }

  union {
    uint64_t u64[2];
    uint8_t u8[16];
  } bits;

  bits.u64[0] = rn1();
  bits.u64[1] = rn2();

  static const char *format = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  static const char *hex_x = "0123456789abcdef";
  static const char *hex_y = hex_x + 8;

  str.resize(36);

  for (int i = 0, p = 0; i < 36; i++) {
    auto c = format[i];
    switch (c) {
      case 'x': str[i] = hex_x[0xf & (bits.u8[p>>1] >> ((p&1)*4))]; p++; break;
      case 'y': str[i] = hex_y[0x3 & (bits.u8[p>>1] >> ((p&1)*4))]; p++; break;
      case '-': str[i] = c; break;
      default : str[i] = c; p++; break;
    }
  }
}

bool starts_with(const std::string &str, const std::string &prefix) {
  if (str.length() < prefix.length()) return false;
  return !std::strncmp(str.c_str(), prefix.c_str(), prefix.length());
}

bool ends_with(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length()) return false;
  return !std::strncmp(str.c_str() + str.length() - suffix.length(), suffix.c_str(), suffix.length());
}

bool iequals(const std::string &a, const std::string &b) {
  if (a.length() != b.length()) return false;
  for (size_t i = 0; i < a.length(); i++) {
    auto ca = std::tolower(a[i]);
    auto cb = std::tolower(b[i]);
    if (ca != cb) return false;
  }
  return true;
}

auto trim(const std::string &str) -> std::string {
  size_t n = str.length(), i = 0, j = n;
  while (i < n && str[i] <= ' ') i++;
  while (j > 0 && str[j-1] <= ' ') j--;
  return j >= i ? str.substr(i, j - i) : std::string();
}

auto split(const std::string &str, char sep) -> std::list<std::string> {
  std::list<std::string> list;
  size_t p = 0;
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] == sep) {
      list.push_back(str.substr(p, i - p));
      p = i + 1;
    }
  }
  list.push_back(str.substr(p, str.length() - p));
  return list;
}

auto lower(const std::string &str) -> std::string {
  auto lstr = str;
  for (size_t i = 0; i < lstr.length(); i++) lstr[i] = std::tolower(lstr[i]);
  return lstr;
}

void escape(const std::string &str, const std::function<void(char)> &out) {
  for (auto c : str) {
    switch (c) {
      case '"' : out('\\'); out('"'); break;
      case '\\': out('\\'); out('\\'); break;
      case '\a': out('\\'); out('a'); break;
      case '\b': out('\\'); out('b'); break;
      case '\f': out('\\'); out('f'); break;
      case '\n': out('\\'); out('n'); break;
      case '\r': out('\\'); out('r'); break;
      case '\t': out('\\'); out('t'); break;
      case '\v': out('\\'); out('v'); break;
      default: out(c); break;
    }
  }
}

auto escape(const std::string &str) -> std::string {
  std::string str2;
  escape(str, [&](char c) { str2 += c; });
  return str2;
}

void unescape(const std::string &str, const std::function<void(char)> &out) {
  for (size_t i = 0; i < str.size(); i++) {
    auto c = str[i];
    if (c == '\\') {
      c = str[++i];
      if (!c) break;
      switch (c) {
        case 'a': c = '\a'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'v': c = '\v'; break;
        case '0': c = '\0'; break;
      }
    }
    out(c);
  }
}

auto unescape(const std::string &str) -> std::string {
  std::string str2;
  unescape(str, [&](char c) { str2 += c; });
  return str2;
}

auto decode_uri(const std::string &str) -> std::string {
  std::string out;
  for (size_t i = 0; i < str.size(); i++) {
    auto ch = str[i];
    if (ch == '%') {
      if (i + 2 >= str.size()) break;
      auto h = HexDecoder::c2h(str[i+1]);
      auto l = HexDecoder::c2h(str[i+2]);
      out += char((h << 4) | l);
      i += 2;
    } else {
      out += ch;
    }
  }
  return out;
}

auto encode_uri(const std::string &str) -> std::string {
  std::string out;
  for (size_t i = 0; i < str.size(); i++) {
    auto ch = str[i];
    if (('0' <= ch && ch <= '9')||
        ('a' <= ch && ch <= 'z')||
        ('A' <= ch && ch <= 'Z')||
        ('-' == ch)||
        ('_' == ch)||
        ('.' == ch)||
        ('~' == ch))
    {
      out += ch;
    } else {
      out += '%';
      out += std::toupper(HexEncoder::h2c((ch >> 4) & 0x0f));
      out += std::toupper(HexEncoder::h2c((ch >> 0) & 0x0f));
    }
  }
  return out;
}

auto encode_hex(char *out, const void *inp, int len) -> int {
  int n = 0;
  HexEncoder encoder(
    [&](char c) {
      out[n++] = c;
    }
  );
  const auto *buf = (const uint8_t *)inp;
  for (int i = 0; i < len; i++) {
    encoder.input(buf[i]);
  }
  return n;
}

auto decode_hex(void *out, const char *inp, int len) -> int {
  if (len % 2) return -1;
  auto *buf = (uint8_t *)out;
  int n = 0;
  HexDecoder decoder(
    [&](uint8_t b) {
      buf[n++] = b;
    }
  );
  for (int i = 0; i < len; i++) {
    decoder.input(inp[i]);
  }
  return n;
}

auto encode_base64(char *out, const void *inp, int len) -> int {
  int n = 0;
  Base64Encoder encoder(
    [&](char c) {
      out[n++] = c;
    }
  );
  const auto *buf = (const uint8_t *)inp;
  for (int i = 0; i < len; i++) {
    encoder.input(buf[i]);
  }
  encoder.flush();
  return n;
}

auto decode_base64(void *out, const char *inp, int len) -> int {
  if (len % 4 > 0) return -1;
  auto *buf = (uint8_t *)out;
  int n = 0;
  Base64Decoder decoder(
    [&](uint8_t b) {
      buf[n++] = b;
    }
  );
  for (int i = 0; i < len; i++) {
    int c = inp[i];
    if (!decoder.input(c)) return -1;
  }
  return decoder.complete() ? n : -1;
}

auto encode_base64url(char *out, const void *inp, int len) -> int {
  int n = 0;
  Base64UrlEncoder encoder(
    [&](char c) {
      out[n++] = c;
    }
  );
  const auto *buf = (const uint8_t *)inp;
  for (int i = 0; i < len; i++) {
    encoder.input(buf[i]);
  }
  encoder.flush();
  return n;
}

auto decode_base64url(void *out, const char *inp, int len) -> int {
  auto *buf = (uint8_t *)out;
  int n = 0;
  Base64UrlDecoder decoder(
    [&](uint8_t b) {
      buf[n++] = b;
    }
  );
  for (int i = 0; i < len; i++) {
    if (!decoder.input(inp[i])) {
      return -1;
    }
  }
  return decoder.flush() ? n : -1;
}

auto path_join(const std::string &base, const std::string &path) -> std::string {
  if (!base.empty() && base.back() == '/') {
    if (!path.empty() && path.front() == '/') {
      return base + path.substr(1);
    } else {
      return base + path;
    }
  } else {
    if (!path.empty() && path.front() == '/') {
      return base + path;
    } else {
      return base + '/' + path;
    }
  }
}

auto path_normalize(const std::string &path) -> std::string {
  std::string output;
  for (size_t i = 0, j = 0; i < path.length(); i = j + 1) {
    j = i;
    while (j < path.length() && path[j] != '/') j++;
    if (j == 0) continue;
    if (path[i] == '.') {
      if (j == 1) continue;
      if (j == 2 && path[i+1] == '.') {
        auto p = output.find_last_of('/');
        if (p == std::string::npos) output.clear();
        else output = output.substr(0, p);
        continue;
      }
    }
    output += '/';
    output += path.substr(i, j - i);
  }
  return output;
}

auto path_dirname(const std::string &path) -> std::string {
  auto i = path.rfind('/');
  if (i == std::string::npos) return "";
  return path.substr(0, i);
}

//
// HexEncoder
//

static char s_hex_tab[] = {
  "0123456789abcdef"
};

auto HexEncoder::h2c(char c) -> char {
  return s_hex_tab[c & 15];
}

void HexEncoder::input(uint8_t b) {
  m_output(h2c(b >> 4));
  m_output(h2c(b >> 0));
}

//
// HexDecoder
//

auto HexDecoder::c2h(char c) -> char {
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'f') return c - 'a' + 10;
  if ('A' <= c && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool HexDecoder::input(char c) {
  c = c2h(c);
  if (c < 0) return false;
  m_byte = (m_byte << 4) | c;
  if (++m_shift == 2) {
    m_output(m_byte);
    m_byte = 0;
    m_shift = 0;
  }
  return true;
}

//
// Base64Encoder
//

static char s_base64_tab[] = {
  "ABCDEFGHIJKLMNOP"
  "QRSTUVWXYZabcdef"
  "ghijklmnopqrstuv"
  "wxyz0123456789+/"
};

void Base64Encoder::input(uint8_t b) {
  auto triplet = m_triplet = (m_triplet << 8) | b;
  if (++m_shift == 3) {
    auto &out = m_output;
    out(s_base64_tab[(triplet >> 18) & 63]);
    out(s_base64_tab[(triplet >> 12) & 63]);
    out(s_base64_tab[(triplet >>  6) & 63]);
    out(s_base64_tab[(triplet >>  0) & 63]);
    m_shift = 0;
    m_triplet = 0;
  }
}

void Base64Encoder::flush() {
  auto triplet = m_triplet;
  auto &out = m_output;
  switch (m_shift) {
    case 1:
      triplet <<= 16;
      out(s_base64_tab[(triplet >> 18) & 63]);
      out(s_base64_tab[(triplet >> 12) & 63]);
      out('=');
      out('=');
      break;
    case 2:
      triplet <<= 8;
      out(s_base64_tab[(triplet >> 18) & 63]);
      out(s_base64_tab[(triplet >> 12) & 63]);
      out(s_base64_tab[(triplet >>  6) & 63]);
      out('=');
      break;
  }
}

//
// Base64Decoder
//

bool Base64Decoder::input(char c) {
  if (m_done) {
    if (c == '=' && m_shift == 3) {
      m_shift = 0;
      return true;
    } else {
      return false;
    }
  }
  if (c == '=') {
    m_done = true;
    if (m_shift == 3) {
      m_output((m_triplet >> 10) & 255);
      m_output((m_triplet >>  2) & 255);
      m_shift = 0;
      return true;
    } else if (m_shift == 2) {
      m_output((m_triplet >> 4) & 255);
      m_shift = 3;
      return true;
    } else {
      return false;
    }
  }
  else if (c == '+') c = 62;
  else if (c == '/') c = 63;
  else if ('0' <= c && c <= '9') c = c - '0' + 52;
  else if ('a' <= c && c <= 'z') c = c - 'a' + 26;
  else if ('A' <= c && c <= 'Z') c = c - 'A';
  else return false;
  auto triplet = m_triplet = (m_triplet << 6) | c;
  if (++m_shift == 4) {
    auto &out = m_output;
    out((triplet >> 16) & 255);
    out((triplet >>  8) & 255);
    out((triplet >>  0) & 255);
    m_shift = 0;
    m_triplet = 0;
  }
  return true;
}

//
// Base64UrlEncoder
//

static char s_base64url_tab[] = {
  "ABCDEFGHIJKLMNOP"
  "QRSTUVWXYZabcdef"
  "ghijklmnopqrstuv"
  "wxyz0123456789-_"
};

void Base64UrlEncoder::input(uint8_t b) {
  auto triplet = m_triplet = (m_triplet << 8) | b;
  if (++m_shift == 3) {
    auto &out = m_output;
    out(s_base64url_tab[(triplet >> 18) & 63]);
    out(s_base64url_tab[(triplet >> 12) & 63]);
    out(s_base64url_tab[(triplet >>  6) & 63]);
    out(s_base64url_tab[(triplet >>  0) & 63]);
    m_shift = 0;
    m_triplet = 0;
  }
}

void Base64UrlEncoder::flush() {
  auto triplet = m_triplet;
  auto &out = m_output;
  switch (m_shift) {
    case 1:
      triplet <<= 16;
      out(s_base64url_tab[(triplet >> 18) & 63]);
      out(s_base64url_tab[(triplet >> 12) & 63]);
      break;
    case 2:
      triplet <<= 8;
      out(s_base64url_tab[(triplet >> 18) & 63]);
      out(s_base64url_tab[(triplet >> 12) & 63]);
      out(s_base64url_tab[(triplet >>  6) & 63]);
      break;
  }
}

//
// Base64UrlDecoder
//

bool Base64UrlDecoder::input(char c) {
  if (m_done) return false;
  else if (c == '-') c = 62;
  else if (c == '_') c = 63;
  else if ('0' <= c && c <= '9') c = c - '0' + 52;
  else if ('a' <= c && c <= 'z') c = c - 'a' + 26;
  else if ('A' <= c && c <= 'Z') c = c - 'A';
  else return false;
  auto triplet = m_triplet = (m_triplet << 6) | c;
  if (++m_shift == 4) {
    auto &out = m_output;
    out((triplet >> 16) & 255);
    out((triplet >>  8) & 255);
    out((triplet >>  0) & 255);
    m_shift = 0;
    m_triplet = 0;
  }
  return true;
}

bool Base64UrlDecoder::flush() {
  if (m_done) return false;
  else m_done = true;
  if (m_shift == 3) {
    m_output((m_triplet >> 10) & 255);
    m_output((m_triplet >>  2) & 255);
    return true;
  } else if (m_shift == 2) {
    m_output((m_triplet >> 4) & 255);
    return true;
  } else if (m_shift == 0) {
    return true;
  } else {
    return false;
  }
}

} // namespace utils
} // namespace pipy
