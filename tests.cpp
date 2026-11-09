#define EDG_CORE_ONLY
#include "explorer-daily-groups.wh.cpp"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
int checks=0;
void Check(bool ok,const char* name) {++checks;if(!ok){printf("FAIL: %s\n",name);exit(1);}}
FILETIME Ft(WORD y,WORD m,WORD d,WORD h=0,WORD min=0,WORD sec=0){SYSTEMTIME s{};s.wYear=y;s.wMonth=m;s.wDay=d;s.wHour=h;s.wMinute=min;s.wSecond=sec;FILETIME f{};Check(SystemTimeToFileTime(&s,&f),"construct input");return f;}
FILETIME Token(FILETIME f,const DYNAMIC_TIME_ZONE_INFORMATION& tz,const wchar_t* expected){FILETIME t{};Check(daily::MakeDateToken(f,tz,&t),"convert token");PWSTR text=nullptr;Check(SUCCEEDED(daily::FormatToken(t,&text)),"format token");Check(wcscmp(text,expected)==0,"expected date");CoTaskMemFree(text);return t;}
int main(){
 DYNAMIC_TIME_ZONE_INFORMATION japan{};japan.Bias=-540; wcscpy_s(japan.TimeZoneKeyName,L"Tokyo Standard Time");
 auto a=Token(Ft(2026,11,8,15),japan,L"2026-11-09");
 auto b=Token(Ft(2026,11,9,14,59,59),japan,L"2026-11-09");
 auto c=Token(Ft(2026,11,9,15),japan,L"2026-11-10");
 Check(CompareFileTime(&a,&b)==0,"same local day equality");
 Check(CompareFileTime(&b,&c)<0,"chronological ordering");
 Token(Ft(2026,12,31,15),japan,L"2027-01-01");
 Token(Ft(2024,2,28,15),japan,L"2024-02-29");
 Token(Ft(2024,2,29,15),japan,L"2024-03-01");
 DYNAMIC_TIME_ZONE_INFORMATION west{};west.Bias=480; wcscpy_s(west.TimeZoneKeyName,L"Pacific Standard Time");
 Token(Ft(2026,1,1,7,59),west,L"2025-12-31");
 Token(Ft(2026,1,1,8),west,L"2026-01-01");
 // Explicit recurring US-style rules; no system time-zone changes.
 west.DaylightBias=-60;west.DaylightDate={0,3,0,2,2,0,0,0};west.StandardDate={0,11,0,1,2,0,0,0};
 auto d=Token(Ft(2026,3,8,9,59),west,L"2026-03-08");
 auto e=Token(Ft(2026,3,8,10,1),west,L"2026-03-08");
 Check(CompareFileTime(&d,&e)==0,"spring-forward day remains one group");
 d=Token(Ft(2026,11,1,8,30),west,L"2026-11-01");
 e=Token(Ft(2026,11,1,9,30),west,L"2026-11-01");
 Check(CompareFileTime(&d,&e)==0,"fall-back repeated hour remains one group");
 FILETIME invalid{0xffffffff,0xffffffff},out{};
 Check(!daily::MakeDateToken(invalid,japan,&out),"invalid FILETIME rejected");
 Check(!daily::MakeDateToken(a,japan,nullptr),"null output rejected");
 Check(daily::FormatToken(a,nullptr)==E_POINTER,"null text rejected");
 PWSTR text=reinterpret_cast<PWSTR>(1);
 Check(FAILED(daily::FormatToken(invalid,&text)) && text==nullptr,"failure clears output");
 printf("PASS: %d assertions. Date core only; Explorer hooks/UI NOT tested.\n",checks);
}
