// Standalone driver: exercises the repo's own steamutils.cc through its public
// interop entry point (SteamUtils_Invoke) with a stub Steamworks + dictionary.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>

#include "steam_api.h"
#include "interoplib.h"
#include "interopstub.h"
#include "dictionaryi.h"
#include "steamapi.h"
#include "steamutils.h"

static const int kAvatarW = 184;
static const int kAvatarH = 184;

class FakeUtils : public ISteamUtils {
  public:
    uint32 GetSecondsSinceAppActive() override { return 0; }
    uint32 GetSecondsSinceComputerActive() override { return 0; }
    EUniverse GetConnectedUniverse() override { return k_EUniversePublic; }
    uint32 GetServerRealTime() override { return 0; }
    const char *GetIPCountry() override { return "US"; }
    bool GetImageSize(int iImage, uint32 *pnWidth, uint32 *pnHeight) override {
        *pnWidth = kAvatarW;
        *pnHeight = kAvatarH;
        return true;
    }
    bool GetImageRGBA(int iImage, uint8 *pubDest, int nDestBufferSize) override {
        memset(pubDest, 0xAB, nDestBufferSize);
        return true;
    }
    bool GetCSERIPPort(uint32 *unIP, uint16 *usPort) override { return false; }
    uint8 GetCurrentBatteryPower() override { return 255; }
    uint32 GetAppID() override { return 480; }
    void SetOverlayNotificationPosition(ENotificationPosition p) override {}
    bool IsAPICallCompleted(SteamAPICall_t h, bool *pbFailed) override { return false; }
    ESteamAPICallFailure GetAPICallFailureReason(SteamAPICall_t h) override { return k_ESteamAPICallFailureNone; }
    bool GetAPICallResult(SteamAPICall_t h, void *pCallback, int cub, int iExpected, bool *pbFailed) override {
        return false;
    }
    void RunFrame() override {}
    uint32 GetIPCCallCount() override { return 0; }
    void SetWarningMessageHook(SteamAPIWarningMessageHook_t f) override {}
    bool IsOverlayEnabled() override { return false; }
    bool BOverlayNeedsPresent() override { return false; }
    SteamAPICall_t CheckFileSignature(const char *sz) override { return 0; }
    bool ShowGamepadTextInput(EGamepadTextInputMode m, EGamepadTextInputLineMode l, const char *d, uint32 c,
                              const char *e) override {
        return false;
    }
    uint32 GetEnteredGamepadTextLength() override { return 0; }
    bool GetEnteredGamepadTextInput(char *pchText, uint32 cchText) override { return false; }
    const char *GetSteamUILanguage() override { return "english"; }
    bool IsSteamRunningInVR() override { return false; }
    void SetOverlayNotificationInset(int h, int v) override {}
    bool IsSteamInBigPictureMode() override { return false; }
    void StartVRDashboard() override {}
    bool IsVRHeadsetStreamingEnabled() override { return false; }
    void SetVRHeadsetStreamingEnabled(bool b) override {}
};

static FakeUtils g_fake_utils;
static void *g_context[64];

extern "C" void *SteamInternal_ContextInit(void *p) {
    g_context[3] = &g_fake_utils;  // CSteamAPIContext::m_pSteamUtils
    return g_context;
}
extern "C" void *SteamInternal_CreateInterface(const char *ver) { return nullptr; }
extern "C" HSteamPipe SteamAPI_GetHSteamPipe() { return 0; }
extern "C" HSteamUser SteamAPI_GetHSteamUser() { return 0; }

bool SteamAPI_IsInitialized() { return true; }

/*********************************************************************/
// Minimal IDictionary stub: "method" -> getImageRGBA, "index" -> 0.

struct FakeDict {
    ClassStruct Class;
    IDictionaryVtbl *vtbl;
};

static bool fake_get_string_ptr_by_key(echandle handle, const char *key, const char **out) {
    if (strcmp(key, "method") == 0) {
        *out = "getImageRGBA";
        return true;
    }
    return false;
}
static bool fake_get_int32_by_key(echandle handle, const char *key, int32_t *out) {
    if (strcmp(key, "index") == 0) {
        *out = 0;
        return true;
    }
    return false;
}
static int32_t g_returned_len = 0;
static unsigned long g_returned_hash = 0;
static bool fake_add_string(echandle handle, const char *key, const char *value, echandle *item) {
    g_returned_len = value ? (int32_t)strlen(value) : -1;
    g_returned_hash = 5381;
    for (const char *c = value; c && *c; c += 1)
        g_returned_hash = g_returned_hash * 33 + (unsigned char)*c;
    return true;
}
static bool fake_add_null(echandle handle, const char *key, echandle *item) { return true; }

static size_t resident_bytes() {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
        return 0;
    return info.resident_size;
}

int main(int argc, char **argv) {
    const int iterations = (argc > 1) ? atoi(argv[1]) : 1000;

    IDictionaryVtbl vtbl;
    memset(&vtbl, 0, sizeof(vtbl));
    vtbl.get_string_ptr_by_key = fake_get_string_ptr_by_key;
    vtbl.get_int32_by_key = fake_get_int32_by_key;
    vtbl.add_string = fake_add_string;
    vtbl.add_null = fake_add_null;

    FakeDict method_dict{};
    FakeDict return_dict{};
    method_dict.vtbl = &vtbl;
    return_dict.vtbl = &vtbl;

    SteamUtils_Invoke(nullptr, &method_dict, &return_dict);  // warm up
    const size_t before = resident_bytes();

    for (int i = 0; i < iterations; i += 1)
        SteamUtils_Invoke(nullptr, &method_dict, &return_dict);

    const size_t after = resident_bytes();
    printf("getImageRGBA calls : %d (avatar %dx%d, %d bytes of pixels each)\n", iterations, kAvatarW, kAvatarH,
           4 * kAvatarW * kAvatarH);
    printf("base64 returned    : %d chars, hash %lu\n", g_returned_len, g_returned_hash);
    printf("resident before    : %zu KB\n", before / 1024);
    printf("resident after     : %zu KB\n", after / 1024);
    printf("growth             : %zd KB\n", ((ssize_t)after - (ssize_t)before) / 1024);
    return 0;
}
