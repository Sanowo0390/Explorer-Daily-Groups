// ==WindhawkMod==
// @id              explorer-daily-groups
// @name            Explorer Daily Groups
// @description     Experimental daily date grouping. Disabled by default; see Readme and build restrictions.
// @version         0.1.0
// @author          Sanowo
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -lpropsys -lversion -luuid
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Explorer Daily Groups — experimental prototype

NOT validated in Explorer. This is not a production-ready mod.
Only the inspected DLL versions are accepted:
windows.storage.dll 10.0.26100.9949; propsys.dll 7.0.26100.9549.
That is a development gate, NOT a compatibility certification.

Close ALL folder windows/tabs before enabling, disabling, changing settings,
or updating this mod. Reopen them afterwards. Existing category caches are not
rebuilt by this prototype. Do not test during file transfers.

Enable daily grouping is OFF by default. Modified date is selected by default;
Created date is optional. Select Group by > Date modified/Date created yourself.
Headers use yyyy-MM-dd. No files, timestamps, registry settings or sort columns
are written by this code. Search, libraries, cloud and virtual folders, group
filter actions, collapse persistence, midnight/time-zone changes and other
Explorer mods are untested. First test in a disposable local folder/VM.

The implementation changes the property grouping range to discrete, and scopes
date normalization to CDetailCategorizer::OnGetItemCategory on the calling
thread. It uses PDB symbols, never fixed offsets. Missing symbols fail init.
Whether Explorer uses this complete path is NOT yet established by a UI test.
See the accompanying README.md and TEST-PLAN.ja.md for evidence and limits.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- enabled: false
  $name: Enable daily grouping (experimental)
  $description: Close all Explorer folder windows before changing any setting.
- modified: true
  $name: Date modified
- created: false
  $name: Date created
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <strsafe.h>
#include <shobjidl.h>
// Windhawk's default WINVER can hide this Windows 8+ declaration.
extern "C" WINBASEAPI BOOL WINAPI SystemTimeToTzSpecificLocalTimeEx(
    const DYNAMIC_TIME_ZONE_INFORMATION*, const SYSTEMTIME*, LPSYSTEMTIME);

namespace daily {
// Canonical INTERNAL value: local Gregorian calendar date encoded at 00:00 UTC.
// It is deliberately a date token, not a real file timestamp. Never persist it.
// This avoids ambiguous/nonexistent local midnights and provides stable ordering.
bool MakeDateToken(const FILETIME& input, const DYNAMIC_TIME_ZONE_INFORMATION& tz,
                   FILETIME* token) {
    SYSTEMTIME utc{}, local{};
    if (!token || !FileTimeToSystemTime(&input, &utc) ||
        !SystemTimeToTzSpecificLocalTimeEx(&tz, &utc, &local) ||
        local.wYear < 1601 || local.wYear > 9999) return false;
    local.wHour = local.wMinute = local.wSecond = local.wMilliseconds = 0;
    return SystemTimeToFileTime(&local, token) != FALSE;
}
HRESULT FormatToken(const FILETIME& token, PWSTR* output) {
    if (!output) return E_POINTER;
    *output = nullptr;
    SYSTEMTIME date{};
    if (!FileTimeToSystemTime(&token, &date) || date.wYear > 9999)
        return E_INVALIDARG;
    auto text = static_cast<PWSTR>(CoTaskMemAlloc(11 * sizeof(wchar_t)));
    if (!text) return E_OUTOFMEMORY;
    HRESULT hr = StringCchPrintfW(text, 11, L"%04u-%02u-%02u",
                                  date.wYear, date.wMonth, date.wDay);
    if (FAILED(hr)) { CoTaskMemFree(text); return hr; }
    *output = text;
    return S_OK;
}
}

#ifndef EDG_CORE_ONLY
#include <windhawk_utils.h>
#include <wrl/client.h>
#include <vector>
#include <atomic>
using Microsoft::WRL::ComPtr;

namespace {
bool g_modified, g_created;
std::atomic<unsigned long> g_ranges{0}, g_scopes{0}, g_tokens{0}, g_labels{0};
DYNAMIC_TIME_ZONE_INFORMATION g_zone{};
// Initialized before hooks become active; settings changes request a full reload.
bool Selected(REFPROPERTYKEY key) {
    return (g_modified && IsEqualPropertyKey(key, PKEY_DateModified)) ||
           (g_created && IsEqualPropertyKey(key, PKEY_DateCreated));
}
struct Context {
    void* item;
    PROPERTYKEY key;
    bool tokenProduced = false;
    FILETIME token{};
};
thread_local Context* g_context = nullptr;
struct Scope {
    Context* previous;
    explicit Scope(Context* current) : previous(g_context) { g_context = current; }
    ~Scope() { g_context = previous; }
};

using GroupFn = HRESULT(WINAPI*)(IPropertyDescription*, PROPDESC_GROUPING_RANGE*);
using FormatFn = HRESULT(WINAPI*)(IPropertyDescription*, REFPROPVARIANT,
                                PROPDESC_FORMAT_FLAGS, PWSTR*);
using CategoryFn = HRESULT(WINAPI*)(void*, void*, IShellFolder2*, PROPVARIANT*);
using ValueFn = HRESULT(WINAPI*)(void*, REFPROPERTYKEY, PROPVARIANT*);
using KeyFn = HRESULT(WINAPI*)(void*, PROPERTYKEY*);
GroupFn g_group;
FormatFn g_format;
CategoryFn g_category;
ValueFn g_value;
KeyFn g_key;

HRESULT WINAPI GroupHook(IPropertyDescription* self, PROPDESC_GROUPING_RANGE* range) {
    HRESULT hr = g_group(self, range);
    PROPERTYKEY key{};
    if (SUCCEEDED(hr) && range && *range == PDGR_DATE &&
        SUCCEEDED(self->GetPropertyKey(&key)) && Selected(key)) {
        *range = PDGR_DISCRETE;
        ++g_ranges;
    }
    return hr;
}
HRESULT WINAPI CategoryHook(void* self, void* item, IShellFolder2* folder,
                            PROPVARIANT* category) {
    PROPERTYKEY key{};
    // Resolve a native member function rather than reading private object fields.
    bool selected = SUCCEEDED(g_key(self, &key)) && Selected(key);
    if (selected) ++g_scopes;
    Context context{item, key};
    Scope scope(selected ? &context : nullptr);
    return g_category(self, item, folder, category);
}
HRESULT WINAPI ValueHook(void* self, REFPROPERTYKEY key, PROPVARIANT* value) {
    HRESULT hr = g_value(self, key, value);
    auto context = g_context;
    if (hr == S_OK && value && value->vt == VT_FILETIME && context &&
        self == context->item && IsEqualPropertyKey(key, context->key)) {
        FILETIME token{};
        if (daily::MakeDateToken(value->filetime, g_zone, &token)) {
            value->filetime = token;
            context->token = token;
            context->tokenProduced = true;
            ++g_tokens;
        }
    }
    return hr;
}
HRESULT WINAPI FormatHook(IPropertyDescription* self, REFPROPVARIANT value,
                          PROPDESC_FORMAT_FLAGS flags, PWSTR* text) {
    auto context = g_context;
    PROPERTYKEY key{};
    if (context && context->tokenProduced && value.vt == VT_FILETIME &&
        CompareFileTime(&value.filetime, &context->token) == 0 &&
        SUCCEEDED(self->GetPropertyKey(&key)) && IsEqualPropertyKey(key, context->key)) {
        ++g_labels;
        return daily::FormatToken(value.filetime, text);
    }
    return g_format(self, value, flags, text);
}
bool VersionIs(HMODULE module, DWORD majorMinor, DWORD buildRevision) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(module, path, ARRAYSIZE(path));
    if (!n || n == ARRAYSIZE(path)) return false;
    DWORD size = GetFileVersionInfoSizeW(path, nullptr);
    if (!size) return false;
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path, 0, size, data.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr; UINT len = 0;
    return VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &len) &&
           len >= sizeof(*info) && info->dwSignature == 0xFEEF04BD &&
           info->dwFileVersionMS == majorMinor && info->dwFileVersionLS == buildRevision;
}
}

BOOL Wh_ModInit() {
    if (!Wh_GetIntSetting(L"enabled")) {
        Wh_Log(L"Disabled by setting. No hooks installed.");
        return TRUE;
    }
    g_modified = Wh_GetIntSetting(L"modified") != 0;
    g_created = Wh_GetIntSetting(L"created") != 0;
    if (!g_modified && !g_created) return TRUE;
    HMODULE storage = GetModuleHandleW(L"windows.storage.dll");
    HMODULE propsys = GetModuleHandleW(L"propsys.dll");
    if (!storage || !propsys ||
        !VersionIs(storage, MAKELONG(0,10), MAKELONG(9949,26100)) ||
        !VersionIs(propsys, MAKELONG(0,7), MAKELONG(9549,26100))) {
        Wh_Log(L"Unsupported DLL versions. No hooks installed."); return FALSE;
    }
    if (GetDynamicTimeZoneInformation(&g_zone) == TIME_ZONE_ID_INVALID) return FALSE;
    const WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {{L"protected: virtual long __cdecl CDetailCategorizer::OnGetItemCategory(struct ICategorizableItem *,struct IShellFolder2 *,struct tagPROPVARIANT *)"}, &g_category, CategoryHook},
        {{L"public: virtual long __cdecl CCategorizableItemOnItemStore::GetValue(struct _tagpropertykey const &,struct tagPROPVARIANT *)"}, &g_value, ValueHook},
        {{L"public: virtual long __cdecl CBaseCategorizer::GetPropertyKey(struct _tagpropertykey *)"}, &g_key},
    };
    if (!WindhawkUtils::HookSymbols(storage, hooks, ARRAYSIZE(hooks))) return FALSE;
    ComPtr<IPropertyDescription> modified, created;
    if (FAILED(PSGetPropertyDescription(PKEY_DateModified, IID_PPV_ARGS(&modified))) ||
        FAILED(PSGetPropertyDescription(PKEY_DateCreated, IID_PPV_ARGS(&created)))) return FALSE;
    // IPropertyDescription's documented ABI: IUnknown + methods in propsys.h.
    // Hook through the actual interface pointer so adjustment thunks are respected.
    auto a = *reinterpret_cast<void***>(modified.Get());
    auto b = *reinterpret_cast<void***>(created.Get());
    constexpr size_t groupingSlot = 13, formatSlot = 22;
    if (a[groupingSlot] != b[groupingSlot] || a[formatSlot] != b[formatSlot]) return FALSE;
    if (!Wh_SetFunctionHook(a[groupingSlot], reinterpret_cast<void*>(GroupHook),
                           reinterpret_cast<void**>(&g_group)) ||
        !Wh_SetFunctionHook(a[formatSlot], reinterpret_cast<void*>(FormatHook),
                           reinterpret_cast<void**>(&g_format))) return FALSE;
    Wh_Log(L"Experimental hooks scheduled; Explorer UI behavior is unverified.");
    return TRUE;
}
void Wh_ModSettingsChanged(BOOL* reload) { *reload = TRUE; }
void Wh_ModUninit() {
    Wh_Log(L"Daily Groups totals: ranges=%lu scopes=%lu tokens=%lu labels=%lu",
           g_ranges.load(), g_scopes.load(), g_tokens.load(), g_labels.load());
}
#endif
