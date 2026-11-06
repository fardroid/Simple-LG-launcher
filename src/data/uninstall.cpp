#include "data/uninstall.h"

#include "i18n/i18n.h"
#include "sys/luna.h"

#include <cstring>

const char *UNINSTALL_LOG = "/tmp/launcher-uninstall.log";

namespace {

bool plain_id (const std::string &id) {
	if (id.empty()) return false;
	for (unsigned char c : id) if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) return false;
	return true;
}

bool starts (const std::string &s, const char *prefix) { return s.compare(0, strlen(prefix), prefix) == 0; }

}  // namespace

Verdict uninstall_verdict (const std::string &id, const std::string &icon_path, int removable) {
	if (!plain_id(id)) return {false, tr("This app ID cannot be used", "Этот идентификатор приложения нельзя использовать")};
	if (id == "tld.my.simplelauncher" || id == "tld.my.simplelauncher.native") return {false, tr("This is the launcher itself", "Это сам лаунчер")};
	if (id == "com.webos.app.home") return {false, tr("LG Home is part of the TV", "LG Home — часть ТВ")};
	if (id == "org.webosbrew.hbchannel") return {false, tr("The launcher needs the Homebrew Channel to run commands", "Лаунчеру нужен Homebrew Channel для выполнения команд")};
	for (const char *p : {"com.webos.", "com.palm.", "com.lge."})
		if (starts(id, p)) return {false, tr("System apps cannot be removed", "Системные приложения удалять нельзя")};
	if (removable == 0) return {false, tr("The TV says this app cannot be removed", "ТВ сообщает, что это приложение нельзя удалить")};
	if (removable == 1) return {true, ""};
	// where it is installed: developer-mode apps, and apps from the LG Content Store
	if (starts(icon_path, "/media/developer/apps/") || starts(icon_path, "/media/cryptofs/apps/")) return {true, ""};
	return {false, tr("This does not look like an app you installed", "Похоже, это не установленное вами приложение")};
}

std::string uninstall_command (const std::string &id) {
	if (!plain_id(id)) return "";
	std::string present = "luna-send -n 1 luna://com.webos.applicationManager/getAppInfo \"{\\\"id\\\":\\\"$ID\\\"}\" 2>/dev/null | grep -q '\"returnValue\":true'";
	auto remove = [] (const char *service) {
		return std::string("luna-send -w 20000 -n 1 luna://com.webos.appInstallService/") + service + " \"{\\\"id\\\":\\\"$ID\\\",\\\"subscribe\\\":false}\" >> \"$LOG\" 2>&1; sleep 2";
	};
	return "ID='" + id + "'; LOG='" + UNINSTALL_LOG + "'; present () { " + present + "; }; " +
		"echo \"$(date +%H:%M:%S) uninstall $ID\" >> \"$LOG\"; " +
		"if present; then " + remove("dev/remove") + "; fi; " +
		"if present; then " + remove("remove") + "; fi; " +
		"if present; then echo still-installed; else echo removed; fi";
}

std::string parse_uninstall (const std::string &output) {
	std::string last;
	size_t at = 0;
	while (at < output.size()) {
		size_t nl = output.find('\n', at);
		std::string line = output.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
		size_t a = line.find_first_not_of(" \t\r"), b = line.find_last_not_of(" \t\r");
		if (a != std::string::npos) last = line.substr(a, b - a + 1);
		if (nl == std::string::npos) break;
		at = nl + 1;
	}
	return last == "removed" || last == "still-installed" ? last : "failed";
}

std::string uninstall_app (const std::string &id) {
	std::string cmd = uninstall_command(id);
	if (cmd.empty()) return "failed";
	bool ok = false;
	std::string out = hbc_exec(cmd, 70000, &ok);
	return ok ? parse_uninstall(out) : "failed";
}
