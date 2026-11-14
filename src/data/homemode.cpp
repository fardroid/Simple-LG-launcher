#include "data/homemode.h"

#include "i18n/i18n.h"
#include "sys/luna.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <regex>
#include <thread>

const std::vector<std::string> HOME_MODES = {"default", "mount", "helper", "none"};

namespace {
const char *WEB_ID = "tld.my.simplelauncher";
bool is_web (const std::string &id) { return id == WEB_ID; }
}  // namespace

std::optional<HomeModeInfo> parse_mode_status (const std::string &out) {
	std::smatch m;
	static const std::regex mode_re(R"((^|\n)mode=(none|helper|default|mount)(\r?\n|$))");
	if (!std::regex_search(out, m, mode_re)) return std::nullopt;
	HomeModeInfo info;
	info.mode = m[2];
	static const std::regex can_re(R"((^|\n)can_default=yes(\r?\n|$))");
	info.can_default = std::regex_search(out, can_re);
	static const std::regex rel_re(R"((^|\n)release=([^\n]*))");
	if (std::regex_search(out, m, rel_re)) {
		std::string r = m[2];
		size_t a = r.find_first_not_of(" \t\r"), b = r.find_last_not_of(" \t\r");
		info.release = a == std::string::npos ? "" : r.substr(a, b - a + 1);
	}
	return info;
}

std::optional<SwitchResult> parse_result (const std::string &out) {
	std::string s = out;
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
	size_t nl = s.rfind('\n');
	std::string line = nl == std::string::npos ? s : s.substr(nl + 1);
	std::smatch m;
	static const std::regex re(R"(^(ok|failed) (\w+)(?::\s*(.*))?$)");
	if (!std::regex_match(line, m, re)) return std::nullopt;
	return SwitchResult{m[1] == "ok", m[2], m[3]};
}

int release_major (const std::string &release) { return atoi(release.c_str()); }

ModeText mode_text (const std::string &mode) {
	if (mode == "default") return {
		tr("Home app: the TV opens the launcher", "Штатно: ТВ сам открывает лаунчер"),
		tr("The launcher becomes the TV's Home app. Switching the TV on shows what was on before: LG's start with Home is switched off.", "Лаунчер становится домашним приложением ТВ. При включении ТВ показывается то, что было до выключения: «Запуск с Home» от LG выключается."),
		{tr("Home and Back out of the last app open the launcher", "Home и выход из последнего приложения открывают лаунчер"),
			tr("LG Home is not replaced and keeps its tile", "LG Home не подменяется и остаётся на своей плитке")},
		{tr("Only where the TV lets you choose a Home app (webOS 25)", "Только где ТВ даёт выбрать домашнее приложение (webOS 25)"),
			tr("After standby the launcher takes 2–3 seconds to open", "После сна лаунчер открывается за 2–3 секунды"),
			tr("Changes two TV settings (put back when switched off)", "Меняет две настройки ТВ (при выключении возвращаются)"),
			tr("Switch this off before uninstalling the launcher", "Перед удалением лаунчера выключите этот способ")}};
	if (mode == "mount") return {
		tr("Swap: the launcher in LG Home's folder", "Подмена: лаунчер в папке LG Home"),
		tr("The TV still thinks it is opening LG Home, but the launcher is put in LG Home's folder for it to find.", "Телевизор по-прежнему думает, что открывает LG Home, но в его папку подложен лаунчер."),
		{tr("Works on TVs that cannot choose a Home app", "Работает на ТВ, где домашнее приложение выбрать нельзя"),
			tr("The TV treats the launcher exactly like its own Home", "ТВ обращается с лаунчером точно как со своим Home")},
		{tr("Puts a folder over LG Home's own (in memory only: gone after a restart)", "Накрывает папку LG Home своей (только в памяти: исчезает после перезагрузки)"),
			tr("After a full restart LG Home shows for about 40 seconds", "После полной перезагрузки около 40 секунд виден LG Home"),
			tr("LG Home itself cannot be opened while it is on", "Сам LG Home открыть нельзя, пока это включено")}};
	if (mode == "helper") return {
		tr("Helper: catches LG Home and switches", "Помощник: ловит LG Home и переключает"),
		tr("A small program on the TV notices when LG Home comes up and opens the launcher over it at once.", "Небольшая программа на ТВ замечает, что открылся LG Home, и сразу открывает поверх него лаунчер."),
		{tr("Works on almost any rooted TV", "Работает почти на любом рутованном ТВ"), tr("Nothing of LG's is replaced", "Ничего от LG не подменяется")},
		{tr("LG Home can flash for a moment", "LG Home может мелькнуть на мгновение"), tr("A program keeps running in the background", "В фоне всё время работает программа")}};
	return {
		tr("Off: Home opens LG Home", "Выключено: Home открывает LG Home"),
		tr("The launcher is an ordinary app and opens from its own tile.", "Лаунчер — обычное приложение и открывается со своей плитки."),
		{tr("Nothing runs and nothing is changed on the TV", "Ничего не работает в фоне и ничего не меняется на ТВ")},
		{tr("The Home button and waking up show LG Home", "Кнопка Home и включение ТВ показывают LG Home")}};
}

ModeNote mode_note (const std::string &mode, const std::optional<HomeModeInfo> &info) {
	if (mode == "default" && info && !info->can_default) return {true, tr("This TV does not let you choose a Home app", "Этот ТВ не даёт выбрать домашнее приложение")};
	return {false, ""};
}

std::string tools_dir (const std::string &app_id) { return "/media/developer/apps/usr/palm/applications/" + app_id + "/tools"; }
std::string status_command (const std::string &app_id) { return "sh '" + tools_dir(app_id) + "/homemode.sh' status"; }
// the helper is switched on by its own script (it copies its files to their folder after homemode.sh has switched the rest off)
std::string run_command (const std::string &app_id, const std::string &mode) {
	if (mode == "helper") return "sh '" + tools_dir(app_id) + "/helper.sh' run";
	return "sh '" + tools_dir(app_id) + "/homemode.sh' run " + mode;
}
std::string migrate_command (const std::string &app_id) { return "sh '" + tools_dir(app_id) + "/migrate.sh'; true"; }
std::string name_last_input_command (const std::string &app_id, const std::string &id) { return "sh '" + tools_dir(app_id) + "/lastinput.sh' name '" + id + "'; true"; }
std::string restore_last_input_command (const std::string &app_id) { return "sh '" + tools_dir(app_id) + "/lastinput.sh' restore; true"; }
std::string takeover_refresh_command (const std::string &app_id) { return "sh '" + tools_dir(app_id) + "/takeover.sh' refresh; true"; }
std::string helper_refresh_command (const std::string &app_id) { return "sh '" + tools_dir(app_id) + "/helper.sh' refresh; true"; }
std::string stock_command (const std::string &app_id) { return "sh '" + hook_dir(app_id) + "/stock.sh' >/dev/null 2>&1; true"; }
std::string refresh_command (const std::string &app_id) { return "sh '" + tools_dir(app_id) + "/defaulthome.sh' refresh; true"; }
// scripts/gen-tools.sh moves the state of a launcher with another id than the web version's to launcher-native-…
std::string result_file (const std::string &app_id) { return is_web(app_id) ? "/tmp/launcher-homemode.result" : "/tmp/launcher-native-homemode.result"; }
std::string hook_dir (const std::string &app_id) { return is_web(app_id) ? "/media/developer/launcher-homehook" : "/media/developer/launcher-native-homehook"; }
std::string alive_file (const std::string &app_id) { return is_web(app_id) ? "/tmp/launcher-alive" : "/tmp/launcher-native-alive"; }

std::optional<HomeModeInfo> read_home_mode (const std::string &app_id, bool *root, std::string *why) {
	bool ok = false;
	std::string out = hbc_exec(status_command(app_id), 10000, &ok, why);
	if (root) *root = ok;
	return ok ? parse_mode_status(out) : std::nullopt;
}

const std::vector<std::string> ROOT_OPTIONS = {"homeMethod", "homeCleanup", "restartTv"};

std::string root_reason (Root root, const std::string &detail) {
	if (root != Root::no) return "";
	// webOS 26: the app itself is refused the call (tools/webos26-access.sh)
	if (detail.find("Denied method call") != std::string::npos)
		return tr("webOS 26 keeps apps away from the Homebrew Channel. Once, over SSH: sh /media/developer/apps/usr/palm/applications/tld.my.simplelauncher/tools/webos26-access.sh install (see the README).",
			"webOS 26 не пускает приложения к Homebrew Channel. Один раз по SSH: sh /media/developer/apps/usr/palm/applications/tld.my.simplelauncher/tools/webos26-access.sh install (см. README).");
	// no answer, or no such service: the Homebrew Channel is not there (not rooted) or not started yet
	bool absent = detail.empty() || detail == "timeout" || detail == "call refused" || detail.find("not running") != std::string::npos ||
		detail.find("Unknown service") != std::string::npos || detail.find("does not exist") != std::string::npos || detail.find("not found") != std::string::npos;
	std::string why = absent
		? tr("the Homebrew Channel service is not running. If the TV is rooted, open the Homebrew Channel once, then open Settings again.", "служба Homebrew Channel не запущена. Если ТВ рутован, один раз откройте Homebrew Channel, затем заново откройте настройки.")
		: tr("the TV refused the command.", "ТВ отклонил команду.");
	return fmt(tr("Needs a rooted TV: {0}", "Нужен рутованный ТВ: {0}"), why) + (detail.empty() || absent ? "" : fmt(tr(" (the TV said: {0})", " (ответ ТВ: {0})"), detail));
}

SwitchResult switch_home_mode (const std::string &app_id, const std::string &mode) {
	if (std::find(HOME_MODES.begin(), HOME_MODES.end(), mode) == HOME_MODES.end()) return {false, mode, "unknown mode"};
	bool ok = false;
	std::string started = hbc_exec("rm -f '" + result_file(app_id) + "'; " + run_command(app_id, mode), 10000, &ok);
	if (!ok) return {false, mode, tr("the TV refused the command", "ТВ отклонил команду")};
	if (started.find("busy") != std::string::npos) return {false, mode, tr("another switch is still running", "ещё идёт другое переключение")};
	for (int tries = 0; tries < 60; tries++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		bool read = false;
		std::string out = hbc_exec("cat '" + result_file(app_id) + "' 2>/dev/null; true", 6000, &read);
		if (!read) continue;
		if (auto r = parse_result(out)) return *r;
	}
	return {false, mode, tr("no answer from the TV", "ТВ не ответил")};
}
