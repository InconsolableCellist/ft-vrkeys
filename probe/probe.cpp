// ft-vrkeys feasibility probe: reads both controllers' sticks, triggers, grips, buttons and
// tip poses at once through a global-priority action set, and prints what changes.
// Usage: probe [seconds] [--low]   (--low: normal overlay priority instead of global)
#include <openvr.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

int main(int argc, char **argv) {
    double secs = argc > 1 ? std::atof(argv[1]) : 30;
    bool low = argc > 2 && !std::strcmp(argv[2], "--low");
    bool top = argc > 2 && !std::strcmp(argv[2], "--max");
    vr::EVRInitError err;
    vr::VR_Init(&err, vr::VRApplication_Overlay);
    if (err) { std::printf("VR_Init: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(err)); return 1; }
    char exe[4096]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1); exe[n > 0 ? n : 0] = 0;
    std::string dir(exe); dir = dir.substr(0, dir.rfind('/'));
    auto in = vr::VRInput();
    auto e = in->SetActionManifestPath((dir + "/actions/actions.json").c_str());
    std::printf("manifest: %d   global setting: %d\n", int(e),
                vr::VRSettings()->GetBool("steamvr", "globalActionSetPriority", nullptr));
    vr::VRActionSetHandle_t set; in->GetActionSetHandle("/actions/keys", &set);
    const char *dig[] = {"lstickclick", "lsticktouch", "lbumper", "rstickclick", "rsticktouch", "rbumper",
                         "a", "b", "x", "y", "dpad_up", "dpad_down", "dpad_left", "dpad_right"};
    const char *an1[] = {"ltrigger", "lgrip", "rtrigger", "rgrip"};
    const char *an2[] = {"lstick", "rstick"};
    vr::VRActionHandle_t hd[14], h1[4], h2[2], pose[2], hap[2];
    for (int i = 0; i < 14; ++i) in->GetActionHandle(("/actions/keys/in/" + std::string(dig[i])).c_str(), &hd[i]);
    for (int i = 0; i < 4; ++i) in->GetActionHandle(("/actions/keys/in/" + std::string(an1[i])).c_str(), &h1[i]);
    for (int i = 0; i < 2; ++i) in->GetActionHandle(("/actions/keys/in/" + std::string(an2[i])).c_str(), &h2[i]);
    in->GetActionHandle("/actions/keys/in/lpose", &pose[0]); in->GetActionHandle("/actions/keys/in/rpose", &pose[1]);
    in->GetActionHandle("/actions/keys/out/lhaptic", &hap[0]); in->GetActionHandle("/actions/keys/out/rhaptic", &hap[1]);

    bool dPrev[14] = {}; int aPrev[4] = {}; int sPrev[2] = {-2, -2}; int act = -1;
    auto start = Clock::now(), lastPose = start;
    while (std::chrono::duration<double>(Clock::now() - start).count() < secs) {
        vr::VREvent_t ev; while (vr::VRSystem()->PollNextEvent(&ev, sizeof ev)) {}
        vr::VRActiveActionSet_t a{}; a.ulActionSet = set;
        a.nPriority = low ? 0 : top ? vr::k_nActionSetOverlayGlobalPriorityMax : vr::k_nActionSetOverlayGlobalPriorityMin + 0x200;
        auto u = in->UpdateActionState(&a, sizeof a, 1);
        double t = std::chrono::duration<double>(Clock::now() - start).count();
        int anyActive = 0;
        for (int i = 0; i < 14; ++i) {
            vr::InputDigitalActionData_t d{}; in->GetDigitalActionData(hd[i], &d, sizeof d, vr::k_ulInvalidInputValueHandle);
            anyActive |= d.bActive;
            if (d.bState != dPrev[i]) { dPrev[i] = d.bState; std::printf("%6.2f %s %s\n", t, dig[i], d.bState ? "down" : "up");
                if (d.bState && (!std::strcmp(dig[i], "a") || !std::strcmp(dig[i], "x")))
                    in->TriggerHapticVibrationAction(hap[1], 0, 0.03f, 200, 0.6f, vr::k_ulInvalidInputValueHandle); }
        }
        for (int i = 0; i < 4; ++i) {
            vr::InputAnalogActionData_t d{}; in->GetAnalogActionData(h1[i], &d, sizeof d, vr::k_ulInvalidInputValueHandle);
            anyActive |= d.bActive;
            int q = int(d.x * 4 + .5);  // quarters
            if (q != aPrev[i]) { aPrev[i] = q; std::printf("%6.2f %s %.2f\n", t, an1[i], d.x); }
        }
        for (int i = 0; i < 2; ++i) {
            vr::InputAnalogActionData_t d{}; in->GetAnalogActionData(h2[i], &d, sizeof d, vr::k_ulInvalidInputValueHandle);
            anyActive |= d.bActive;
            double r = std::hypot(d.x, d.y);
            int dir = r < 0.5 ? -1 : (int(std::lround(std::atan2(d.y, d.x) / (M_PI / 4))) + 8) % 8;  // 0=E, 2=N
            if (dir != sPrev[i]) { sPrev[i] = dir; std::printf("%6.2f %s dir %d (%.2f,%.2f)\n", t, an2[i], dir, d.x, d.y); }
        }
        if (anyActive != act) { act = anyActive; std::printf("%6.2f update=%d actions active=%d\n", t, int(u), act); }
        if (Clock::now() - lastPose > std::chrono::seconds(3)) {
            lastPose = Clock::now();
            std::printf("%6.2f hmd activity=%d dashboard=%d sceneFocus=%d\n", t,
                        int(vr::VRSystem()->GetTrackedDeviceActivityLevel(0)), vr::VROverlay()->IsDashboardVisible(),
                        vr::VRSystem()->IsInputAvailable());
            for (int d = 1; d < 8; ++d) if (vr::VRSystem()->GetTrackedDeviceClass(d) == vr::TrackedDeviceClass_Controller)
                std::printf("       dev %d controller connected=%d activity=%d\n", d, vr::VRSystem()->IsTrackedDeviceConnected(d),
                            int(vr::VRSystem()->GetTrackedDeviceActivityLevel(d)));
            for (int i = 0; i < 2; ++i) {
                vr::InputPoseActionData_t p{};
                in->GetPoseActionDataForNextFrame(pose[i], vr::TrackingUniverseStanding, &p, sizeof p, vr::k_ulInvalidInputValueHandle);
                auto &m = p.pose.mDeviceToAbsoluteTracking.m;
                std::printf("%6.2f %spose active=%d valid=%d pos=(%.2f %.2f %.2f) aim=(%.2f %.2f %.2f)\n", t, i ? "r" : "l",
                            p.bActive, p.pose.bPoseIsValid, m[0][3], m[1][3], m[2][3], -m[0][2], -m[1][2], -m[2][2]);
            }
        }
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }
    vr::VR_Shutdown();
}
