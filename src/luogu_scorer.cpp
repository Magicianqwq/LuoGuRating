// -*- coding: utf-8 -*-
// 洛谷计分器 (Luogu Scorer) - Win32 GUI 版
// 编译: g++ -O2 -municode -mwindows -static luogu_scorer.cpp -o 洛谷计分器.exe -lwininet
// 功能: 启动页(标题+开始按钮) -> 登录页(账号/密码/_client_id) -> 拉取已通过题目
#include <windows.h>
#include <commctrl.h>
#include <wininet.h>
#include <richedit.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <functional>
#include <map>
#include <ctime>

// ---------------- 全局控件 ----------------
static HINSTANCE g_hInst;
static HWND g_hWnd;
static HWND g_btnStart;          // 启动页: 开始
static HWND g_lblTitle, g_lblSub;// 启动页: 标题/副标题
static bool g_onLogin = false;    // 当前是否在登录页（卡片背景在 WM_ERASEBKGND 里画）
static HWND g_lblMode;
static HWND g_btnMode[3];         // 登录方式：三个可点击的胶囊按钮
static int  g_modeIdx = 0;        // 0 password / 1 cookie / 2 public
static const wchar_t* kModeNames[3] = { L"password", L"cookie", L"public" };
static HWND g_lblUser, g_edUser;
static HWND g_lblPwd,  g_edPwd;
static HWND g_lblCid,  g_edCid;
static HWND g_btnBack, g_btnRun;
static HWND g_lblStatus;
static HWND g_edLog;
// 启动页 -> 个人账号
static HWND g_btnAccount;          // 启动页: 个人账号
static HWND g_lblAccState;         // 启动页: 当前账号 / 我的得分
static bool g_onAccount = false;   // 是否在账号页（卡片背景同登录页）
static HWND g_lblAccTitle;
static HWND g_lblAccName, g_edAccName;
static HWND g_lblAccPwd,  g_edAccPwd;
static HWND g_lblAccUid,  g_edAccUid;
static HWND g_btnAccLogin, g_btnAccReg, g_btnAccOut, g_btnAccBack;
static HWND g_lblAccMsg;
static HWND g_btnMe;               // 启动页: 我的成绩
static HWND g_btnDaily;           // 启动页: 每日一题
static HWND g_btnPush;            // 启动页: 推分推荐
static bool g_onMe = false;        // 是否在「我的成绩」页
static std::wstring g_dailyBrief; // 最近一次「每日一题」结果，显示在启动页状态行
static std::wstring g_pushBrief;  // 最近一次「推分推荐」结果，显示在启动页状态行
static HWND g_lblMeName, g_lblMeRating, g_lblMeStat;
static HWND g_btnB50, g_btnAll, g_btnSort, g_btnMeQuery, g_btnMeBack;
static HWND g_edList;              // 题目列表（RichEdit，按难度上色）
static HWND g_btnSponsor;          // 右下角: 赞助作者（点击复制链接）
static const wchar_t* kSponsorUrl = L"https://afdian.com/a/PoYinprogramme";
static HFONT g_fTitle, g_fSub, g_fUI, g_fUIBold, g_fBigBtn, g_fMono, g_fRating;
static HBRUSH g_brBg, g_brCard, g_brEdit;
static volatile LONG g_busy = 0;
static double g_scale = 1.0;                 // DPI 缩放系数 (96dpi => 1.0)
static inline int S(int v) {
	return (int)(v * g_scale + (v >= 0 ? 0.5 : -0.5));
}
static const UINT WM_LOG = WM_APP + 1;
static const UINT WM_DONE = WM_APP + 2;
static const UINT WM_PICK = WM_APP + 3;   // 每日一题 / 推分推荐 挑好了

static const COLORREF C_BG    = RGB(30, 30, 46);
static const COLORREF C_CARD  = RGB(42, 42, 60);
static const COLORREF C_FG    = RGB(232, 232, 240);
static const COLORREF C_SUB   = RGB(154, 154, 176);
static const COLORREF C_GREEN = RGB(62, 207, 142);
static const COLORREF C_RED   = RGB(255, 107, 107);
static int g_statusKind = 1;      // 状态文字颜色：0 普通 / 1 成功 / 2 失败
static const COLORREF C_EDITBG = RGB(22, 22, 31);
static const COLORREF C_LINE  = RGB(74, 74, 96);
static const COLORREF C_EDITFG = RGB(201, 209, 217);

// ---------------- 工具函数 ----------------
static std::wstring W(const char* utf8) {
	if (!utf8 || !*utf8) return L"";
	int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	std::wstring s(n - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &s[0], n);
	return s;
}
static std::string N(const wchar_t* w) {
	if (!w || !*w) return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	std::string s(n - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
	return s;
}


static void Log(const std::wstring& msg) {
	wchar_t* p = new wchar_t[msg.size() + 1];
	wcscpy(p, msg.c_str());
	PostMessageW(g_hWnd, WM_LOG, 0, (LPARAM)p);
}
static void LogF(const wchar_t* fmt, ...) {
	wchar_t buf[2048];
	va_list ap;
	va_start(ap, fmt);
	_vsnwprintf(buf, 2048, fmt, ap);
	buf[2047] = L'\0';
	va_end(ap);
	Log(buf);
}

// ---------------- 迷你 JSON 取值 ----------------
static bool JsonFind(const std::string& s, const std::string& key, size_t& pos) {
	std::string pat = "\"" + key + "\"";
	pos = s.find(pat);
	if (pos == std::string::npos) return false;
	pos = s.find(':', pos + pat.size());
	if (pos == std::string::npos) return false;
	++pos;
	while (pos < s.size() && isspace((unsigned char)s[pos])) ++pos;
	return true;
}
static std::string JsonStr(const std::string& s, const std::string& key) {
	size_t p;
	if (!JsonFind(s, key, p)) return "";
	if (s[p] != '"') return "";
	std::string out;
	for (size_t i = p + 1; i < s.size(); ++i) {
		char c = s[i];
		if (c == '\\' && i + 1 < s.size()) {
			char e = s[++i];
			switch (e) {
				case 'n':
					out += '\n';
					break;
				case 't':
					out += '\t';
					break;
				case 'u': {
					if (i + 4 < s.size()) {
						unsigned cp = strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16);
						i += 4;
						// 简单处理: 转成 UTF-8 (BMP)
						if (cp < 0x80) out += (char)cp;
						else if (cp < 0x800) {
							out += (char)(0xC0 | (cp >> 6));
							out += (char)(0x80 | (cp & 0x3F));
						} else {
							out += (char)(0xE0 | (cp >> 12));
							out += (char)(0x80 | ((cp >> 6) & 0x3F));
							out += (char)(0x80 | (cp & 0x3F));
						}
					}
					break;
				}
				default:
					out += e;
			}
		} else if (c == '"') break;
		else out += c;
	}
	return out;
}
static long long JsonNum(const std::string& s, const std::string& key) {
	size_t p;
	if (!JsonFind(s, key, p)) return -1;
	return strtoll(s.c_str() + p, nullptr, 10);
}

// ================= 本地数据层：SHA-256 / 文件 / 配置 / 账号 =================
static size_t JsonObjectEnd(const std::string& s, size_t start);   // 定义在后面（JSON 解析段）
// ---- SHA-256（账号密码只存哈希，不落明文） ----
struct Sha256Ctx {
	unsigned int h[8];
	unsigned long long len;
	unsigned char buf[64];
	size_t n;
};
static const unsigned int K256[64] = {
	0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
	0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
	0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
	0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
	0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
	0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
	0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
	0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define ROR32(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static void Sha256Block(Sha256Ctx& c, const unsigned char* p) {
	unsigned int w[64];
	for (int i = 0; i < 16; ++i)
		w[i] = ((unsigned)p[4 * i] << 24) | ((unsigned)p[4 * i + 1] << 16) |
		       ((unsigned)p[4 * i + 2] << 8) | (unsigned)p[4 * i + 3];
	for (int i = 16; i < 64; ++i) {
		unsigned s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
		unsigned s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	unsigned a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
	unsigned e = c.h[4], f = c.h[5], g = c.h[6], hh = c.h[7];
	for (int i = 0; i < 64; ++i) {
		unsigned S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
		unsigned ch = (e & f) ^ ((~e) & g);
		unsigned t1 = hh + S1 + ch + K256[i] + w[i];
		unsigned S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
		unsigned maj = (a & b) ^ (a & cc) ^ (b & cc);
		unsigned t2 = S0 + maj;
		hh = g;
		g = f;
		f = e;
		e = d + t1;
		d = cc;
		cc = b;
		b = a;
		a = t1 + t2;
	}
	c.h[0] += a;
	c.h[1] += b;
	c.h[2] += cc;
	c.h[3] += d;
	c.h[4] += e;
	c.h[5] += f;
	c.h[6] += g;
	c.h[7] += hh;
}
static void Sha256Init(Sha256Ctx& c) {
	c.h[0] = 0x6a09e667u;
	c.h[1] = 0xbb67ae85u;
	c.h[2] = 0x3c6ef372u;
	c.h[3] = 0xa54ff53au;
	c.h[4] = 0x510e527fu;
	c.h[5] = 0x9b05688cu;
	c.h[6] = 0x1f83d9abu;
	c.h[7] = 0x5be0cd19u;
	c.len = 0;
	c.n = 0;
}
static void Sha256Update(Sha256Ctx& c, const void* data, size_t len) {
	const unsigned char* p = (const unsigned char*)data;
	c.len += len;
	while (len) {
		size_t take = 64 - c.n;
		if (take > len) take = len;
		memcpy(c.buf + c.n, p, take);
		c.n += take;
		p += take;
		len -= take;
		if (c.n == 64) {
			Sha256Block(c, c.buf);
			c.n = 0;
		}
	}
}
static std::string Sha256Hex(const std::string& data) {
	Sha256Ctx c;
	Sha256Init(c);
	Sha256Update(c, data.data(), data.size());
	unsigned long long bits = c.len * 8;
	unsigned char one = 0x80, zero = 0;
	Sha256Update(c, &one, 1);
	while (c.n != 56) Sha256Update(c, &zero, 1);
	unsigned char lenb[8];
	for (int i = 0; i < 8; ++i) lenb[i] = (unsigned char)(bits >> (56 - 8 * i));
	Sha256Update(c, lenb, 8);
	char out[65];
	for (int i = 0; i < 8; ++i) sprintf(out + i * 8, "%08x", c.h[i]);
	out[64] = 0;
	return std::string(out);
}

// ---- 文件小工具 ----
static std::wstring g_dataDirOverride;   // 用 --datadir 指定时，data 目录不再跟着 exe 走
static bool EnsureDirW(const std::wstring& path);

static std::wstring ExeDir() {
	wchar_t p[MAX_PATH] = { 0 };
	DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
	if (!n) return std::wstring();
	std::wstring s(p, n);
	size_t k = s.find_last_of(L"\\/");
	return (k == std::wstring::npos) ? std::wstring() : s.substr(0, k + 1);
}
static std::wstring DataDir() {
	if (!g_dataDirOverride.empty()) return g_dataDirOverride;
	return ExeDir() + L"data\\";
}

static bool EnsureDirW(const std::wstring& path) {
	if (path.empty()) return false;
	std::wstring cur;
	for (size_t i = 0; i < path.size(); ++i) {
		cur += path[i];
		if (path[i] == L'\\' || path[i] == L'/' || i + 1 == path.size()) {
			if (cur.size() > 3) CreateDirectoryW(cur.c_str(), nullptr);
		}
	}
	DWORD a = GetFileAttributesW(path.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static std::string ReadAllUtf8(const std::wstring& path) {
	HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
	                       OPEN_EXISTING, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE) return std::string();
	std::string out;
	char buf[8192];
	DWORD rd = 0;
	while (ReadFile(f, buf, sizeof(buf), &rd, nullptr) && rd > 0) out.append(buf, rd);
	CloseHandle(f);
	if (out.size() >= 3 && (unsigned char)out[0] == 0xEF &&
	    (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF)
		out.erase(0, 3);
	return out;
}
static bool WriteAllUtf8(const std::wstring& path, const std::string& data) {
	HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE) return false;
	const char bom[3] = { (char)0xEF, (char)0xBB, (char)0xBF };
	DWORD wr = 0;
	bool ok = WriteFile(f, bom, 3, &wr, nullptr) != 0;
	if (ok) ok = WriteFile(f, data.data(), (DWORD)data.size(), &wr, nullptr) != 0;
	CloseHandle(f);
	return ok;
}
static std::string EscJ(const std::string& s) {
	std::string o;
	for (unsigned char c : s) {
		switch (c) {
			case '"':
				o += "\\\"";
				break;
			case '\\':
				o += "\\\\";
				break;
			case '\n':
				o += "\\n";
				break;
			case '\r':
				o += "\\r";
				break;
			case '\t':
				o += "\\t";
				break;
			default:
				if (c < 0x20) {
					char t[8];
					sprintf(t, "\\u%04x", c);
					o += t;
				} else o += (char)c;
		}
	}
	return o;
}
static std::string EscJw(const std::wstring& s) {
	return EscJ(N(s.c_str()));
}

static std::wstring NowStamp() {
	SYSTEMTIME t;
	GetLocalTime(&t);
	wchar_t b[32];
	swprintf(b, L"%04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	return b;
}
static std::wstring NowFileStamp() {
	SYSTEMTIME t;
	GetLocalTime(&t);
	wchar_t b[32];
	swprintf(b, L"%04d%02d%02d_%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	return b;
}

// ---- 配置（data\config.json，第一次运行自动生成） ----
// 洛谷难度 0~7（0=暂无评定 1=红 2=橙 3=黄 4=绿 5=青 6=蓝 7=紫/黑合并档）
// 八色框架：红400 橙800 黄1200 绿1600 青2000 蓝2400 紫2800 黑3200。
// 接口的难度 7 是紫/黑合在一起返回的，这里默认按紫（2800）取值；
// 黑题（NOI/NOI+/CTSC 顶级）在 problem_rating.json 里单题写 3200+ 的分数即可生效。
static int g_ratingByDiff[9] = { 0, 400, 800, 1200, 1600, 2000, 2400, 2800, 3200 };
static int g_topN = 50;                      // 计分口径：取最好的 N 题总分，再除以 N（不足 N 题也除以 N）


static std::wstring ConfigPath() {
	return DataDir() + L"config.json";
}
static void LoadConfig() {
	EnsureDirW(DataDir());
	std::string txt = ReadAllUtf8(ConfigPath());
	if (txt.empty()) {
		std::string def =
		    "{\n"
		    "  \"topN\": 50,\n"
		    "  \"ratingByDiff\": [0, 400, 800, 1200, 1600, 2000, 2400, 2800, 3200],\n"

		    "  \"note\": \"ratingByDiff 是难度分数（0=暂无评定 1=红入门 2=橙普及- 3=黄普及/提高- 4=绿普及+/提高 5=青提高+/省选- 6=蓝省选/NOI- 7=紫NOI- 8=黑NOI/NOI+/CTSC）。八色框架：红400 橙800 黄1200 绿1600 青2000 蓝2400 紫2800 黑3200。洛谷接口的难度 7 是紫/黑合并返回的，程序默认按紫算，黑题在 problem_rating.json 里单题写 3200+ 即可。改完重启程序生效。\"\n"
		    "}\n";
		WriteAllUtf8(ConfigPath(), def);
		return;
	}
	long long n = JsonNum(txt, "topN");
	if (n > 0 && n <= 500) g_topN = (int)n;
	// 尝试过但没通过、又没在 problem_scores.json 里写明确分数的题，按这个百分比折算

	size_t a = txt.find("\"ratingByDiff\"");
	if (a != std::string::npos) {
		size_t b = txt.find('[', a);
		size_t e = (b == std::string::npos) ? std::string::npos : txt.find(']', b);
		if (b != std::string::npos && e != std::string::npos) {
			const char* p = txt.c_str() + b + 1;
			int vals[9] = { 0 }, cnt = 0;
			for (int i = 0; i < 9; ++i) {
				while (*p && (*p == ',' || isspace((unsigned char) * p))) ++p;
				if (!*p || *p == ']') break;
				char* endp = nullptr;
				long v = strtol(p, &endp, 10);
				if (endp == p) break;
				p = endp;
				if (v >= 0) vals[cnt++] = (int)v;
			}
			if (cnt == 9) {                       // 写了 9 个：0=未定 1红…8黑 严格对应
				for (int i = 0; i < 9; ++i) g_ratingByDiff[i] = vals[i];
			} else if (cnt == 7) {                // 只写了 7 个：当成「红→紫」
				for (int i = 7; i >= 1; --i) g_ratingByDiff[i] = vals[i - 1];
				g_ratingByDiff[0] = vals[0];      // 暂无评定跟入门同分
			} else if (cnt == 8) {                // 写了 8 个：0~7 严格对应（旧格式，紫/黑未拆）
				for (int i = 0; i < 8; ++i) g_ratingByDiff[i] = vals[i];
				g_ratingByDiff[8] = vals[7] + 400; // 黑 = 紫 + 400
			} else if (cnt > 0) {                 // 其它情况：按 1.. 往后填，剩下的保留默认
				for (int i = 0; i < cnt && i < 9; ++i) g_ratingByDiff[i + 1 <= 8 ? i + 1 : 8] = vals[i];
			}
		}
	}
}
static int RatingOf(int diff) {
	if (diff < 0 || diff > 8) diff = 0;
	return g_ratingByDiff[diff];
}
// ---------------- 单题分数 ----------------
// data\difficulty_scores.json（主表，洛谷所有收录题目的实际难度分）：
//   { "tierBase": [0,400,...], "scores": { "P1000": 434, "P1001": 397, ... }, ... }
//   —— 只读其中的 scores（pid → 分数），其余字段（note/tierBase/manualOverrides）忽略
// data\problem_rating.json（小的手改表，可选）：{ "P1001": 400 } —— 后读、覆盖主表
static std::map<std::wstring, int> g_problemRating;

static std::wstring DifficultyScoresPath() {
	return DataDir() + L"difficulty_scores.json";
}
static std::wstring ProblemRatingPath() {
	return DataDir() + L"problem_rating.json";
}
static std::wstring UpperW(std::wstring s) {
	for (auto& c : s) {
		if (c >= L'a' && c <= L'z') c = (wchar_t)(c - L'a' + L'A');
	}
	return s;
}
// 在 js[from, to) 区间里逐对解析 "PID": 分数，写进 g_problemRating
static void ParseScorePairs(const std::string& js, size_t from, size_t to) {
	size_t i = from;
	while (i < to && i < js.size()) {
		size_t k = js.find('"', i);
		if (k == std::string::npos || k >= to) break;
		size_t ke = js.find('"', k + 1);
		if (ke == std::string::npos || ke >= to) break;
		std::string key = js.substr(k + 1, ke - k - 1);
		i = ke + 1;
		size_t c = js.find(':', i);
		if (c == std::string::npos || c >= to) break;
		size_t j = c + 1;
		while (j < to && j < js.size() && isspace((unsigned char)js[j])) ++j;
		if (j < to && j < js.size() && (isdigit((unsigned char)js[j]) || js[j] == '-')) {
			char* endp = nullptr;
			long v = strtol(js.c_str() + j, &endp, 10);
			if (endp && endp != js.c_str() + j && v >= 0 && v <= 10000)
				g_problemRating[UpperW(W(key.c_str()))] = (int)v;
			if (endp) i = (size_t)(endp - js.c_str());
		} else {
			size_t nc = js.find(',', j);      // 值不是数字（字符串/数组/对象），跳到下一对
			i = (nc == std::string::npos || nc >= to) ? to : nc + 1;
		}
	}
}
static void LoadProblemRatings() {
	g_problemRating.clear();
	std::string big = ReadAllUtf8(DifficultyScoresPath());
	if (!big.empty()) {
		size_t p = big.find("\"scores\"");
		if (p != std::string::npos) {
			size_t b = big.find('{', p);
			if (b != std::string::npos) {
				size_t e = JsonObjectEnd(big, b);
				ParseScorePairs(big, b + 1, (e == std::string::npos) ? big.size() : e);
			}
		} else {
			ParseScorePairs(big, 0, big.size());   // 整份文件就是平铺的 pid→分数
		}
	}
	std::string own = ReadAllUtf8(ProblemRatingPath());
	if (!own.empty()) ParseScorePairs(own, 0, own.size());   // 手改的优先
}
// 单题最终分数：手改优先，否则按难度系数
static int ScoreOf(const std::string& pid, int diff) {
	if (!pid.empty()) {
		auto it = g_problemRating.find(UpperW(W(pid.c_str())));
		if (it != g_problemRating.end()) return it->second;
	}
	return RatingOf(diff);
}
// 这题的分数是不是被手改成了和难度系数不同的值
static bool ScoreOverridden(const std::string& pid, int diff) {
	return ScoreOf(pid, diff) != RatingOf(diff);
}
// 九档难度名（按用户定义：0 未评定 / 1 入门 / 2 普及- / 3 普及 / 4 普及+/提高- /
// 5 提高 / 6 提高+/省选- / 7 省选/NOI- / 8 NOI/NOI+/CTSC）
static const wchar_t* kDiffNames[9] = {
	L"未评定", L"入门", L"普及-", L"普及", L"普及+/提高-",
	L"提高", L"提高+/省选-", L"省选/NOI-", L"NOI/NOI+/CTSC"
};

// ---- 账号（data\accounts.json） ----
struct Account {
	std::wstring name, salt, hash, luoguName, created, updated;
	long long uid = -1;
};
static std::wstring AccountsPath() {
	return DataDir() + L"accounts.json";
}

static std::vector<Account> LoadAccounts() {
	std::vector<Account> out;
	std::string txt = ReadAllUtf8(AccountsPath());
	if (txt.empty()) return out;
	size_t pos = txt.find("\"accounts\"");
	if (pos == std::string::npos) return out;
	size_t b = txt.find('[', pos);
	if (b == std::string::npos) return out;
	size_t i = b + 1;
	while (i < txt.size()) {
		size_t o = txt.find_first_of("{]", i);
		if (o == std::string::npos || txt[o] == ']') break;
		size_t e = JsonObjectEnd(txt, o);
		if (e == std::string::npos) break;
		std::string obj = txt.substr(o, e - o + 1);
		Account a;
		a.name      = W(JsonStr(obj, "name").c_str());
		a.salt      = W(JsonStr(obj, "salt").c_str());
		a.hash      = W(JsonStr(obj, "hash").c_str());
		a.luoguName = W(JsonStr(obj, "luoguName").c_str());
		a.uid       = JsonNum(obj, "uid");
		a.created   = W(JsonStr(obj, "created").c_str());
		a.updated   = W(JsonStr(obj, "updated").c_str());
		if (!a.name.empty()) out.push_back(a);
		i = e + 1;
	}
	return out;
}
static bool SaveAccounts(const std::vector<Account>& list) {
	EnsureDirW(DataDir());
	std::string s = "{\n  \"accounts\": [";
	for (size_t i = 0; i < list.size(); ++i) {
		const Account& a = list[i];
		if (i) s += ",";
		s += "\n    {\"name\":\"" + EscJw(a.name) +
		     "\",\"salt\":\"" + EscJw(a.salt) +
		     "\",\"hash\":\"" + EscJw(a.hash) +
		     "\",\"uid\":" + std::to_string(a.uid) +
		     ",\"luoguName\":\"" + EscJw(a.luoguName) +
		     "\",\"created\":\"" + EscJw(a.created) +
		     "\",\"updated\":\"" + EscJw(a.updated) + "\"}";
	}
	if (!list.empty()) s += "\n  ";
	s += "]\n}\n";
	return WriteAllUtf8(AccountsPath(), s);
}
static std::wstring MakeSalt() {
	SYSTEMTIME t;
	GetLocalTime(&t);
	char seed[128];
	sprintf(seed, "%04d%02d%02d%02d%02d%02d%llu%lu", t.wYear, t.wMonth, t.wDay,
	        t.wHour, t.wMinute, t.wSecond,
	        (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentProcessId());
	std::string h = Sha256Hex(seed);
	return W(h.substr(0, 16).c_str());
}

// ---- 当前登录状态 ----
static std::wstring g_accName;         // 当前登录的账号名（空 = 未登录）
static long long    g_accUid = -1;     // 该账号绑定的洛谷 UID（-1 = 未绑定）
static std::wstring g_accLuoguName;    // 绑定的洛谷用户名

// ---- 记住上次登录的账号（data\session.json），下次启动自动登录 ----
static std::wstring SessionPath() {
	return DataDir() + L"session.json";
}
static void SaveSession() {
	if (g_accName.empty()) {
		DeleteFileW(SessionPath().c_str());
		return;
	}
	std::string js = "{\r\n  \"account\": \"" + EscJw(g_accName) + "\"\r\n}\r\n";
	WriteAllUtf8(SessionPath(), js);
}
static void ClearSession() {
	DeleteFileW(SessionPath().c_str());
}
static void LoadSession() {
	std::string js = ReadAllUtf8(SessionPath());
	if (js.empty()) return;
	std::wstring name = W(JsonStr(js, "account").c_str());
	if (name.empty()) return;
	for (const auto& a : LoadAccounts()) {
		if (a.name == name) {
			g_accName = a.name;
			g_accUid = a.uid;
			g_accLuoguName = a.luoguName;
			return;
		}
	}
	ClearSession();          // 账号已被删除
}

// ---------------- WinINet HTTP ----------------
struct NetSession {
	HINTERNET hInet = nullptr, hConn = nullptr;
	bool Open() {
		hInet = InternetOpenW(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
		                      L"AppleWebKit/537.36 (KHTML, like Gecko) "
		                      L"Chrome/124.0 Safari/537.36",
		                      INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
		if (!hInet) return false;
		DWORD to = 20000;
		InternetSetOptionW(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
		InternetSetOptionW(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
		hConn = InternetConnectW(hInet, L"www.luogu.com.cn",
		                         INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr,
		                         INTERNET_SERVICE_HTTP, 0, 0);
		return hConn != nullptr;
	}
	void Close() {
		if (hConn) InternetCloseHandle(hConn);
		if (hInet) InternetCloseHandle(hInet);
		hConn = hInet = nullptr;
	}
	std::string Get(const std::wstring& path) {
		HINTERNET hReq = HttpOpenRequestW(hConn, L"GET", path.c_str(), nullptr,
		                                  L"https://www.luogu.com.cn/", nullptr,
		                                  INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD |
		                                  INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP, 0);
		if (!hReq) return "";
		const wchar_t* hdrs =
		    L"Accept: application/json, text/plain, */*\r\n"
		    L"Accept-Language: zh-CN,zh;q=0.9\r\n";
		std::string body;
		if (HttpSendRequestW(hReq, hdrs, (DWORD) -1, nullptr, 0)) {
			DWORD status = 0, sz = sizeof(status);
			HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
			               &status, &sz, nullptr);
			char buf[8192];
			DWORD rd = 0;
			while (InternetReadFile(hReq, buf, sizeof(buf), &rd) && rd > 0)
				body.append(buf, rd);
			if (status != 200)
				LogF(L"[警告] HTTP %lu : %s", status, path.c_str());
		} else {
			LogF(L"[错误] 请求失败(%lu): %s", GetLastError(), path.c_str());
		}
		InternetCloseHandle(hReq);
		return body;
	}
	std::string Post(const std::wstring& path, const std::string& jsonBody,
	                 const std::wstring& extraHeader = L"") {
		HINTERNET hReq = HttpOpenRequestW(hConn, L"POST", path.c_str(), nullptr,
		                                  L"https://www.luogu.com.cn/", nullptr,
		                                  INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD |
		                                  INTERNET_FLAG_NO_CACHE_WRITE, 0);
		if (!hReq) return "";
		std::wstring hdrsW =
		    L"Content-Type: application/json\r\n"
		    L"Accept: application/json, text/plain, */*\r\n";
		if (!extraHeader.empty()) hdrsW += extraHeader;
		const wchar_t* hdrs = hdrsW.c_str();
		std::string body;
		if (HttpSendRequestW(hReq, hdrs, (DWORD) -1, (LPVOID)jsonBody.data(),
		                     (DWORD)jsonBody.size())) {
			char buf[8192];
			DWORD rd = 0;
			while (InternetReadFile(hReq, buf, sizeof(buf), &rd) && rd > 0)
				body.append(buf, rd);
		} else {
			LogF(L"[错误] POST 失败(%lu): %s", GetLastError(), path.c_str());
		}
		InternetCloseHandle(hReq);
		return body;
	}
	// 读取某个 cookie 的值（登录时要用 __csrf）
	std::wstring Cookie(const wchar_t* name) {
		std::vector<wchar_t> buf(8192, 0);
		DWORD sz = (DWORD)buf.size();
		if (!InternetGetCookieExW(L"https://www.luogu.com.cn/", name, buf.data(), &sz,
		                          INTERNET_COOKIE_HTTPONLY, nullptr))
			return L"";
		std::wstring v(buf.data());
		size_t eq = v.find(L'=');
		if (eq == std::wstring::npos) return L"";
		return v.substr(eq + 1);
	}
	void SetCookie(const std::wstring& name, const std::wstring& value) {
		std::wstring data = name + L"=" + value;
		InternetSetCookieW(L"https://www.luogu.com.cn/", nullptr, data.c_str());
	}
};

// ---------------- 已通过题目解析 ----------------
// 找到从 s[start]（必须是 '{'）开始的对象的结束位置（跳过字符串里的括号）
static size_t JsonObjectEnd(const std::string& s, size_t start) {
	int depth = 0;
	bool inStr = false;
	for (size_t i = start; i < s.size(); ++i) {
		char c = s[i];
		if (inStr) {
			if (c == '\\') ++i;
			else if (c == '"') inStr = false;
		} else {
			if (c == '"') inStr = true;
			else if (c == '{' || c == '[') ++depth;
			else if (c == '}' || c == ']') {
				--depth;
				if (depth == 0) return i;
			}
		}
	}
	return std::string::npos;
}

struct ProblemItem {
	std::string pid, name;
	int diff = 0;
	int pct = 100;      // 这道题实际拿到的得分百分比：通过=100，尝试过=部分分
};

// 每道题都按满分算（部分分功能已取消，洛谷公开接口拿不到每题得分）
static int PercentOfItem(const ProblemItem&) { return 100; }
static int ScoreOfItem(const ProblemItem& it) { return ScoreOf(it.pid, it.diff); }
// 这题显示成哪一档：以洛谷的难度标签（diff）为准，只有分数到了黑档起点才升级成 NOI/NOI+/CTSC。
// 这样「难度虚高 → 实际分被压低」的题不会因为分数掉档而显示成更简单的档位。
static int TierOfItem(const ProblemItem& it) {
	int d = it.diff;
	if (d < 0 || d > 8) d = 0;
	if (d < 8 && ScoreOf(it.pid, it.diff) >= g_ratingByDiff[8]) d = 8;
	return d;
}
static int TierOfScorePid(const std::string& pid, int diff, int sc) {
	int d = (diff >= 0 && diff <= 8) ? diff : 0;
	if (d < 8 && sc >= g_ratingByDiff[8]) d = 8;
	(void)pid;
	return d;
}

// 洛谷用户页把数据嵌在 <script id="lentille-context"> 的 JSON 里，
// data.passed 就是「已通过」列表：[{"type":"P","name":"A+B Problem","pid":"P1001"},...]
static bool ParseProblemList(const std::string& html, const char* key,
                              std::vector<ProblemItem>& out) {
	std::string pat = std::string("\"") + key + "\":[";
	size_t a = html.find(pat);
	if (a == std::string::npos) return false;
	size_t i = html.find('[', a);
	if (i == std::string::npos) return false;
	++i;
	while (i < html.size()) {
		size_t b = html.find_first_of("{]", i);
		if (b == std::string::npos || html[b] == ']') break;
		size_t e = JsonObjectEnd(html, b);
		if (e == std::string::npos) break;
		std::string obj = html.substr(b, e - b + 1);
		ProblemItem it;
		it.pid  = JsonStr(obj, "pid");
		it.name = JsonStr(obj, "name");
		long long df = JsonNum(obj, "difficulty");
		it.diff = (df >= 0 && df <= 7) ? (int)df : 0;
		if (it.pid.empty()) break;
		out.push_back(it);
		i = e + 1;
	}
	return !out.empty();
}
// data.passed = 已通过；data.submitted = 尝试过但没通过（只有题目信息，没有分数）
static bool ParsePassed(const std::string& html, std::vector<ProblemItem>& out) {
	return ParseProblemList(html, "passed", out);
}

// 用户名 -> UID（纯数字直接用）
static long long ResolveUid(NetSession& net, const std::wstring& user) {
	bool digits = !user.empty();
	for (wchar_t c : user) if (c < L'0' || c > L'9') {
			digits = false;
			break;
		}
	if (digits) return _wtoi64(user.c_str());

	std::string kw = N(user.c_str());
	std::string esc;
	for (char c : kw) {
		if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') esc += c;
		else {
			char t[8];
			sprintf(t, "%%%02X", (unsigned char)c);
			esc += t;
		}
	}
	std::string body = net.Get(L"/api/user/search?keyword=" + std::wstring(esc.begin(), esc.end()));
	if (body.empty()) return -1;

	std::string lowerUser = kw;
	for (char& c : lowerUser) c = (char)tolower((unsigned char)c);
	size_t pos = 0;
	long long firstUid = -1;
	while (true) {
		size_t u = body.find("\"uid\":", pos);
		if (u == std::string::npos) break;
		long long uid = strtoll(body.c_str() + u + 6, nullptr, 10);
		size_t np = body.find("\"name\":\"", u);
		std::string nm = (np == std::string::npos) ? "" : JsonStr(body.substr(np), "name");
		std::string lownm = nm;
		for (char& c : lownm) c = (char)tolower((unsigned char)c);
		if (firstUid < 0) firstUid = uid;
		if (lownm == lowerUser) return uid;
		pos = u + 6;
	}
	return firstUid;
}

// 从登录页 HTML 的 <meta name="csrf-token" content="..."> 里取 csrf 令牌
static std::string ExtractCsrfFromHtml(const std::string& html) {
	size_t k = html.find("csrf-token");
	if (k == std::string::npos) return std::string();
	size_t c = html.find("content=\"", k);
	if (c == std::string::npos) return std::string();
	c += 9;
	size_t e = html.find('"', c);
	if (e == std::string::npos) return std::string();
	return html.substr(c, e - c);
}

// ---------------- 评分与归档 ----------------
struct ScoreStats {
	int total = 0;      // 参与统计的题数（= passed + attempted）
	int passed = 0;     // 满分通过的题数
	int attempted = 0;  // 尝试过、只拿到部分分的题数
	int used = 0;       // 实际计入总分的题数（<= topN）
	long long totalScore = 0;   // 最好的 N 题总分
	int avg = 0;        // 总分 / topN（不足 topN 题也除以 topN，避免小题量占便宜）
	int best = 0, worst = 0;
	int tier[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };   // 各难度题数
};

// 按 CF 难度分从高到低排序，取最好的 topN 道总分，再除以 topN 得到 Rating
static ScoreStats ComputeStats(const std::vector<ProblemItem>& ps) {
	ScoreStats st;
	st.total = (int)ps.size();
	std::vector<int> rs;
	rs.reserve(ps.size());
	for (const auto& pr : ps) {
		int d = TierOfItem(pr);
		st.tier[d]++;
		if (PercentOfItem(pr) >= 100) st.passed++;
		else                          st.attempted++;
		rs.push_back(ScoreOfItem(pr));
	}
	std::sort(rs.begin(), rs.end(), std::greater<int>());
	int n = (int)rs.size();
	if (n > g_topN) n = g_topN;
	st.used = n;
	long long sum = 0;
	for (int i = 0; i < n; ++i) sum += rs[i];
	st.totalScore = sum;
	int div = g_topN > 0 ? g_topN : 1;      // 不足 topN 题也按 topN 除，题少的不会被平均数抬高
	st.avg = (int)((sum + div / 2) / div);
	if (!rs.empty()) {
		st.best = rs.front();
		st.worst = rs.back();
	}
	return st;
}

// 一次查询结果 -> JSON（UTF-8 带 BOM），方便以后做推荐功能
// 按 UID 反查洛谷用户名：直接填数字 UID 查询时也能记下真实昵称
static std::wstring ResolveName(NetSession& net, long long uid, const std::wstring& hint) {
	bool numeric = !hint.empty();
	for (wchar_t c : hint) if (c < L'0' || c > L'9') {
			numeric = false;
			break;
		}
	if (!hint.empty() && !numeric) return hint;
	wchar_t path[128];
	swprintf(path, L"/api/user/search?keyword=%lld", uid);
	std::string js = net.Get(path);
	size_t k = js.find("\"name\":\"");
	if (k == std::string::npos) return hint;
	k += 8;
	size_t e = js.find('"', k);
	if (e == std::string::npos || e <= k) return hint;
	return W(js.substr(k, e - k).c_str());
}

static std::string SnapshotJson(long long uid, const std::wstring& luoguName,
                                const std::wstring& accName, const std::wstring& mode,
                                const std::vector<ProblemItem>& ps, const ScoreStats& st) {
	std::string s = "{\n";
	s += "  \"uid\": " + std::to_string(uid) + ",\n";
	s += "  \"luoguName\": \"" + EscJw(luoguName) + "\",\n";
	s += "  \"account\": \"" + EscJw(accName) + "\",\n";
	s += "  \"mode\": \"" + EscJw(mode) + "\",\n";
	s += "  \"savedAt\": \"" + EscJw(NowStamp()) + "\",\n";
	s += "  \"totalPassed\": " + std::to_string(st.passed) + ",\n";
	s += "  \"totalAttempted\": " + std::to_string(st.attempted) + ",\n";
	s += "  \"topN\": " + std::to_string(g_topN) + ",\n";
	s += "  \"divisor\": " + std::to_string(g_topN) + ",\n";
	s += "  \"topScore\": " + std::to_string(st.totalScore) + ",\n";
	s += "  \"usedForAverage\": " + std::to_string(st.used) + ",\n";
	s += "  \"average\": " + std::to_string(st.avg) + ",\n";
	s += "  \"tiers\": [";
	bool first = true;
	for (int d = 0; d < 9; ++d) {
		if (!st.tier[d]) continue;
		if (!first) s += ", ";
		first = false;
		s += "{\"difficulty\": " + std::to_string(d) +
		     ", \"rating\": " + std::to_string(RatingOf(d)) +
		     ", \"count\": " + std::to_string(st.tier[d]) + "}";
	}
	s += "],\n  \"problems\": [";
	for (size_t i = 0; i < ps.size(); ++i) {
		if (i) s += ",";
		s += "\n    {\"pid\": \"" + EscJ(ps[i].pid) + "\", \"name\": \"" + EscJ(ps[i].name) +
		     "\", \"difficulty\": " + std::to_string(ps[i].diff) +
		     ", \"percent\": " + std::to_string(PercentOfItem(ps[i])) +
		     ", \"rating\": " + std::to_string(ScoreOfItem(ps[i])) + "}";
	}
	s += ps.empty() ? "]\n}\n" : "\n  ]\n}\n";
	return s;
}

static int CountJsonFiles(const std::wstring& dir) {
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW((dir + L"*.json").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return 0;
	int n = 0;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++n;
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return n;
}

// 保存这次结果: 自己的 -> data/accounts/<账号>/ ; 别人的 -> data/others/<uid>/
static bool ArchiveResult(long long uid, const std::wstring& luoguName, const std::wstring& accName,
                          const std::wstring& mode, bool isSelf,
                          const std::vector<ProblemItem>& ps, const ScoreStats& st,
                          std::wstring& relPath, int& runCount) {
	std::wstring dir = isSelf ? (DataDir() + L"accounts\\" + accName + L"\\")
	                   : (DataDir() + L"others\\" + std::to_wstring(uid) + L"\\");
	if (!EnsureDirW(dir)) return false;
	if (!EnsureDirW(dir + L"snapshots\\")) return false;
	std::string js = SnapshotJson(uid, luoguName, accName, mode, ps, st);
	std::wstring snap = dir + L"snapshots\\" + NowFileStamp() + L".json";
	bool ok = WriteAllUtf8(snap, js);
	ok = WriteAllUtf8(dir + L"latest.json", js) && ok;
	relPath = isSelf ? (L"data\\accounts\\" + accName + L"\\latest.json")
	          : (L"data\\others\\" + std::to_wstring(uid) + L"\\latest.json");
	runCount = CountJsonFiles(dir + L"snapshots\\");
	return ok;
}

// ---------------- 工作线程 ----------------
struct JobArgs {
	std::wstring mode, user, pwd, cid;
	std::wstring account;       // 当前登录的本地账号（空 = 未登录）
	long long    selfUid = -1;  // 该账号绑定的洛谷 UID
};

static const wchar_t* DiffShort(int d);   // 定义在下面，Worker 里先用

static DWORD WINAPI Worker(LPVOID lp) {
	JobArgs* job = (JobArgs*)lp;
	LogF(L"[信息] 开始查询（模式：%s）…", job->mode.c_str());
	NetSession net;
	if (!net.Open()) {
		Log(L"[错误] 网络初始化失败");
		PostMessageW(g_hWnd, WM_DONE, 0, 0);
		delete job;
		return 0;
	}

	long long uid = -1;
	if (job->mode == L"public") {
		Log(L"[信息] 公开查询模式：先按用户名找到 UID…");
		uid = ResolveUid(net, job->user);
		if (uid <= 0) {
			Log(L"[失败] 没有找到这个用户，请检查用户名 / UID 是否正确。");
		} else {
			LogF(L"[信息] 已定位到 UID: %lld", uid);
		}
	} else if (job->mode == L"password") {
		Log(L"[信息] 使用账号密码登录…");
		if (!job->cid.empty()) net.SetCookie(L"_client_id", job->cid);
		std::string loginHtml = net.Get(L"/auth/login");   // 先取登录页，拿 csrf 令牌
		std::wstring csrf = net.Cookie(L"__csrf");
		if (csrf.empty()) {
			std::string c = ExtractCsrfFromHtml(loginHtml);
			if (!c.empty()) {
				csrf = W(c.c_str());
				Log(L"[信息] 已从登录页取得 csrf-token");
			}
		}
		if (csrf.empty()) Log(L"[警告] 没有取到 csrf-token，登录可能被拒绝");
		std::string payload = "{\"username\":\"" + N(job->user.c_str()) +
		                      "\",\"password\":\"" + N(job->pwd.c_str()) +
		                      "\",\"captcha\":\"\"}";
		std::wstring hdr = csrf.empty() ? L"" : (L"x-csrf-token: " + csrf + L"\r\n");
		std::string resp = net.Post(L"/auth/login", payload, hdr);
		std::wstring uidCookie = net.Cookie(L"_uid");
		if (!uidCookie.empty()) uid = _wtoi64(uidCookie.c_str());
		if (uid > 0) {
			LogF(L"[成功] 登录成功（UID: %lld）", uid);
		} else {
			std::string msg = JsonStr(resp, "message");
			if (msg.empty()) msg = "用户名或密码错误，或触发了人机验证/验证码。";
			LogF(L"[失败] 登录未通过：%s", W(msg.c_str()).c_str());
			if (!resp.empty())
				LogF(L"[调试] 服务器返回 %s", W(resp.substr(0, 200).c_str()).c_str());
			Log(L"[提示] 把「登录方式」改成 public，可以无需密码直接查已通过题目。");
			Log(L"[提示] 也可以在浏览器 F12 里复制 _uid / _client_id，用 cookie 方式登录。");
		}
	} else {
		net.SetCookie(L"_uid", job->user);
		net.SetCookie(L"_client_id", job->pwd);
		if (!job->cid.empty()) net.SetCookie(L"__csrf", job->cid);
		Log(L"[信息] 已注入 Cookie 登录态（_uid / _client_id）");
		uid = _wtoi64(job->user.c_str());
		if (uid <= 0) uid = ResolveUid(net, job->user);
		if (uid > 0) LogF(L"[信息] 使用用户编号: %lld", uid);
	}

	if (uid > 0) {
		wchar_t path[128];
		swprintf(path, L"/user/%lld/practice", uid);
		std::string html = net.Get(path);
		std::vector<ProblemItem> problems;
		if (!ParsePassed(html, problems)) {
			// 可能是被风控拦了一次，重试一遍
			html = net.Get(path);
			ParsePassed(html, problems);
		}
		if (!problems.empty()) {
			LogF(L"[信息] 已通过题目 %zu 道：", problems.size());
			for (size_t i = 0; i < problems.size(); ++i) {
				const ProblemItem& pr = problems[i];
				LogF(L"  %s  %s   https://www.luogu.com.cn/problem/%s",
				     W(pr.pid.c_str()).c_str(),
				     W(pr.name.c_str()).c_str(),
				     W(pr.pid.c_str()).c_str());
			}
			ScoreStats st = ComputeStats(problems);
			LogF(L"[统计] 已通过 %d 道，取最好的 %d 道总分 %lld ÷ %d = %d 分（最高 %d / 最低 %d）",
			     st.passed, st.used, st.totalScore, g_topN, st.avg, st.best, st.worst);
			std::wstring dist;
			for (int d = 0; d < 9; ++d) {
				if (!st.tier[d]) continue;
				wchar_t b[80];
				swprintf(b, L"%s ×%d   ", kDiffNames[d], st.tier[d]);
				dist += b;
			}
			if (!dist.empty()) LogF(L"[统计] 难度分布：%s", dist.c_str());

			std::wstring luoName = ResolveName(net, uid, job->user);
			if (!luoName.empty() && luoName != job->user)
				LogF(L"[信息] 洛谷昵称：%s", luoName.c_str());
			bool isSelf = (!job->account.empty() && job->selfUid > 0 && uid == job->selfUid);
			std::wstring rel;
			int runs = 0;
			if (ArchiveResult(uid, luoName.empty() ? job->user : luoName,
			                  job->account, job->mode, isSelf, problems, st, rel, runs)) {
				if (isSelf) {
					LogF(L"[归档] 已计入你的账号「%s」：%s（第 %d 次记录）",
					     job->account.c_str(), rel.c_str(), runs);
				} else if (job->account.empty()) {
					LogF(L"[归档] 未登录，这是外部成绩，单独保存到：%s（不计入任何账号）", rel.c_str());
				} else {
					LogF(L"[归档] 这是别人的成绩，单独保存到：%s（不计入你的成绩）", rel.c_str());
				}
			} else {
				Log(L"[警告] 成绩保存失败：data 目录可能不可写。");
			}
			LogF(L"[完成] 共 %zu 道已通过题目，Rating %d 分（最好 %d 道总分 ÷ %d）。",
			     problems.size(), st.avg, st.used, g_topN);
		} else {
			Log(L"[完成] 没有取到「已通过」列表（可能是网络受限、需要登录，或该用户没有公开记录）。");
		}
	} else if (job->mode != L"public") {
		Log(L"[失败] 未获取到用户编号，登录态可能无效。");
	}


	net.Close();
	PostMessageW(g_hWnd, WM_DONE, 0, 0);
	delete job;
	return 0;
}

// ---------------- 我的成绩页：颜色 / 数据 / 列表 ----------------
static int  g_meView = 0;              // 0 未选 / 1 最佳 g_topN 题 / 2 全部已通过
static int  g_meSort = 0;              // 0 按难度分组 / 1 按分数 / 2 按题号
static std::vector<ProblemItem> g_myProblems;
static std::wstring g_myLuoguName;

static COLORREF DiffColor(int d) {
	switch (d) {
		case 1:
			return RGB(255, 92, 92);     // 红 入门
		case 2:
			return RGB(255, 158, 66);    // 橙 普及-
		case 3:
			return RGB(255, 202, 56);    // 黄 普及/提高-
		case 4:
			return RGB(102, 214, 82);    // 绿 普及+/提高
		case 5:
			return RGB(46, 218, 214);    // 青 提高+/省选-
		case 6:
			return RGB(92, 170, 255);    // 蓝 省选/NOI-
		case 7:
			return RGB(198, 140, 255);   // 紫 NOI/CTSC（提亮，深底上不糊）
		default:
			return C_SUB;               // 暂无评定
	}
}
// 列表标签用：洛谷官方难度档名（[] 和里面的内容一起上色）
static const wchar_t* DiffShort(int d) {
	static const wchar_t* k[9] = { L"未评定", L"入门", L"普及-", L"普及", L"普及+/提高-",
	                               L"提高", L"提高+/省选-", L"省选/NOI-", L"NOI/NOI+/CTSC"
	                             };
	return (d >= 0 && d <= 8) ? k[d] : k[0];
}
// 统计行用：短名，省得一行写不下
static const wchar_t* DiffTierShort(int d) {
	static const wchar_t* k[9] = { L"未评定", L"入门", L"普及-", L"普及", L"普及+",
	                               L"提高", L"提高+", L"省选", L"NOI"
	                             };
	return (d >= 0 && d <= 8) ? k[d] : k[0];
}

// 解析归档文件里的 "problems":[...]
static void ParseSnapshotProblems(const std::string& js, std::vector<ProblemItem>& out) {
	out.clear();
	size_t p = js.find("\"problems\"");
	if (p == std::string::npos) return;
	size_t lb = js.find('[', p);
	if (lb == std::string::npos) return;
	size_t i = lb + 1;
	while (i < js.size()) {
		size_t o = js.find('{', i);
		if (o == std::string::npos) break;
		size_t e = JsonObjectEnd(js, o);
		if (e <= o || e >= js.size()) break;
		std::string obj = js.substr(o, e - o + 1);
		ProblemItem it;
		it.pid = JsonStr(obj, "pid");
		it.name = JsonStr(obj, "name");
		long long d = JsonNum(obj, "difficulty");
		it.diff = (d >= 0 && d <= 8) ? (int)d : 0;
		// 早期版本把「尝试过但没通过」的题也存进了快照（percent < 100），
		// 部分分功能已经取消，这里直接跳过它们，免得旧数据被算成已通过。
		long long pc = JsonNum(obj, "percent");
		if (pc >= 0 && pc < 100) { i = e + 1; continue; }
		if (!it.pid.empty()) out.push_back(it);
		i = e + 1;
	}
}

// 从 data\accounts\<账号>\latest.json 读回上次归档的题目
static void LoadMyProblems() {
	g_myProblems.clear();
	g_myLuoguName.clear();
	if (g_accName.empty()) return;
	std::string js = ReadAllUtf8(DataDir() + L"accounts\\" + g_accName + L"\\latest.json");
	if (js.empty()) return;
	std::wstring ln = W(JsonStr(js, "luoguName").c_str());
	if (!ln.empty()) g_myLuoguName = ln;
	ParseSnapshotProblems(js, g_myProblems);
}

static std::wstring PadR(const std::wstring& s, size_t n) {
	std::wstring t = s;
	while (t.size() < n) t += L' ';
	return t;
}

// 按「区分方式」把题目写进 RichEdit，难度用洛谷原色
// 做法：先拼成整段文本 + 记录每段颜色，一次性 WM_SETTEXT，再按区间上色（避免空控件首段继承默认字号）
struct RichRange {
	int start;
	int len;
	COLORREF color;
	bool bold = false;
};

// RichEdit 内部一个段落只占 1 个字符位，但 WM_GETTEXTLENGTH 按 \r\n 计数；
// 这里先量一下真实长度，决定偏移量要不要每个换行减 1。
static void RichApplyRanges(HWND re, const std::vector<RichRange>& rs, const std::wstring& text) {
	int breaks = 0;
	for (wchar_t c : text) if (c == L'\r') ++breaks;
	int ctrl = GetWindowTextLengthW(re);
	bool crOnly = (ctrl != (int)text.size());       // 控制里按 1 个字符存换行
	auto conv = [&](int off) {
		if (!crOnly) return off;
		int b = 0;
		for (int i = 0; i < off && i < (int)text.size(); ++i) if (text[i] == L'\r') ++b;
		return off - b;
	};
	for (const auto& r : rs) {
		if (r.len <= 0) continue;
		int a = conv(r.start), b = conv(r.start + r.len);
		CHARRANGE cr = { a, b };
		SendMessageW(re, EM_EXSETSEL, 0, (LPARAM)&cr);
		CHARFORMAT2W cf = {};
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_COLOR | CFM_SIZE | CFM_FACE | CFM_BOLD;
		cf.crTextColor = r.color;
		cf.dwEffects = CFE_BOLD;          // 列表统一加粗（r.bold 只用于区分层级，不再切细体）
		cf.yHeight = (LONG)S(16) * 10;
		wcscpy_s(cf.szFaceName, L"Microsoft YaHei UI");
		SendMessageW(re, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	}
}

static void FillList() {
	if (!g_edList) return;
	if (g_myProblems.empty()) {
		SetWindowTextW(g_edList, L"还没有数据。先点「开始」查一次自己的洛谷账号，成绩会自动归档到这里。\r\n");
		return;
	}
	std::vector<ProblemItem> v = g_myProblems;
	std::sort(v.begin(), v.end(), [](const ProblemItem & a, const ProblemItem & b) {
		int ra = ScoreOfItem(a), rb = ScoreOfItem(b);
		if (ra != rb) return ra > rb;
		return a.pid < b.pid;
	});
	if (g_meView == 1 && (int)v.size() > g_topN) v.resize(g_topN);

	std::wstring text;
	std::vector<RichRange> rs;
	auto add = [&](const std::wstring & s, COLORREF c, bool bold = false) {
		rs.push_back(RichRange{ (int)text.size(), (int)s.size(), c, bold });
		text += s;
	};
	auto addLine = [&](const std::wstring & s, COLORREF c) {
		add(s + L"\r\n", c);
	};

	{
		// 列表抬头：当前看的是哪一组
		int total = (int)g_myProblems.size();
		int nPass = 0;
		for (const auto& it : g_myProblems) if (PercentOfItem(it) >= 100) nPass++;
		(void)nPass;
		std::wstring head = (g_meView == 1)
		                    ? (L"B50 · 已通过 " + std::to_wstring(total) + L" 道"
		                       + ((int)v.size() < g_topN
		                          ? (L"（不满 " + std::to_wstring(g_topN) + L" 道，按总分 ÷ "
		                             + std::to_wstring(g_topN) + L" 计）")
		                          : (L" · 取最好的 " + std::to_wstring((int)v.size()) + L" 道")))
		                    : (L"总览 · 全部 " + std::to_wstring(total) + L" 道已通过");
		addLine(head, C_SUB);
		addLine(L"", C_SUB);
	}

	auto emit = [&](const ProblemItem & it) {
		int base = ScoreOf(it.pid, it.diff);
		int pct  = PercentOfItem(it);
		std::wstring tag = L"[" + std::wstring(DiffShort(TierOfItem(it))) + L" " + std::to_wstring(base);
		if (pct < 100) tag += L" · " + std::to_wstring(pct) + L"%";
		tag += L"]";
		add(tag, C_EDITFG, true);
		{
			std::wstring padded = PadR(W(it.pid.c_str()), 8);
			addLine(L"  " + padded + L"  " + W(it.name.c_str()), C_EDITFG);
		}
	};

	if (g_meSort == 0) {                                    // 按难度档分组（档内按分数从高到低）
		for (int d = 8; d >= 0; --d) {
			std::vector<ProblemItem> grp;
			for (const auto& it : v) {
				if (TierOfItem(it) == d) grp.push_back(it);
			}
			if (grp.empty()) continue;
			std::sort(grp.begin(), grp.end(), [](const ProblemItem & a, const ProblemItem & b) {
				int ra = ScoreOfItem(a), rb = ScoreOfItem(b);
				if (ra != rb) return ra > rb;
				return a.pid < b.pid;
			});
			int sum = 0;
			for (const auto& it : grp) sum += ScoreOfItem(it);
			int avg = (int)((sum + (int)grp.size() / 2) / (int)grp.size());
			std::wstring head = L"── " + std::wstring(DiffShort(d)) + L" · 共 "
			                    + std::to_wstring(grp.size()) + L" 题 · 平均 "
			                    + std::to_wstring(avg) + L" 分 ──";
			addLine(head, C_SUB);
			for (const auto& it : grp) emit(it);
			addLine(L"", C_EDITFG);
		}
	} else {
		std::vector<ProblemItem> w = v;
		if (g_meSort == 2)
			std::sort(w.begin(), w.end(), [](const ProblemItem & a, const ProblemItem & b) {
			return a.pid < b.pid;
		});
		for (const auto& it : w) emit(it);
	}

	SetWindowTextW(g_edList, text.c_str());
	RichApplyRanges(g_edList, rs, text);
	SendMessageW(g_edList, EM_SETSEL, 0, 0);
	SendMessageW(g_edList, WM_VSCROLL, SB_TOP, 0);
	InvalidateRect(g_edList, nullptr, TRUE);
	UpdateWindow(g_edList);
}

static void HideMeCtl() {
	g_onMe = false;
	for (HWND h : {
	         g_lblMeName, g_lblMeRating, g_lblMeStat, g_btnB50, g_btnAll,
	         g_btnSort, g_btnMeQuery, g_btnMeBack, g_edList
	     })
		if (h) ShowWindow(h, SW_HIDE);
}

// ---------------- 页面切换 ----------------
static void UpdateModeLabels();
static std::wstring ReadField(HWND ed);
static void HideLoginCtl();
static void HideAccountCtl();
static void RefreshAccountLabel();
static void ShowAccount();

static void ShowStart() {
	ShowWindow(g_lblTitle, SW_SHOW);
	ShowWindow(g_lblSub, SW_SHOW);
	ShowWindow(g_btnStart, SW_SHOW);
	ShowWindow(g_btnAccount, SW_SHOW);
	ShowWindow(g_btnMe, SW_SHOW);
	ShowWindow(g_btnDaily, SW_SHOW);
	ShowWindow(g_btnPush, SW_SHOW);
	ShowWindow(g_lblAccState, SW_SHOW);
	g_onLogin = false;
	g_onAccount = false;
	HideLoginCtl();
	HideAccountCtl();
	HideMeCtl();
	RefreshAccountLabel();
	InvalidateRect(g_hWnd, nullptr, TRUE);
}

static void ShowLogin() {
	ShowWindow(g_lblTitle, SW_HIDE);
	ShowWindow(g_lblSub, SW_HIDE);
	ShowWindow(g_btnStart, SW_HIDE);
	ShowWindow(g_btnAccount, SW_HIDE);
	ShowWindow(g_btnMe, SW_HIDE);
	ShowWindow(g_btnDaily, SW_HIDE);
	ShowWindow(g_btnPush, SW_HIDE);
	ShowWindow(g_lblAccState, SW_HIDE);
	HideAccountCtl();
	HideMeCtl();
	g_onAccount = false;
	g_onLogin = true;
	ShowWindow(g_lblMode, SW_SHOW);
	for (HWND b : g_btnMode) ShowWindow(b, SW_SHOW);
	ShowWindow(g_lblUser, SW_SHOW);
	ShowWindow(g_edUser, SW_SHOW);
	ShowWindow(g_lblPwd, SW_SHOW);
	ShowWindow(g_edPwd, SW_SHOW);
	ShowWindow(g_lblCid, SW_SHOW);
	ShowWindow(g_edCid, SW_SHOW);
	ShowWindow(g_btnBack, SW_SHOW);
	ShowWindow(g_btnRun, SW_SHOW);
	ShowWindow(g_lblStatus, SW_SHOW);
	ShowWindow(g_edLog, SW_SHOW);
	InvalidateRect(g_hWnd, nullptr, TRUE);
	UpdateModeLabels();
	// 已登录且绑定过洛谷号：public 模式下直接把号填好，省得每次手打
	if (ReadField(g_edUser).empty() && !g_accName.empty()) {
		if (g_accUid > 0) SetWindowTextW(g_edUser, std::to_wstring(g_accUid).c_str());
		else if (!g_accLuoguName.empty()) SetWindowTextW(g_edUser, g_accLuoguName.c_str());
	}
	SetFocus(g_edUser);           // 进入登录页就把光标放进第一个输入框，可直接打字
}

// 给输入框留出左右内边距（单行 EDIT 的垂直居中由系统负责）
static void EditMargins(HWND ed) {
	SendMessageW(ed, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
	             MAKELONG(S(10), S(8)));
}

// 让单行输入框里的文字垂直居中，并留出左右内边距
static void CenterEdit(HWND ed) {
	HFONT f = (HFONT)SendMessageW(ed, WM_GETFONT, 0, 0);
	HDC dc = GetDC(ed);
	HFONT old = f ? (HFONT)SelectObject(dc, f) : nullptr;
	TEXTMETRICW tm = {};
	GetTextMetricsW(dc, &tm);
	if (old) SelectObject(dc, old);
	ReleaseDC(ed, dc);

	RECT rc;
	GetClientRect(ed, &rc);
	int lh  = tm.tmHeight;
	int top = (rc.bottom - lh) / 2;
	if (top < 0) top = 0;
	RECT fmt = { 0, top, rc.right, top + lh + S(2) };
	SendMessageW(ed, EM_SETRECT, 0, (LPARAM)&fmt);
	SendMessageW(ed, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
	             MAKELONG(S(10), S(8)));
}

// 当前登录方式名字
static std::wstring CurrentMode() {
	if (g_modeIdx < 0 || g_modeIdx > 2) g_modeIdx = 0;
	return kModeNames[g_modeIdx];
}

// 读取输入框内容（去掉换行与尾部空格）
static std::wstring ReadField(HWND ed) {
	int n = GetWindowTextLengthW(ed);
	if (n <= 0) return std::wstring();
	std::vector<wchar_t> buf(n + 1);
	GetWindowTextW(ed, buf.data(), n + 1);
	std::wstring out;
	for (int i = 0; i < n; ++i) {
		wchar_t c = buf[(size_t)i];
		if (c != L'\r' && c != L'\n') out.push_back(c);
	}
	while (!out.empty() && out.back() == L' ') out.pop_back();
	return out;
}

// ---------------- 个人账号页 ----------------
static void Layout(int W_, int H_);
static void HideLoginCtl() {
	ShowWindow(g_lblMode, SW_HIDE);
	for (HWND b : g_btnMode) ShowWindow(b, SW_HIDE);
	ShowWindow(g_lblUser, SW_HIDE);
	ShowWindow(g_edUser, SW_HIDE);
	ShowWindow(g_lblPwd, SW_HIDE);
	ShowWindow(g_edPwd, SW_HIDE);
	ShowWindow(g_lblCid, SW_HIDE);
	ShowWindow(g_edCid, SW_HIDE);
	ShowWindow(g_btnBack, SW_HIDE);
	ShowWindow(g_btnRun, SW_HIDE);
	ShowWindow(g_lblStatus, SW_HIDE);
	ShowWindow(g_edLog, SW_HIDE);
}
static void HideAccountCtl() {
	g_onAccount = false;
	for (HWND h : {
	         g_lblAccTitle, g_lblAccName, g_edAccName, g_lblAccPwd, g_edAccPwd,
	         g_lblAccUid, g_edAccUid, g_btnAccLogin, g_btnAccReg, g_btnAccOut,
	         g_btnAccBack, g_lblAccMsg
	     })
		ShowWindow(h, SW_HIDE);
}

// 启动页上的账号状态行（未登录 / 账号 + 最佳 N 题平均分）
static void RefreshAccountLabel() {
	if (!g_lblAccState) return;
	LoadProblemRatings();
	if (g_accName.empty()) {
		std::wstring s0 = L"未登录 · 点「个人账号」注册/登录后，自己的成绩才会归档";
		if (!g_dailyBrief.empty()) s0 += L"\r\n" + g_dailyBrief;
		if (!g_pushBrief.empty()) s0 += L"\r\n" + g_pushBrief;
		SetWindowTextW(g_lblAccState, s0.c_str());
		return;
	}
	std::wstring t = L"账号：" + g_accName;
	if (g_accUid > 0) t += L"   洛谷 UID：" + std::to_wstring(g_accUid);
	std::string latest = ReadAllUtf8(DataDir() + L"accounts\\" + g_accName + L"\\latest.json");
	long long avg = latest.empty() ? -1 : JsonNum(latest, "average");
	long long num = latest.empty() ? -1 : JsonNum(latest, "totalPassed");
	long long partial = 0;
	long long usedN = 0, topScore = -1;
	{
		// 用当前难度分数表重新算一遍，改了 config.json 不用重新查询也能看到新分数
		std::vector<ProblemItem> ps;
		ParseSnapshotProblems(latest, ps);
		if (!ps.empty()) {
			ScoreStats st = ComputeStats(ps);
			avg = st.avg;
			num = st.passed;
			partial = st.attempted;
			usedN = st.used;
			topScore = st.totalScore;
		}
	}
	if (avg >= 0) {
		// 第一行放 Rating，第二行放计分口径，避免字体加粗后一行放不下被硬折行
		t += L"   Rating " + std::to_wstring(avg);
		if (num > 0) {
			t += L"（已通过 " + std::to_wstring(num) + L" 题";
			if (partial > 0) t += L" · 部分分 " + std::to_wstring(partial) + L" 题";
			t += L"）";
		}
		t += L"\r\n计分：最好 " + std::to_wstring(usedN > 0 ? usedN : g_topN) + L" 题总分";
		if (topScore >= 0) t += L" " + std::to_wstring(topScore);
		t += L" ÷ " + std::to_wstring(g_topN);
	} else {
		t += L"   （还没有归档成绩，去查一次自己的洛谷账号）";
	}
	if (!g_dailyBrief.empty()) t += L"\r\n" + g_dailyBrief;
	if (!g_pushBrief.empty()) t += L"\r\n" + g_pushBrief;
	SetWindowTextW(g_lblAccState, t.c_str());
}

// 把「用户名 或 UID」变成 UID：纯数字直接用，否则联网查一次
static bool ResolveLuoguUid(const std::wstring& text, long long& uid, std::wstring& shownName) {
	bool digits = !text.empty();
	for (wchar_t c : text) if (c < L'0' || c > L'9') {
			digits = false;
			break;
		}
	if (digits) {
		uid = _wtoi64(text.c_str());
		shownName = text;
		return uid > 0;
	}
	HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
	NetSession net;
	if (net.Open()) {
		uid = ResolveUid(net, text);
		net.Close();
	}
	SetCursor(old);
	if (uid > 0) {
		shownName = text;
		return true;
	}
	return false;
}

static void OnAccountRegister() {
	std::wstring name = ReadField(g_edAccName);
	std::wstring pwd  = ReadField(g_edAccPwd);
	std::wstring luo  = ReadField(g_edAccUid);
	if (name.empty()) {
		SetWindowTextW(g_lblAccMsg, L"请先填一个账号名（随便起，不是洛谷账号）。");
		return;
	}
	if (pwd.size() < 4) {
		SetWindowTextW(g_lblAccMsg, L"密码至少 4 位。");
		return;
	}
	for (wchar_t c : name) {
		if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' ||
		    c == L'"' || c == L'<' || c == L'>' || c == L'|') {
			SetWindowTextW(g_lblAccMsg, L"账号名不能包含 \\ / : * ? \" < > | 这些字符。");
			return;
		}
	}
	std::vector<Account> list = LoadAccounts();
	for (const auto& a : list) {
		if (a.name == name) {
			SetWindowTextW(g_lblAccMsg, L"这个账号名已经被用了，换一个。");
			return;
		}
	}
	long long uid = -1;
	std::wstring luoName;
	if (!luo.empty() && !ResolveLuoguUid(luo, uid, luoName)) {
		SetWindowTextW(g_lblAccMsg, L"没能解析这个洛谷用户，请直接填数字 UID。");
		return;
	}
	Account a;
	a.name = name;
	a.salt = MakeSalt();
	a.hash = W(Sha256Hex(N((a.salt + L":" + pwd).c_str())).c_str());
	a.uid = uid;
	a.luoguName = luoName;
	a.created = a.updated = NowStamp();
	list.push_back(a);
	if (!SaveAccounts(list)) {
		SetWindowTextW(g_lblAccMsg, L"写入 data\\accounts.json 失败，检查目录权限。");
		return;
	}
	g_accName = a.name;
	g_accUid = a.uid;
	g_accLuoguName = a.luoguName;
	SaveSession();                       // 记住这个账号：下次启动自动登录
	SetWindowTextW(g_lblAccMsg, L"注册成功，已自动登录。");
	RefreshAccountLabel();
	ShowStart();
}

static void OnAccountLogin() {
	std::wstring name = ReadField(g_edAccName);
	std::wstring pwd  = ReadField(g_edAccPwd);
	if (name.empty() || pwd.empty()) {
		SetWindowTextW(g_lblAccMsg, L"请填写账号名和密码。");
		return;
	}
	std::vector<Account> list = LoadAccounts();
	for (const auto& a : list) {
		if (a.name != name) continue;
		std::string h = Sha256Hex(N((a.salt + L":" + pwd).c_str()));
		if (W(h.c_str()) != a.hash) {
			SetWindowTextW(g_lblAccMsg, L"密码不对。");
			return;
		}
		g_accName = a.name;
		g_accUid = a.uid;
		g_accLuoguName = a.luoguName;
		SaveSession();                   // 记住这个账号：下次启动自动登录
		SetWindowTextW(g_lblAccMsg, L"登录成功。");
		RefreshAccountLabel();
		ShowStart();
		return;
	}
	SetWindowTextW(g_lblAccMsg, L"没有这个账号，先点「注册」。");
}

static void OnAccountLogout() {
	g_accName.clear();
	g_accUid = -1;
	g_accLuoguName.clear();
	ClearSession();
	g_myProblems.clear();
	g_myLuoguName.clear();
	SetWindowTextW(g_edAccPwd, L"");
	SetWindowTextW(g_lblAccMsg, L"已退出登录。之后查询的结果都会存到 data\\others\\。");
	RefreshAccountLabel();
}

static void ShowAccount() {
	g_onLogin = false;
	g_onAccount = true;
	ShowWindow(g_lblTitle, SW_HIDE);
	ShowWindow(g_lblSub, SW_HIDE);
	ShowWindow(g_btnStart, SW_HIDE);
	ShowWindow(g_btnAccount, SW_HIDE);
	ShowWindow(g_btnMe, SW_HIDE);
	ShowWindow(g_btnDaily, SW_HIDE);
	ShowWindow(g_btnPush, SW_HIDE);
	ShowWindow(g_lblAccState, SW_HIDE);
	HideLoginCtl();
	for (HWND h : {
	         g_lblAccTitle, g_lblAccName, g_edAccName, g_lblAccPwd, g_edAccPwd,
	         g_lblAccUid, g_edAccUid, g_btnAccLogin, g_btnAccReg, g_btnAccOut,
	         g_btnAccBack, g_lblAccMsg
	     })
		ShowWindow(h, SW_SHOW);
	SetWindowTextW(g_edAccName, g_accName.c_str());
	SetWindowTextW(g_edAccPwd, L"");
	SetWindowTextW(g_edAccUid, g_accLuoguName.empty()
	               ? (g_accUid > 0 ? std::to_wstring(g_accUid).c_str() : L"")
	               : g_accLuoguName.c_str());
	SetWindowTextW(g_lblAccMsg, g_accName.empty()
	               ? L"注册后：查自己的洛谷账号会归档到 data\\accounts\\<账号>\\"
	               : L"已登录。查自己的号会更新成绩，查别人的只存 data\\others\\。");
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	Layout(rc.right, rc.bottom);
	InvalidateRect(g_hWnd, nullptr, TRUE);
	SetFocus(g_edAccName);
}


// 刷新「我的成绩」页上的昵称 / rating / 统计行
static void FillMePage() {
	LoadProblemRatings();                // 允许改完 json 直接进页面就生效
	LoadMyProblems();
	std::wstring nick = g_myLuoguName.empty() ? g_accName : g_myLuoguName;
	std::wstring head = L"昵称：" + nick;
	if (g_accUid > 0) head += L"    洛谷 UID：" + std::to_wstring(g_accUid);
	if (!g_accName.empty() && g_accName != nick) head += L"    本机账号：" + g_accName;
	SetWindowTextW(g_lblMeName, head.c_str());

	if (g_myProblems.empty()) {
		SetWindowTextW(g_lblMeRating, L"—");
		SetWindowTextW(g_lblMeStat, L"还没有归档成绩：点「开始」查一次自己的洛谷账号，或点「重新查询」。");
		FillList();
		return;
	}
	ScoreStats st = ComputeStats(g_myProblems);
	wchar_t rb[64];
	swprintf(rb, 64, L"Rating %d", st.avg);
	SetWindowTextW(g_lblMeRating, rb);

	std::wstring line = L"已通过 " + std::to_wstring(st.passed) + L" 题";
	if (st.attempted > 0) line += L" + 部分分 " + std::to_wstring(st.attempted) + L" 题";
	line += L" · 计分：最好 " + std::to_wstring(st.used) + L" 题总分 " + std::to_wstring(st.totalScore)
	        + L" ÷ " + std::to_wstring(g_topN) + L" · 最高 " + std::to_wstring(st.best)
	        + L" / 最低 " + std::to_wstring(st.worst) + L" 分";
	{
		std::wstring dist = L"分布：";
		bool firstD = true;
		for (int d = 8; d >= 1; --d) {
			if (!st.tier[d]) continue;
			if (!firstD) dist += L" · ";
			firstD = false;
			dist += std::wstring(DiffTierShort(d)) + L" " + std::to_wstring(st.tier[d]);
		}
		SetWindowTextW(g_lblMeStat, (firstD ? line : (line + L"\r\n" + dist)).c_str());
	}
	FillList();
}

static void ShowMe() {
	if (g_accName.empty()) {
		MessageBoxW(g_hWnd, L"还没有登录本地账号。\n\n先在「个人账号」里注册或登录，之后每次启动都会自动登录。", L"洛谷计分器", MB_ICONINFORMATION);
		ShowAccount();
		return;
	}
	g_onLogin = false;
	g_onAccount = false;
	g_onMe = true;
	ShowWindow(g_lblTitle, SW_HIDE);
	ShowWindow(g_lblSub, SW_HIDE);
	ShowWindow(g_btnStart, SW_HIDE);
	ShowWindow(g_btnAccount, SW_HIDE);
	ShowWindow(g_btnMe, SW_HIDE);
	ShowWindow(g_btnDaily, SW_HIDE);
	ShowWindow(g_btnPush, SW_HIDE);
	ShowWindow(g_lblAccState, SW_HIDE);
	HideLoginCtl();
	HideAccountCtl();
	for (HWND h : {
	         g_lblMeName, g_lblMeRating, g_lblMeStat, g_btnB50, g_btnAll,
	         g_btnSort, g_btnMeQuery, g_btnMeBack, g_edList
	     })
		ShowWindow(h, SW_SHOW);
	g_meView = 1;                       // 默认进 B50
	SetWindowTextW(g_btnSort, g_meSort == 0 ? L"区分方式：分组"
	               : (g_meSort == 1 ? L"区分方式：按分数" : L"区分方式：按题号"));
	FillMePage();
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	Layout(rc.right, rc.bottom);
	InvalidateRect(g_hWnd, nullptr, TRUE);
}

static void CopyTextToClipboard(const std::wstring& s) {
	if (!OpenClipboard(g_hWnd)) return;
	EmptyClipboard();
	size_t bytes = (s.size() + 1) * sizeof(wchar_t);
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (h) {
		void* dst = GlobalLock(h);
		if (dst) {
			memcpy(dst, s.c_str(), bytes);
			GlobalUnlock(h);
			SetClipboardData(CF_UNICODETEXT, h);
		} else {
			GlobalFree(h);
		}
	}
	CloseClipboard();
}

// ---------------- 每日一题 / 推分推荐 ----------------
// 每日一题：在自己 rating 附近挑一道还没做的题，保持手感
// 推分推荐：挑一道比 rating 高一截的题，用来突破
static LONG g_dailyBusy = 0;
static LONG g_pushBusy = 0;

struct PickJob {
	int mode = 1;                    // 0 = 每日一题, 1 = 推分推荐
	int rating = 0;
	std::vector<ProblemItem> passed;
};
struct PickResult {
	int mode = 1;
	bool ok = false;
	std::wstring title;             // 弹窗标题
	std::wstring text;              // 弹窗正文
	std::wstring brief;             // 启动页状态行
	std::wstring url;
};

static std::wstring PickPath(int mode) {
	return DataDir() + (mode == 0 ? L"daily.json" : L"push.json");
}
static std::string UpperA(std::string s) {
	for (auto& c : s) c = (char)toupper((unsigned char)c);
	return s;
}
// 今天日期（本地时间），一天一换
static std::string TodayKey() {
	SYSTEMTIME st;
	GetLocalTime(&st);
	char b[32];
	sprintf(b, "%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth, (int)st.wDay);
	return std::string(b);
}
struct PickToday {
	std::string date;
	std::vector<std::string> picks;   // 今天已经随机到过的题号
};
static PickToday LoadPickToday(int mode) {
	PickToday t;
	t.date = TodayKey();
	std::string js = ReadAllUtf8(PickPath(mode));
	if (js.empty()) return t;
	std::string d = JsonStr(js, "date");
	if (d != t.date) return t;                 // 不是今天的，重置
	size_t p = js.find("\"picks\"");
	if (p == std::string::npos) return t;
	size_t a = js.find('[', p), b = js.find(']', p);
	if (a == std::string::npos || b == std::string::npos || b < a) return t;
	size_t k = a + 1;
	while (k < b) {
		size_t q1 = js.find('"', k);
		if (q1 == std::string::npos || q1 >= b) break;
		size_t q2 = js.find('"', q1 + 1);
		if (q2 == std::string::npos || q2 > b) break;
		t.picks.push_back(js.substr(q1 + 1, q2 - q1 - 1));
		k = q2 + 1;
	}
	return t;
}
static void SavePickToday(int mode, const PickToday& t) {
	std::string js = "{\r\n  \"date\": \"" + EscJ(t.date) + "\",\r\n"
	                 "  \"note\": \"今天随机到过的题目，重复时会在弹窗里提示\",\r\n  \"picks\": [";
	for (size_t i = 0; i < t.picks.size(); ++i) {
		if (i) js += ", ";
		js += "\"" + EscJ(t.picks[i]) + "\"";
	}
	js += "]\r\n}\r\n";
	WriteAllUtf8(PickPath(mode), js);
}

// 题库列表页同样把数据嵌在 lentille-context 里： data.problems.result = [{"pid":"P1007","name":"...","difficulty":3},...]
static void ParseProblemListPage(const std::string& html, std::vector<ProblemItem>& out) {
	size_t a = html.find("\"problems\"");
	if (a == std::string::npos) return;
	size_t b = html.find("\"result\"", a);
	if (b == std::string::npos) return;
	size_t lb = html.find('[', b);
	if (lb == std::string::npos) return;
	size_t i = lb + 1;
	while (i < html.size()) {
		size_t o = html.find_first_of("{]", i);
		if (o == std::string::npos || html[o] == ']') break;
		size_t e = JsonObjectEnd(html, o);
		if (e <= o) break;
		std::string obj = html.substr(o, e - o + 1);
		ProblemItem it;
		it.pid = JsonStr(obj, "pid");
		it.name = JsonStr(obj, "name");
		long long d = JsonNum(obj, "difficulty");
		it.diff = (d >= 0 && d <= 8) ? (int)d : 0;
		if (!it.pid.empty()) out.push_back(it);
		i = e + 1;
	}
}

// 某一档难度：取第 1 页 + 随机抽 2 页，凑够候选（页数越多，重复概率越低）
static void FetchBucket(NetSession& net, int diff, std::vector<ProblemItem>& out) {
	std::wstring head = L"/problem/list?difficulty=" + std::to_wstring(diff) + L"&page=";
	std::string html = net.Get(head + L"1&_contentOnly=1");
	ParseProblemListPage(html, out);
	long long cnt = JsonNum(html, "count");
	size_t pages = cnt > 0 ? (size_t)((cnt + 49) / 50) : 1;
	if (pages > 40) pages = 40;                 // 随便抽，不用全站翻
	for (int k = 0; k < 2 && pages > 1; ++k) {
		size_t pg = 1 + (size_t)(rand() % (pages - 1));
		if (pg <= 1) continue;
		std::string h2 = net.Get(head + std::to_wstring(pg) + L"&_contentOnly=1");
		ParseProblemListPage(h2, out);
	}
	// 去重（同一页可能被抓两次）
	std::vector<ProblemItem> uniq;
	for (auto& it : out) {
		bool dup = false;
		for (auto& u : uniq) if (u.pid == it.pid) { dup = true; break; }
		if (!dup) uniq.push_back(it);
	}
	out.swap(uniq);
}

// 在 [lo, hi] 分之间收集还没做过的题
static void CollectPool(NetSession& net, int mode, int lo, int hi,
                        const std::vector<std::string>& passed, std::vector<ProblemItem>& pool) {
	// 洛谷题库列表接口的难度只有 0~7（7 是紫/黑合并档），所以按 0~7 抓，
	// 具体分数再按 difficulty_scores.json 里的实际分过滤（黑题就在 7 里面）。
	for (int d = 0; d <= 7; ++d) {
		int ds = RatingOf(d);
		if (ds + 400 < lo || ds - 400 > hi) continue;   // 档内实际分可能有 ±400 的浮动
		std::vector<ProblemItem> v;
		FetchBucket(net, d, v);
		for (auto& it : v) {
			if (it.name.find(N(L"模板")) != std::string::npos) continue;   // 模板题不算练习题
			int sc = ScoreOf(it.pid, it.diff);
			if (sc < lo || sc > hi) continue;
			if (mode == 1 && sc <= lo) continue;                            // 推分推荐必须严格更高
			if (std::find(passed.begin(), passed.end(), UpperA(it.pid)) != passed.end()) continue;
			pool.push_back(it);
		}
	}
}

static DWORD WINAPI PickWorker(LPVOID param) {
	PickJob* job = (PickJob*)param;
	PickResult* res = new PickResult();
	res->mode = job->mode;
	res->title = (job->mode == 0) ? L"每日一题" : L"推分推荐";

	std::vector<std::string> passed;
	for (auto& it : job->passed) passed.push_back(UpperA(it.pid));
	PickToday today = LoadPickToday(job->mode);

	NetSession net;
	if (!net.Open()) {
		res->text = L"连不上洛谷，挑不了题。等会儿再试一次。";
		PostMessageW(g_hWnd, WM_PICK, 0, (LPARAM)res);
		delete job;
		return 0;
	}

	std::vector<ProblemItem> pool;
	if (job->mode == 0) {
		// 每日一题：rating 上下 200 分内，不够再放宽到 400
		for (int band = 200; band <= 400 && pool.empty(); band += 200)
			CollectPool(net, 0, job->rating - band, job->rating + band, passed, pool);
	} else {
		// 推分推荐：只挑严格高于 rating 的，先高 200 分内，没有就逐步放宽
		for (int extra = 200; extra <= 800 && pool.empty(); extra += 200)
			CollectPool(net, 1, job->rating + 1, job->rating + extra, passed, pool);
	}
	net.Close();

	if (pool.empty()) {
		res->text = (job->mode == 0)
		            ? L"没找到合适的练手题：你 rating 附近 400 分以内的题可能都做过了。\n\n"
		              L"可以先重新查一次成绩，或者过段时间再试。"
		            : L"没找到合适的题：比你高 800 分以内的题可能都做过了。\n\n"
		              L"可以先重新查一次成绩，或者过段时间再试。";
		PostMessageW(g_hWnd, WM_PICK, 0, (LPARAM)res);
		delete job;
		return 0;
	}

	// 先把今天已经抽过的排除掉，实在没有才允许重复（并提示）
	std::vector<size_t> cand;
	for (size_t i = 0; i < pool.size(); ++i) {
		std::string key = UpperA(pool[i].pid);
		if (std::find(today.picks.begin(), today.picks.end(), key) == today.picks.end())
			cand.push_back(i);
	}
	bool again = cand.empty();
	if (again) for (size_t i = 0; i < pool.size(); ++i) cand.push_back(i);
	size_t pickIdx = cand[(size_t)(rand() % cand.size())];
	ProblemItem pick = pool[pickIdx];
	int sc = ScoreOf(pick.pid, pick.diff);
	int delta = sc - job->rating;
	std::wstring pidw(pick.pid.begin(), pick.pid.end());

	today.picks.push_back(UpperA(pick.pid));
	if (today.picks.size() > 50) today.picks.erase(today.picks.begin());
	SavePickToday(job->mode, today);

	std::wstring gap;
	if (delta > 0) gap = L"（这道题高 " + std::to_wstring(delta) + L" 分）";
	else if (delta < 0) gap = L"（这道题低 " + std::to_wstring(-delta) + L" 分）";
	else gap = L"（和你现在的分数差不多）";

	res->ok = true;
	res->url = L"https://www.luogu.com.cn/problem/" + pidw;
	res->brief = (job->mode == 0 ? L"每日一题：" : L"推分推荐：") + pidw + L"（"
	             + DiffShort(TierOfScorePid(pick.pid, pick.diff, sc)) + L" · " + std::to_wstring(sc) + L" 分）";
	res->text = (job->mode == 0 ? L"给你挑了一道练手题：" : L"给你挑了一道比现在水平高一截的题：")
	            + std::wstring(L"\r\n\r\n")
	            + pidw + L"    " + W(pick.name.c_str()) + L"\r\n"
	            + L"难度：" + DiffShort(TierOfScorePid(pick.pid, pick.diff, sc)) + L" · " + std::to_wstring(sc) + L" 分\r\n"
	            + L"你的 Rating：" + std::to_wstring(job->rating) + gap + L"\r\n\r\n"
	            + (again ? L"今天随机到过这道题了，换一道也行。\r\n\r\n" : L"")
	            + L"要复制这道题的网址吗？（点「是」复制到剪贴板，再去浏览器粘贴）";
	PostMessageW(g_hWnd, WM_PICK, 0, (LPARAM)res);
	delete job;
	return 0;
}

static void StartPick(int mode) {
	const wchar_t* title = (mode == 0) ? L"每日一题" : L"推分推荐";
	if (g_accName.empty()) {
		MessageBoxW(g_hWnd, L"还没有登录本地账号。\n\n先在「个人账号」里注册或登录，程序才知道你的 Rating。",
		            title, MB_ICONINFORMATION);
		ShowAccount();
		return;
	}
	LoadProblemRatings();
	LoadMyProblems();
	if (g_myProblems.empty()) {
		MessageBoxW(g_hWnd, L"还没有成绩：先点「开始」查一次自己的洛谷账号，\n"
		            L"或者进「我的成绩」点「重新查询」，之后就能按 Rating 挑题了。",
		            title, MB_ICONINFORMATION);
		return;
	}
	LONG* busy = (mode == 0) ? &g_dailyBusy : &g_pushBusy;
	if (InterlockedCompareExchange(busy, 1, 0) != 0) return;
	ScoreStats st = ComputeStats(g_myProblems);
	PickJob* job = new PickJob();
	job->mode = mode;
	job->rating = st.avg;
	job->passed = g_myProblems;
	SetWindowTextW(g_lblAccState, (mode == 0) ? L"正在找一道练手题…" : L"正在找一道比你 Rating 高的题…");
	CloseHandle(CreateThread(nullptr, 0, PickWorker, job, 0, nullptr));
}

// 卡片（登录 / 账号 / 我的成绩）的高度
static int CardHeight() {
	return S(g_onAccount ? 300 : (g_onMe ? 225 : 220));
}

// ---------------- 布局 ----------------
static void Layout(int W_, int H_) {
	// 启动页
	SetWindowPos(g_lblTitle, nullptr, 0, H_ / 5, W_, S(72), SWP_NOZORDER);
	SetWindowPos(g_lblSub,   nullptr, 0, H_ / 5 + S(80), W_, S(32), SWP_NOZORDER);
	SetWindowPos(g_btnStart, nullptr, W_ / 2 - S(100), H_ / 5 + S(150), S(200), S(58), SWP_NOZORDER);
	// 第一行：每日一题 / 推分推荐（都是「挑一道题」的练习入口，单独一排）
	SetWindowPos(g_btnDaily,   nullptr, W_ / 2 - S(186), H_ / 5 + S(228), S(180), S(46), SWP_NOZORDER);
	SetWindowPos(g_btnPush,    nullptr, W_ / 2 + S(6),   H_ / 5 + S(228), S(180), S(46), SWP_NOZORDER);
	// 第二行：我的成绩 / 个人账号
	SetWindowPos(g_btnMe,      nullptr, W_ / 2 - S(152), H_ / 5 + S(288), S(146), S(44), SWP_NOZORDER);
	SetWindowPos(g_btnAccount, nullptr, W_ / 2 + S(6),   H_ / 5 + S(288), S(146), S(44), SWP_NOZORDER);
	SetWindowPos(g_lblAccState, nullptr, S(20), H_ / 5 + S(344), W_ - S(40), S(72), SWP_NOZORDER);

	// 登录页卡片区域
	int cardX = S(40), cardY = S(24), cardW = W_ - S(80);
	int cardH = CardHeight();

	int lx = cardX + S(20), rx = cardX + S(300), rw = cardW - S(320);
	int row = cardY + S(16), rh = S(40), gap = S(8);
	// 第0行: 说明
	SetWindowPos(g_lblMode, nullptr, lx, row + S(6), S(260), S(24), SWP_NOZORDER);
	{
		int cgap = S(10);
		int chipW = (rw - 2 * cgap) / 3;
		for (int i = 0; i < 3; ++i)
			SetWindowPos(g_btnMode[i], nullptr, rx + i * (chipW + cgap), row - S(1),
			             chipW, S(34), SWP_NOZORDER);
	}
	row += rh + gap;
	SetWindowPos(g_lblUser, nullptr, lx, row + S(6), S(260), S(24), SWP_NOZORDER);
	SetWindowPos(g_edUser,  nullptr, rx, row, rw, S(32), SWP_NOZORDER);
	row += rh + gap;
	SetWindowPos(g_lblPwd,  nullptr, lx, row + S(6), S(260), S(24), SWP_NOZORDER);
	SetWindowPos(g_edPwd,   nullptr, rx, row, rw, S(32), SWP_NOZORDER);
	row += rh + gap;
	SetWindowPos(g_lblCid,  nullptr, lx, row + S(6), S(260), S(24), SWP_NOZORDER);
	SetWindowPos(g_edCid,   nullptr, rx, row, rw, S(32), SWP_NOZORDER);
	EditMargins(g_edUser);
	EditMargins(g_edPwd);
	EditMargins(g_edCid);

	// 账号页
	{
		int r = cardY + S(16);
		SetWindowPos(g_lblAccTitle, nullptr, lx, r, S(420), S(26), SWP_NOZORDER);
		r += S(44);
		SetWindowPos(g_lblAccName, nullptr, lx, r + S(6), S(260), S(24), SWP_NOZORDER);
		SetWindowPos(g_edAccName,  nullptr, rx, r, rw, S(32), SWP_NOZORDER);
		r += rh + gap;
		SetWindowPos(g_lblAccPwd, nullptr, lx, r + S(6), S(260), S(24), SWP_NOZORDER);
		SetWindowPos(g_edAccPwd,  nullptr, rx, r, rw, S(32), SWP_NOZORDER);
		r += rh + gap;
		SetWindowPos(g_lblAccUid, nullptr, lx, r + S(6), S(260), S(24), SWP_NOZORDER);
		SetWindowPos(g_edAccUid,  nullptr, rx, r, rw, S(32), SWP_NOZORDER);
		EditMargins(g_edAccName);
		EditMargins(g_edAccPwd);
		EditMargins(g_edAccUid);
	}

	int by = cardY + cardH + S(12);
	SetWindowPos(g_btnBack, nullptr, cardX, by, S(110), S(42), SWP_NOZORDER);
	SetWindowPos(g_btnRun,  nullptr, W_ - cardX - S(240), by, S(240), S(42), SWP_NOZORDER);
	SetWindowPos(g_lblStatus, nullptr, cardX + S(4), by + S(50), cardW - S(8), S(22), SWP_NOZORDER);
	SetWindowPos(g_btnAccLogin, nullptr, cardX, by, S(110), S(42), SWP_NOZORDER);
	SetWindowPos(g_btnAccReg,   nullptr, cardX + S(120), by, S(110), S(42), SWP_NOZORDER);
	SetWindowPos(g_btnAccOut,   nullptr, cardX + S(240), by, S(150), S(42), SWP_NOZORDER);
	SetWindowPos(g_btnAccBack,  nullptr, W_ - cardX - S(110), by, S(110), S(42), SWP_NOZORDER);
	SetWindowPos(g_lblAccMsg,   nullptr, cardX + S(4), by + S(50), cardW - S(8), S(22), SWP_NOZORDER);
	SetWindowPos(g_edLog, nullptr, cardX, by + S(78), cardW, H_ - (by + S(78)) - S(24), SWP_NOZORDER);

	// 我的成绩页
	{
		int r = cardY + S(14);
		SetWindowPos(g_lblMeName,   nullptr, lx, r, cardW - S(40), S(24), SWP_NOZORDER);
		SetWindowPos(g_lblMeRating, nullptr, lx, r + S(30), S(320), S(58), SWP_NOZORDER);
		SetWindowPos(g_lblMeStat,   nullptr, lx, r + S(96), cardW - S(40), S(64), SWP_NOZORDER);

		int bx = cardX, bh = S(44), bgap = S(8);
		SetWindowPos(g_btnB50,     nullptr, bx, by, S(150), bh, SWP_NOZORDER);
		bx += S(150) + bgap;
		SetWindowPos(g_btnAll,     nullptr, bx, by, S(140), bh, SWP_NOZORDER);
		bx += S(140) + bgap;
		SetWindowPos(g_btnSort,    nullptr, bx, by, S(200), bh, SWP_NOZORDER);
		bx += S(200) + bgap;
		SetWindowPos(g_btnMeQuery, nullptr, bx, by, S(130), bh, SWP_NOZORDER);
		SetWindowPos(g_btnMeBack,  nullptr, W_ - cardX - S(100), by, S(100), bh, SWP_NOZORDER);
		SetWindowPos(g_edList, nullptr, cardX, by + S(56), cardW, H_ - (by + S(56)) - S(24), SWP_NOZORDER);
	}
	// 右下角「♥ 赞助」：始终贴在窗口右下角，只是复制链接
	SetWindowPos(g_btnSponsor, nullptr, W_ - S(16) - S(100), H_ - S(26), S(100), S(22),
	             SWP_NOZORDER);
	InvalidateRect(g_hWnd, nullptr, TRUE);   // 卡片背景由父窗口画，尺寸变化整屏重绘
}

// ---------------- 模式切换 ----------------
static void UpdateModeLabels() {
	std::wstring mode = CurrentMode();
	bool cookie = (mode == L"cookie");
	bool pub    = (mode == L"public");
	if (pub) {
		SetWindowTextW(g_lblUser, L"用户名 或 UID");
		SetWindowTextW(g_lblPwd,  L"密码（此模式无需填写）");
		SetWindowTextW(g_lblCid,  L"_client_id（此模式无需填写）");
	} else if (cookie) {
		SetWindowTextW(g_lblUser, L"_uid（Cookie 值）");
		SetWindowTextW(g_lblPwd,  L"_client_id（Cookie 值）");
		SetWindowTextW(g_lblCid,  L"__csrf（Cookie，可选）");
	} else {
		SetWindowTextW(g_lblUser, L"账号 / 用户名");
		SetWindowTextW(g_lblPwd,  L"密码");
		SetWindowTextW(g_lblCid,  L"_client_id（Cookie，可选）");
	}
	EnableWindow(g_edPwd, !pub);
	EnableWindow(g_edCid, !pub);
	// 密码框打码：password 模式显示圆点，cookie 模式显示明文
	SendMessageW(g_edPwd, EM_SETPASSWORDCHAR, cookie ? 0 : 0x25CF, 0);
	SetWindowTextW(g_edPwd, L"");
	InvalidateRect(g_edPwd, nullptr, TRUE);
	for (HWND b : g_btnMode) if (b) InvalidateRect(b, nullptr, TRUE);   // 刷新胶囊高亮
}

// ---------------- 运行 ----------------
static void OnRun() {
	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;   // 已在运行中
	std::wstring mode = CurrentMode();
	std::wstring user = ReadField(g_edUser);
	std::wstring pwd  = ReadField(g_edPwd);
	std::wstring cid  = ReadField(g_edCid);
	bool pub = (mode == L"public");
	if (user.empty() || (!pub && pwd.empty())) {
		MessageBoxW(g_hWnd,
		            pub
		            ? L"请先填写要查询的用户名（或数字 UID）。"
		            : ((mode == L"cookie")
		               ? L"请先填写 _uid 和 _client_id 的 Cookie 值。"
		               : L"请先填写账号和密码。"),
		            L"洛谷计分器", MB_ICONWARNING);
		InterlockedExchange(&g_busy, 0);
		return;
	}
	EnableWindow(g_btnRun, FALSE);
	SetWindowTextW(g_edLog, L"");
	g_statusKind = 0;
	SetWindowTextW(g_lblStatus, L"[进行中] 正在登录 / 查询…");
	JobArgs* job = new JobArgs{ mode, user, pwd, cid, g_accName, g_accUid };
	HANDLE th = CreateThread(nullptr, 0, Worker, job, 0, nullptr);
}

// ---------------- 自绘控件颜色 ----------------
static LRESULT OnCtlColor(HDC hdc, HWND hCtl) {
	if (hCtl == g_edLog) {
		SetTextColor(hdc, C_EDITFG);
		SetBkColor(hdc, C_EDITBG);
		return (LRESULT)g_brEdit;
	}
	if (hCtl == g_lblTitle) {
		SetTextColor(hdc, C_FG);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brBg;
	}
	if (hCtl == g_lblSub)   {
		SetTextColor(hdc, C_SUB);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brBg;
	}
	if (hCtl == g_lblStatus) {
		SetTextColor(hdc, g_statusKind == 2 ? C_RED : C_GREEN);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brBg;
	}
	if (hCtl == g_lblMode || hCtl == g_lblUser || hCtl == g_lblPwd || hCtl == g_lblCid) {
		SetTextColor(hdc, C_FG);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brCard;
	}
	if (hCtl == g_lblAccState) {
		SetTextColor(hdc, C_SUB);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brBg;
	}
	if (hCtl == g_lblMeName) {
		SetTextColor(hdc, C_FG);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brCard;
	}
	if (hCtl == g_lblMeRating) {
		SetTextColor(hdc, C_GREEN);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brCard;
	}
	if (hCtl == g_lblMeStat) {
		SetTextColor(hdc, C_SUB);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brCard;
	}
	if (hCtl == g_lblAccTitle || hCtl == g_lblAccName || hCtl == g_lblAccPwd || hCtl == g_lblAccUid) {
		SetTextColor(hdc, C_FG);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brCard;
	}
	if (hCtl == g_lblAccMsg) {
		SetTextColor(hdc, C_SUB);
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)g_brBg;
	}
	if (hCtl == g_edUser || hCtl == g_edPwd || hCtl == g_edCid ||
	    hCtl == g_edAccName || hCtl == g_edAccPwd || hCtl == g_edAccUid) {
		SetTextColor(hdc, C_EDITFG);
		SetBkColor(hdc, C_EDITBG);
		return (LRESULT)g_brEdit;
	}
	SetTextColor(hdc, C_FG);
	SetBkColor(hdc, C_BG);
	return (LRESULT)g_brBg;
}

// 输入框子类化：回车 = 开始 / 登录并获取（配合 IsDialogMessage 的 Tab 切换）
static LRESULT CALLBACK EditSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                    UINT_PTR, DWORD_PTR) {
	if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
		if (g_btnRun && IsWindowVisible(g_btnRun) && IsWindowEnabled(g_btnRun)) OnRun();
		else if (g_onAccount) OnAccountLogin();
		else if (g_btnStart && IsWindowVisible(g_btnStart)) ShowLogin();
		return 0;
	}
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}

// ---------------- 窗口过程 ----------------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			g_hWnd = hWnd;
			g_brBg    = CreateSolidBrush(C_BG);
			g_brCard  = CreateSolidBrush(C_CARD);
			g_brEdit  = CreateSolidBrush(C_EDITBG);
			g_fTitle  = CreateFontW(-S(48), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        DEFAULT_PITCH, L"Microsoft YaHei UI");
			g_fSub    = CreateFontW(-S(17), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        DEFAULT_PITCH, L"Microsoft YaHei UI");
			g_fUI     = CreateFontW(-S(16), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        DEFAULT_PITCH, L"Microsoft YaHei UI");
			g_fUIBold = CreateFontW(-S(16), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        DEFAULT_PITCH, L"Microsoft YaHei UI");
			g_fBigBtn = CreateFontW(-S(26), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        DEFAULT_PITCH, L"Microsoft YaHei UI");
			g_fMono   = CreateFontW(-S(14), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        FIXED_PITCH, L"Consolas");
			g_fRating = CreateFontW(-S(38), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
			                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                        DEFAULT_PITCH, L"Microsoft YaHei UI");

			DWORD wsChild = WS_CHILD | WS_VISIBLE;
			g_lblTitle = CreateWindowExW(0, L"STATIC", L"洛谷计分器",
			                             wsChild | SS_CENTER, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_lblSub = CreateWindowExW(0, L"STATIC",
			                           L"Luogu Scorer · 登录后查询已完成（通过）的题目",
			                           wsChild | SS_CENTER, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_btnStart = CreateWindowExW(0, L"BUTTON", L"开 始",
			                             wsChild | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1001, g_hInst, nullptr);

			g_lblMode = CreateWindowExW(0, L"STATIC", L"登录方式",
			                            wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			for (int i = 0; i < 3; ++i)
				g_btnMode[i] = CreateWindowExW(0, L"BUTTON", kModeNames[i],
				                               wsChild | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0,
				                               hWnd, (HMENU)(INT_PTR)(1010 + i), g_hInst, nullptr);
			g_lblUser = CreateWindowExW(0, L"STATIC", L"账号 / 用户名",
			                            wsChild | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)1020, g_hInst, nullptr);
			g_edUser = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                           wsChild | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)1003, g_hInst, nullptr);
			g_lblPwd = CreateWindowExW(0, L"STATIC", L"密码",
			                           wsChild | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)1021, g_hInst, nullptr);
			g_edPwd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                          wsChild | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0,
			                          hWnd, (HMENU)1004, g_hInst, nullptr);
			g_lblCid = CreateWindowExW(0, L"STATIC", L"_client_id（Cookie，可选）",
			                           wsChild | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)1022, g_hInst, nullptr);
			g_edCid = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                          wsChild | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)1005, g_hInst, nullptr);

			// 启动页：我的成绩 / 个人账号 / 账号状态行
			g_btnMe = CreateWindowExW(0, L"BUTTON", L"我的成绩",
			                          wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1009, g_hInst, nullptr);
			g_btnDaily = CreateWindowExW(0, L"BUTTON", L"每日一题",
			                             wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1056, g_hInst, nullptr);
			g_btnPush = CreateWindowExW(0, L"BUTTON", L"推分推荐",
			                            wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1055, g_hInst, nullptr);
			g_btnAccount = CreateWindowExW(0, L"BUTTON", L"个人账号",
			                               wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1008, g_hInst, nullptr);
			g_lblAccState = CreateWindowExW(0, L"STATIC", L"",
			                                wsChild | SS_CENTER, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);

			// 账号页
			g_lblAccTitle = CreateWindowExW(0, L"STATIC", L"个人账号 · 数据只存在本机 data 目录",
			                                wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_lblAccName = CreateWindowExW(0, L"STATIC", L"账号名（随便起）",
			                               wsChild | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)1040, g_hInst, nullptr);
			g_edAccName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                              wsChild | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)1033, g_hInst, nullptr);
			g_lblAccPwd = CreateWindowExW(0, L"STATIC", L"密码（至少 4 位）",
			                              wsChild | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)1041, g_hInst, nullptr);
			g_edAccPwd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                             wsChild | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0,
			                             hWnd, (HMENU)1034, g_hInst, nullptr);
			g_lblAccUid = CreateWindowExW(0, L"STATIC", L"绑定洛谷 UID / 用户名",
			                              wsChild | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)1042, g_hInst, nullptr);
			g_edAccUid = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                             wsChild | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)1035, g_hInst, nullptr);
			g_btnAccLogin = CreateWindowExW(0, L"BUTTON", L"登录",
			                                wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1036, g_hInst, nullptr);
			g_btnAccReg = CreateWindowExW(0, L"BUTTON", L"注册",
			                              wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1037, g_hInst, nullptr);
			g_btnAccOut = CreateWindowExW(0, L"BUTTON", L"退出登录",
			                              wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1038, g_hInst, nullptr);
			g_btnAccBack = CreateWindowExW(0, L"BUTTON", L"返回",
			                               wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1039, g_hInst, nullptr);
			g_lblAccMsg = CreateWindowExW(0, L"STATIC", L"",
			                              wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);

			// 我的成绩页
			g_lblMeName = CreateWindowExW(0, L"STATIC", L"昵称：",
			                              wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_lblMeRating = CreateWindowExW(0, L"STATIC", L"—",
			                                wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_lblMeStat = CreateWindowExW(0, L"STATIC", L"",
			                              wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_btnB50 = CreateWindowExW(0, L"BUTTON", L"B50 最佳 50 题",
			                           wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1050, g_hInst, nullptr);
			g_btnAll = CreateWindowExW(0, L"BUTTON", L"总览全部",
			                           wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1051, g_hInst, nullptr);
			g_btnSort = CreateWindowExW(0, L"BUTTON", L"区分方式：分组",
			                            wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1052, g_hInst, nullptr);
			g_btnMeQuery = CreateWindowExW(0, L"BUTTON", L"重新查询",
			                               wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1054, g_hInst, nullptr);
			g_btnMeBack = CreateWindowExW(0, L"BUTTON", L"返回",
			                              wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1053, g_hInst, nullptr);
			LoadLibraryW(L"Msftedit.dll");       // RichEdit 5.0：列表里可以按难度上色
			g_edList = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
			                           wsChild | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
			                           0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);

			g_btnBack = CreateWindowExW(0, L"BUTTON", L"返回",
			                            wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1006, g_hInst, nullptr);
			g_btnRun = CreateWindowExW(0, L"BUTTON", L"登录并获取已通过题目",
			                           wsChild | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1007, g_hInst, nullptr);
			g_lblStatus = CreateWindowExW(0, L"STATIC", L"",
			                              wsChild, 0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			g_edLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			                          wsChild | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
			                          0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			// 右下角常驻的小「赞助」入口：点了只复制链接，不打开浏览器
			g_btnSponsor = CreateWindowExW(0, L"BUTTON", L"♥ 赞助",
			                               wsChild | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0,
			                               hWnd, (HMENU)1060, g_hInst, nullptr);

			SendMessageW(g_lblTitle, WM_SETFONT, (WPARAM)g_fTitle, TRUE);
			SendMessageW(g_lblSub,   WM_SETFONT, (WPARAM)g_fSub, TRUE);
			SendMessageW(g_btnStart, WM_SETFONT, (WPARAM)g_fBigBtn, TRUE);
			SendMessageW(g_lblMode,  WM_SETFONT, (WPARAM)g_fUI, TRUE);
			for (HWND b : g_btnMode) SendMessageW(b, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblUser,  WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_edUser,   WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_lblPwd,   WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_edPwd,    WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_lblCid,   WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_edCid,    WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_btnMe, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnDaily, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnPush, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblMeName,   WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblMeRating, WM_SETFONT, (WPARAM)g_fRating, TRUE);
			SendMessageW(g_lblMeStat,   WM_SETFONT, (WPARAM)g_fSub, TRUE);
			SendMessageW(g_btnB50, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnAll, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnSort, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnMeQuery, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnMeBack, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnAccount, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblAccState, WM_SETFONT, (WPARAM)g_fSub, TRUE);
			SendMessageW(g_lblAccTitle, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblAccName, WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_edAccName,  WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_lblAccPwd, WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_edAccPwd,  WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_lblAccUid, WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_edAccUid,  WM_SETFONT, (WPARAM)g_fUI, TRUE);
			SendMessageW(g_btnAccLogin, WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnAccReg,   WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnAccOut,   WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnAccBack,  WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblAccMsg,   WM_SETFONT, (WPARAM)g_fSub, TRUE);
			SendMessageW(g_btnBack,  WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_btnRun,   WM_SETFONT, (WPARAM)g_fUIBold, TRUE);
			SendMessageW(g_lblStatus, WM_SETFONT, (WPARAM)g_fSub, TRUE);
			SendMessageW(g_edLog,    WM_SETFONT, (WPARAM)g_fMono, TRUE);
			SendMessageW(g_btnSponsor, WM_SETFONT, (WPARAM)g_fSub, TRUE);
			if (g_edList) {
				SendMessageW(g_edList, EM_SETBKGNDCOLOR, 0, (LPARAM)C_EDITBG);
				CHARFORMAT2W cf = {};
				cf.cbSize = sizeof(cf);
				cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD;
				cf.crTextColor = C_EDITFG;
				cf.dwEffects = CFE_BOLD;
				cf.yHeight = (LONG)S(16) * 10;
				wcscpy_s(cf.szFaceName, L"Microsoft YaHei UI");
				SendMessageW(g_edList, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
			}
			SetWindowTextW(g_edLog,
			               L"这里会显示运行日志。\r\n"
			               L"「public」模式只需填用户名或 UID；也可用账号密码或 Cookie 登录后查询。\r\n");

			// 回车直接开始 / 回车直接查询：给三个输入框装子类化过程
			for (HWND ed : {
			         g_edUser, g_edPwd, g_edCid, g_edAccName, g_edAccPwd, g_edAccUid
			     })
				SetWindowSubclass(ed, EditSubProc, 1, 0);

			ShowStart();
			return 0;
		}
		case WM_SIZE:
			Layout(LOWORD(lParam), HIWORD(lParam));
			return 0;
		case WM_GETMINMAXINFO: {
			MINMAXINFO* mmi = (MINMAXINFO*)lParam;
			mmi->ptMinTrackSize.x = S(720);
			mmi->ptMinTrackSize.y = S(560);
			return 0;
		}
		case WM_ERASEBKGND: {
			HDC hdc = (HDC)wParam;
			RECT rc;
			GetClientRect(hWnd, &rc);
			FillRect(hdc, &rc, g_brBg);
			if (g_onLogin || g_onAccount || g_onMe) {   // 卡片背景由父窗口画，避免子窗口遮挡鼠标
				RECT card = { S(40), S(24), rc.right - S(40), CardHeight() };
				FillRect(hdc, &card, g_brCard);
			}
			// 窗口最下方常驻的免费声明（只画字，不占控件、不改布局）
			{
				RECT foot = { S(16), rc.bottom - S(26), rc.right - S(128), rc.bottom - S(4) };
				if (foot.right > foot.left) {
					SetBkMode(hdc, TRANSPARENT);
					SetTextColor(hdc, C_SUB);
					HFONT of = (HFONT)SelectObject(hdc, g_fSub);
					DrawTextW(hdc, L"本工具完全免费，由鸣破音弦制作", -1, &foot,
					          DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
					SelectObject(hdc, of);
				}
			}
			return 1;
		}
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
			return OnCtlColor((HDC)wParam, (HWND)lParam);
		case WM_DRAWITEM: {
			DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
			if (dis->CtlID == 1060) {          // 右下角「♥ 赞助」：画成低调的文字链
				RECT rc = dis->rcItem;
				FillRect(dis->hDC, &rc, g_brBg);
				SetBkMode(dis->hDC, TRANSPARENT);
				bool down = (dis->itemState & ODS_SELECTED) != 0;
				SetTextColor(dis->hDC, down ? C_GREEN : C_SUB);
				HFONT of = (HFONT)SelectObject(dis->hDC, g_fSub);
				DrawTextW(dis->hDC, L"♥ 赞助", -1, &rc,
				          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
				SelectObject(dis->hDC, of);
				return TRUE;
			}
			int idx = (int)dis->CtlID - 1010;
			if (idx < 0 || idx > 2) break;
			bool active = (idx == g_modeIdx);
			RECT rc = dis->rcItem;
			HBRUSH fill = CreateSolidBrush(active ? C_FG : C_CARD);
			FillRect(dis->hDC, &rc, fill);
			DeleteObject(fill);
			HPEN pen = CreatePen(PS_SOLID, 1, active ? C_FG : C_LINE);
			HGDIOBJ op = SelectObject(dis->hDC, pen);
			HGDIOBJ ob = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
			Rectangle(dis->hDC, rc.left, rc.top, rc.right, rc.bottom);
			SelectObject(dis->hDC, ob);
			SelectObject(dis->hDC, op);
			DeleteObject(pen);
			SetBkMode(dis->hDC, TRANSPARENT);
			SetTextColor(dis->hDC, active ? C_BG : C_SUB);
			HFONT of = (HFONT)SelectObject(dis->hDC, g_fUIBold);
			DrawTextW(dis->hDC, kModeNames[idx], -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			SelectObject(dis->hDC, of);
			return TRUE;
		}
		case WM_SETCURSOR: {
			if ((HWND)wParam == g_btnSponsor) {   // 手型光标，提示可以点
				SetCursor(LoadCursorW(nullptr, IDC_HAND));
				return TRUE;
			}
			break;
		}
		case WM_COMMAND: {
			int id = LOWORD(wParam), code = HIWORD(wParam);
			if (id == 1001 && code == BN_CLICKED) {
				ShowLogin();
				return 0;
			}
			if (id == 1060 && code == BN_CLICKED) {         // 右下角赞助：只复制链接
				CopyTextToClipboard(kSponsorUrl);
				MessageBoxW(hWnd,
				            L"赞助链接已经复制到剪贴板：\r\n\r\nhttps://afdian.com/a/PoYinprogramme\r\n\r\n打开浏览器，在地址栏按 Ctrl+V 粘贴就行。谢谢支持！",
				            L"赞助作者", MB_OK | MB_ICONINFORMATION);
				return 0;
			}
			if (id == 1006 && code == BN_CLICKED) {
				ShowStart();
				return 0;
			}
			if (id == 1007 && code == BN_CLICKED) {
				OnRun();
				return 0;
			}
			if (id >= 1010 && id <= 1012) {                 // 点胶囊切换登录方式
				g_modeIdx = id - 1010;
				UpdateModeLabels();
				return 0;
			}
			if (id == 1009 && code == BN_CLICKED) {
				ShowMe();
				return 0;
			}
			if (id == 1056 && code == BN_CLICKED) {         // 每日一题
				StartPick(0);
				return 0;
			}
			if (id == 1055 && code == BN_CLICKED) {         // 推分推荐
				StartPick(1);
				return 0;
			}
			if (id == 1050 && code == BN_CLICKED) {
				g_meView = 1;
				FillList();
				return 0;
			}
			if (id == 1051 && code == BN_CLICKED) {
				g_meView = 2;
				FillList();
				return 0;
			}
			if (id == 1052 && code == BN_CLICKED) {
				g_meSort = (g_meSort + 1) % 3;
				SetWindowTextW(g_btnSort, g_meSort == 0 ? L"区分方式：分组"
				               : (g_meSort == 1 ? L"区分方式：按分数" : L"区分方式：按题号"));
				FillList();
				return 0;
			}
			if (id == 1053 && code == BN_CLICKED) {
				ShowStart();
				return 0;
			}
			if (id == 1054 && code == BN_CLICKED) {        // 重新查一次自己的号
				ShowLogin();
				if (g_accUid > 0) {
					g_modeIdx = 2;
					UpdateModeLabels();
					SetWindowTextW(g_edUser, std::to_wstring(g_accUid).c_str());
				}
				OnRun();
				return 0;
			}
			if (id == 1008 && code == BN_CLICKED) {
				ShowAccount();
				return 0;
			}
			if (id == 1036) {
				OnAccountLogin();
				return 0;
			}
			if (id == 1037) {
				OnAccountRegister();
				return 0;
			}
			if (id == 1038) {
				OnAccountLogout();
				return 0;
			}
			if (id == 1039) {
				ShowStart();
				return 0;
			}
			if ((id == 1040 || id == 1041 || id == 1042) && code == STN_CLICKED) {
				HWND ed = (id == 1040) ? g_edAccName : (id == 1041 ? g_edAccPwd : g_edAccUid);
				if (ed) {
					SetFocus(ed);
					SendMessageW(ed, EM_SETSEL, 0, -1);
				}
				return 0;
			}
			if ((id == 1020 || id == 1021 || id == 1022) && code == STN_CLICKED) {
				HWND ed = (id == 1020) ? g_edUser : (id == 1021 ? g_edPwd : g_edCid);
				if (ed && IsWindowEnabled(ed)) {             // 点标签即选中对应输入框
					SetFocus(ed);
					SendMessageW(ed, EM_SETSEL, 0, -1);
				}
				return 0;
			}
			break;
		}
		case WM_LOG: {
			wchar_t* p = (wchar_t*)lParam;
			int len = GetWindowTextLengthW(g_edLog);
			SendMessageW(g_edLog, EM_SETSEL, len, len);
			std::wstring line = std::wstring(p) + L"\r\n";
			SendMessageW(g_edLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
			SendMessageW(g_edLog, EM_SCROLLCARET, 0, 0);
			if (p[0] == L'[') {
				if (!wcsncmp(p, L"[失败]", 4) || !wcsncmp(p, L"[错误]", 4) || !wcsncmp(p, L"[警告]", 4))
					g_statusKind = 2;
				else if (!wcsncmp(p, L"[完成]", 4) || !wcsncmp(p, L"[成功]", 4))
					g_statusKind = 1;
				else
					g_statusKind = 0;
				SetWindowTextW(g_lblStatus, p);
			}
			delete[] p;
			return 0;
		}
		case WM_PICK: {
			PickResult* res = (PickResult*)lParam;
			if (!res) return 0;
			InterlockedExchange(res->mode == 0 ? &g_dailyBusy : &g_pushBusy, 0);
			if (res->ok) {
				if (res->mode == 0) g_dailyBrief = res->brief;
				else                g_pushBrief  = res->brief;
				RefreshAccountLabel();
				// 不再尝试自动拉起浏览器（低完整性进程启动 Edge 会静默失败，行为不可靠），
				// 统一改成「复制网址」，由用户自己粘到浏览器里。
				if (MessageBoxW(g_hWnd, res->text.c_str(), res->title.c_str(),
				                MB_YESNO | MB_ICONQUESTION) == IDYES) {
					CopyTextToClipboard(res->url);
					std::wstring done = L"题目网址已经复制到剪贴板：\r\n\r\n" + res->url
					                    + L"\r\n\r\n打开浏览器，在地址栏按 Ctrl+V 再回车就能去做这道题。";
					MessageBoxW(g_hWnd, done.c_str(), L"已复制题目网址", MB_ICONINFORMATION);
				}
			} else {
				RefreshAccountLabel();
				MessageBoxW(g_hWnd, res->text.c_str(), res->title.c_str(), MB_ICONINFORMATION);
			}
			delete res;
			return 0;
		}
		case WM_DONE:
			InterlockedExchange(&g_busy, 0);
			EnableWindow(g_btnRun, TRUE);
			RefreshAccountLabel();          // 归档后更新启动页上的「我的成绩」
			if (g_onMe) FillMePage();
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------- 入口 ----------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
	srand((unsigned)clock() ^ (unsigned)GetTickCount());   // 每次启动都要不一样，否则挑题永远同一道

	// --datadir 由「低完整性时自重启」传进来，让 data 目录仍指向 exe 原来的位置
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv) {
		for (int i = 1; i + 1 < argc; ++i) {
			if (_wcsicmp(argv[i], L"--datadir") == 0) {
				g_dataDirOverride = argv[i + 1];
				if (!g_dataDirOverride.empty() && g_dataDirOverride.back() != L'\\')
					g_dataDirOverride += L'\\';
			}
		}
		LocalFree(argv);
	}
	// 说明：每日一题 / 推分推荐 只负责「把题目网址复制到剪贴板」，不自己去拉起浏览器，
	// 这样不受进程完整性级别（exe 所在目录带 Low 标签）影响，行为稳定可预期。
	g_hInst = hInstance;
	LoadConfig();
	LoadProblemRatings();
	LoadSession();                      // 上次登录的账号：自动登录，不用每次重输                       // data\config.json（第一次运行自动生成默认值）

	// 启用 Per-Monitor DPI 感知，避免高分屏下界面被系统拉伸变模糊
	if (HMODULE hUser = GetModuleHandleW(L"user32.dll")) {
		typedef BOOL (WINAPI * PFN_SetDpiCtx)(HANDLE);
		PFN_SetDpiCtx fn = (PFN_SetDpiCtx)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
		if (fn) fn((HANDLE) -4);  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
	}
	HDC hdcScreen = GetDC(nullptr);
	g_scale = GetDeviceCaps(hdcScreen, LOGPIXELSY) / 96.0;
	ReleaseDC(nullptr, hdcScreen);
	if (g_scale < 1.0) g_scale = 1.0;
	if (g_scale > 3.0) g_scale = 3.0;

	WNDCLASSEXW wc = { sizeof(wc) };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszClassName = L"LuoguScorerWnd";
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	RegisterClassExW(&wc);

	// 常用控件 v6 (按钮样式更现代)
	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
	InitCommonControlsEx(&icc);

	RECT rc = { 0, 0, S(800), S(640) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX), FALSE);
	HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"洛谷计分器",
	                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	                            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
	                            nullptr, nullptr, hInstance, nullptr);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		if (IsDialogMessageW(hWnd, &msg)) continue;   // Tab / Shift+Tab 在控件间切换
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	DeleteObject(g_fTitle);
	DeleteObject(g_fSub);
	DeleteObject(g_fUI);
	DeleteObject(g_fUIBold);
	DeleteObject(g_fBigBtn);
	DeleteObject(g_fMono);
	DeleteObject(g_fRating);
	DeleteObject(g_brBg);
	DeleteObject(g_brCard);
	DeleteObject(g_brEdit);
	return (int)msg.wParam;
}
