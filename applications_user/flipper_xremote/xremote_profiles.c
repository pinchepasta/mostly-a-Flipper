#include "xremote_profiles.h"
#include <furi.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#define XREMOTE_PROFILE_HEADER "XRemote Device Profile"
#define XREMOTE_PROFILE_VERSION 1
#define XREMOTE_PROFILE_KEY "device_type"

static const char* const tv[]={
    "Power","Input","Home","Settings","Back",
    "Up","Down","Left","Right","Ok",
    "Vol_up","Vol_dn","Mute","Play_pa"
};

static const char* const sound[]={
    "Power","Mute",
    "AUX","HDMI_ARC","HDMI_IN","Optical","Bluetooth",
    "Vol_up","Vol_dn",
    "Play_pa","Prev","Next",
    "Bass_up","Bass_dn",
    "Treb_up","Treb_dn",
    "EQ","Reset"
};

static const char* const fan[]={
    "Power","Oscillate","Timer","Speed","Night_Mode"
};

static const char* const heater[]={
    "Power","Heat_High_Low","Oscillate","Timer","Temp_up","Temp_dn"
};
static const char* const custom[]={"Button_1","Button_2","Button_3","Button_4","Button_5","Button_6","Button_7","Button_8","Button_9","Button_10","Button_11","Button_12"};

const char* xremote_profile_get_name(XRemoteDeviceProfile p) {
    switch(p) {
    case XRemoteDeviceProfileTV: return "TV";
    case XRemoteDeviceProfileSoundSystem: return "Sound System";
    case XRemoteDeviceProfileFan: return "Fan";
    case XRemoteDeviceProfileHeater: return "Heater";
    case XRemoteDeviceProfileCustom: return "Custom";
    default: return "General";
    }
}
static const char* const* list(XRemoteDeviceProfile p, uint8_t* n) {
    switch(p) {
    case XRemoteDeviceProfileTV: *n=COUNT_OF(tv); return tv;
    case XRemoteDeviceProfileSoundSystem: *n=COUNT_OF(sound); return sound;
    case XRemoteDeviceProfileFan: *n=COUNT_OF(fan); return fan;
    case XRemoteDeviceProfileHeater: *n=COUNT_OF(heater); return heater;
    case XRemoteDeviceProfileCustom: *n=COUNT_OF(custom); return custom;
    default: *n=0; return NULL;
    }
}
uint8_t xremote_profile_button_count(XRemoteDeviceProfile p) { uint8_t n=0; list(p,&n); return n; }
const char* xremote_profile_button_name(XRemoteDeviceProfile p,uint8_t i) { uint8_t n=0; const char* const* a=list(p,&n); return (a && i<n)?a[i]:NULL; }

static void make_path(const char* ir,char* out,size_t size){snprintf(out,size,"%s.xrp",ir);}
bool xremote_profile_store(const char* ir,XRemoteDeviceProfile p) {
    if(!ir) return false;
    char path[256]; make_path(ir,path,sizeof(path));
    Storage* storage=furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff=flipper_format_file_alloc(storage);
    uint32_t val=(uint32_t)p;
    bool ok=flipper_format_file_open_always(ff,path) &&
        flipper_format_write_header_cstr(ff,XREMOTE_PROFILE_HEADER,XREMOTE_PROFILE_VERSION) &&
        flipper_format_write_uint32(ff,XREMOTE_PROFILE_KEY,&val,1);
    flipper_format_free(ff); furi_record_close(RECORD_STORAGE); return ok;
}
bool xremote_profile_load(const char* ir,XRemoteDeviceProfile* p) {
    if(!ir || !p) return false;
    *p = XRemoteDeviceProfileGeneral;
    char path[256]; make_path(ir,path,sizeof(path));
    Storage* storage=furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff=flipper_format_buffered_file_alloc(storage);
    FuriString* h=furi_string_alloc(); uint32_t v=0,ver=0;
    bool ok=flipper_format_buffered_file_open_existing(ff,path) &&
        flipper_format_read_header(ff,h,&ver) &&
        furi_string_equal_str(h,XREMOTE_PROFILE_HEADER) &&
        ver==XREMOTE_PROFILE_VERSION &&
        flipper_format_read_uint32(ff,XREMOTE_PROFILE_KEY,&v,1) &&
        v<=XRemoteDeviceProfileCustom;
    if(ok)*p=(XRemoteDeviceProfile)v;
    furi_string_free(h);flipper_format_free(ff);furi_record_close(RECORD_STORAGE);return ok;
}
