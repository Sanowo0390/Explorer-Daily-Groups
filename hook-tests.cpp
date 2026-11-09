// Tests hook logic by direct calls with test doubles. Does NOT install hooks.
#include <windhawk_api.h>
#include "explorer-daily-groups.wh.cpp"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
int checks=0;
void Check(bool ok,const char* name){++checks;if(!ok){printf("FAIL %s\n",name);exit(1);}}
PROPERTYKEY testKey=PKEY_DateModified;
FILETIME inputTime{};
PROPVARIANT captured{};
PWSTR capturedText=nullptr;
ComPtr<IPropertyDescription> description;
int itemIdentity;
HRESULT WINAPI KeyMock(void*,PROPERTYKEY* key){*key=testKey;return S_OK;}
HRESULT WINAPI ValueMock(void*,REFPROPERTYKEY,PROPVARIANT* value){*value={};value->vt=VT_FILETIME;value->filetime=inputTime;return S_OK;}
HRESULT WINAPI CategoryMock(void*,void* item,IShellFolder2*,PROPVARIANT*){
 Check(ValueHook(item,testKey,&captured)==S_OK,"value path");
 return FormatHook(description.Get(),captured,PDFF_DEFAULT,&capturedText);
}
int main(){
 Check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM init");
 Check(SUCCEEDED(PSGetPropertyDescription(PKEY_DateModified,IID_PPV_ARGS(&description))),"property description");
 auto v=*reinterpret_cast<void***>(description.Get());g_group=reinterpret_cast<GroupFn>(v[13]);g_format=reinterpret_cast<FormatFn>(v[22]);
 g_modified=true;g_created=false;g_key=KeyMock;g_value=ValueMock;g_category=CategoryMock;
 g_zone.Bias=-540;wcscpy_s(g_zone.TimeZoneKeyName,L"Tokyo Standard Time");
 SYSTEMTIME st{};st.wYear=2026;st.wMonth=11;st.wDay=8;st.wHour=15;st.wMinute=12;
 Check(SystemTimeToFileTime(&st,&inputTime)!=FALSE,"input time");
 PROPDESC_GROUPING_RANGE native{},changed{};
 Check(description->GetGroupingRange(&native)==S_OK && native==PDGR_DATE,"native date grouping");
 Check(GroupHook(description.Get(),&changed)==S_OK && changed==PDGR_DISCRETE,"modified opt-in");
 ComPtr<IPropertyDescription> created;PSGetPropertyDescription(PKEY_DateCreated,IID_PPV_ARGS(&created));
 Check(created && GroupHook(created.Get(),&changed)==S_OK && changed==PDGR_DATE,"created default off");
 g_created=true;Check(GroupHook(created.Get(),&changed)==S_OK && changed==PDGR_DISCRETE,"created opt-in");g_created=false;
 PROPVARIANT value{};
 Check(ValueHook(&itemIdentity,testKey,&value)==S_OK && CompareFileTime(&value.filetime,&inputTime)==0,"ordinary value untouched");
 Check(CategoryHook(nullptr,&itemIdentity,nullptr,&value)==S_OK,"classification scope");
 Check(capturedText && wcscmp(capturedText,L"2026-11-09")==0,"date-only heading");CoTaskMemFree(capturedText);capturedText=nullptr;
 Check(CompareFileTime(&captured.filetime,&inputTime)!=0,"classification copy normalized");
 Check(g_context==nullptr,"scope restored");
 Context outer{&itemIdentity,PKEY_DateModified};
 {Scope scope(&outer);int other;
  Check(ValueHook(&other,testKey,&value)==S_OK && CompareFileTime(&value.filetime,&inputTime)==0,"different item bypass");
  Check(ValueHook(&itemIdentity,PKEY_DateCreated,&value)==S_OK && CompareFileTime(&value.filetime,&inputTime)==0,"different key bypass");
  {Scope nested(nullptr);Check(g_context==nullptr,"nested unrelated scope clears context");}
  Check(g_context==&outer,"nested context restored");
 }
 g_modified=false;
 Check(GroupHook(description.Get(),&changed)==S_OK && changed==native,"disabled property passes through");
 Check(CategoryHook(nullptr,&itemIdentity,nullptr,&value)==S_OK && CompareFileTime(&captured.filetime,&inputTime)==0,"disabled normalization passes through");
 CoTaskMemFree(capturedText);description.Reset();created.Reset();CoUninitialize();
 printf("PASS: %d assertions. Direct hook calls/test doubles; no Explorer injection.\n",checks);
}
