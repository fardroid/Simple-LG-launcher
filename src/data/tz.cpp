#include "data/tz.h"

#include "sys/fs.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace {

struct Zone {
	std::vector<long long> at;          // transition times, seconds since 1970
	std::vector<int> type;              // the local time type from each transition on
	std::vector<long long> offset;      // each type's UT offset, seconds
	int first = 0;                      // the type before the first transition
	std::string footer;                 // TZif v2+: the POSIX TZ string for times after the last transition
};

std::mutex lock;
std::map<std::string, std::shared_ptr<Zone>> zones;
std::string dir = "/usr/share/zoneinfo";

long long be (const unsigned char *p, int n) {
	long long v = 0;
	for (int i = 0; i < n; i++) v = (v << 8) | p[i];
	if (n == 4 && (v & 0x80000000LL)) v -= 0x100000000LL;
	return v;
}

// One TZif data block (v1 with 4-byte times, or v2+ with 8-byte ones); returns its length, or 0 when broken.
size_t read_block (const unsigned char *d, size_t n, int tsize, Zone &z) {
	if (n < 44 || memcmp(d, "TZif", 4) != 0) return 0;
	long long isutcnt = be(d + 20, 4), isstdcnt = be(d + 24, 4), leapcnt = be(d + 28, 4), timecnt = be(d + 32, 4), typecnt = be(d + 36, 4), charcnt = be(d + 40, 4);
	size_t need = 44 + timecnt * tsize + timecnt + typecnt * 6 + charcnt + leapcnt * (tsize + 4) + isstdcnt + isutcnt;
	if (need > n || typecnt <= 0) return 0;
	const unsigned char *p = d + 44;
	z.at.clear(); z.type.clear(); z.offset.clear();
	for (long long i = 0; i < timecnt; i++) z.at.push_back(be(p + i * tsize, tsize));
	p += timecnt * tsize;
	for (long long i = 0; i < timecnt; i++) z.type.push_back(std::min((int)p[i], (int)typecnt - 1));
	p += timecnt;
	for (long long i = 0; i < typecnt; i++) z.offset.push_back(be(p + i * 6, 4));
	// the type in force before the first transition: the first standard-time type (RFC 8536), simply type 0 here
	z.first = 0;
	return need;
}

// The POSIX TZ string's rule for a year: "CET-1CEST,M3.5.0,M10.5.0/3" (only the "M" forms, which is what zoneinfo writes).
struct Posix { long long std_off = 0, dst_off = 0; bool dst = false; int sm, sw, sd, ss, em, ew, ed, es; };

bool parse_posix (const std::string &s, Posix &p) {
	size_t i = 0;
	auto name = [&] {
		if (i < s.size() && s[i] == '<') { while (i < s.size() && s[i] != '>') i++; i++; return; }
		while (i < s.size() && isalpha((unsigned char)s[i])) i++;
	};
	auto offset = [&] (long long &out) {
		int sign = 1;
		if (i < s.size() && (s[i] == '+' || s[i] == '-')) { if (s[i] == '-') sign = -1; i++; }
		long long h = 0, m = 0, sec = 0;
		while (i < s.size() && isdigit((unsigned char)s[i])) h = h * 10 + (s[i++] - '0');
		if (i < s.size() && s[i] == ':') { i++; while (i < s.size() && isdigit((unsigned char)s[i])) m = m * 10 + (s[i++] - '0'); }
		if (i < s.size() && s[i] == ':') { i++; while (i < s.size() && isdigit((unsigned char)s[i])) sec = sec * 10 + (s[i++] - '0'); }
		out = sign * (h * 3600 + m * 60 + sec);
	};
	auto rule = [&] (int &m, int &w, int &d, int &t) {
		if (i >= s.size() || s[i] != 'M') return false;
		i++;
		m = atoi(s.c_str() + i); while (i < s.size() && s[i] != '.') i++; i++;
		w = atoi(s.c_str() + i); while (i < s.size() && s[i] != '.') i++; i++;
		d = atoi(s.c_str() + i); while (i < s.size() && isdigit((unsigned char)s[i])) i++;
		t = 7200;
		if (i < s.size() && s[i] == '/') { i++; long long x; offset(x); t = (int)x; }
		return true;
	};
	name();
	long long o;
	offset(o);
	p.std_off = -o;                       // POSIX offsets are west-positive
	if (i >= s.size()) return true;
	name();
	p.dst_off = p.std_off + 3600;
	if (i < s.size() && s[i] != ',') { offset(o); p.dst_off = -o; }
	if (i >= s.size() || s[i] != ',') return true;
	i++;
	if (!rule(p.sm, p.sw, p.sd, p.ss)) return false;
	if (i >= s.size() || s[i] != ',') return false;
	i++;
	if (!rule(p.em, p.ew, p.ed, p.es)) return false;
	p.dst = true;
	return true;
}

long long days_from_civil (long long y, unsigned m, unsigned d) {
	y -= m <= 2;
	long long era = (y >= 0 ? y : y - 399) / 400;
	unsigned yoe = (unsigned)(y - era * 400);
	unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (long long)doe - 719468;
}

// The day of "Mm.w.d" in a year (w 5 = the last), as days since 1970.
long long rule_day (int year, int m, int w, int d) {
	long long first = days_from_civil(year, m, 1);
	int wd = (int)(((first + 4) % 7 + 7) % 7);
	long long day = first + ((d - wd) + 7) % 7 + (long long)(w - 1) * 7;
	int len = (int)(days_from_civil(m == 12 ? year + 1 : year, m == 12 ? 1 : m + 1, 1) - first);
	while (day - first >= len) day -= 7;
	return day;
}

long long posix_offset (const Posix &p, long long t) {
	if (!p.dst) return p.std_off;
	long long days = t / 86400 - (t % 86400 < 0);
	long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
	unsigned doe = (unsigned)(z - era * 146097);
	unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
	int year = (int)(yoe + era * 400) + (mp >= 10);
	// the change times are in local time: standard time for the start, summer time for the end
	long long start = rule_day(year, p.sm, p.sw, p.sd) * 86400 + p.ss - p.std_off;
	long long end = rule_day(year, p.em, p.ew, p.ed) * 86400 + p.es - p.dst_off;
	bool in = start < end ? (t >= start && t < end) : !(t >= end && t < start);
	return in ? p.dst_off : p.std_off;
}

std::shared_ptr<Zone> load (const std::string &tz) {
	// zone names are plain: letters, digits, "/", "_", "-", "+"
	if (tz.empty() || tz.find("..") != std::string::npos) return nullptr;
	for (char c : tz) if (!(isalnum((unsigned char)c) || c == '/' || c == '_' || c == '-' || c == '+')) return nullptr;
	std::string data;
	if (!read_file(join_path(dir, tz), data, 1 << 20)) return nullptr;
	auto z = std::make_shared<Zone>();
	const unsigned char *d = (const unsigned char *)data.data();
	size_t len = read_block(d, data.size(), 4, *z);
	if (!len) return nullptr;
	if (data.size() > 4 && d[4] >= '2') {
		Zone z2;
		size_t len2 = read_block(d + len, data.size() - len, 8, z2);
		if (len2) {
			z2.footer = std::string(data.data() + len + len2);
			size_t nl1 = z2.footer.find('\n'), nl2 = nl1 == std::string::npos ? std::string::npos : z2.footer.find('\n', nl1 + 1);
			z2.footer = nl1 != std::string::npos && nl2 != std::string::npos ? z2.footer.substr(nl1 + 1, nl2 - nl1 - 1) : "";
			*z = z2;
		}
	}
	return z;
}

}  // namespace

void set_zoneinfo_dir (const std::string &d) { dir = d; }

std::optional<long long> zone_offset (const std::string &tz, long long utc_ms) {
	std::shared_ptr<Zone> z;
	{
		std::lock_guard<std::mutex> l(lock);
		auto it = zones.find(tz);
		if (it == zones.end()) it = zones.emplace(tz, load(tz)).first;
		z = it->second;
	}
	if (!z) return std::nullopt;
	long long t = utc_ms / 1000 - (utc_ms % 1000 < 0);
	auto it = std::upper_bound(z->at.begin(), z->at.end(), t);
	if (it == z->at.end() && !z->footer.empty()) {
		Posix p;
		if (parse_posix(z->footer, p)) return posix_offset(p, t) * 1000;
	}
	int type = it == z->at.begin() ? z->first : z->type[it - z->at.begin() - 1];
	return z->offset[type] * 1000;
}
