// receiver_main.cpp
// -----------------------------------------------------------------------------
// Receiver ESP32:
// - prijíma AudioPacket cez ESP-NOW,
// - rozbalí hodnoty RMS/bass/mid/treble pre L a R,
// - vypočíta spoločnú farbu z priemeru spektra,
// - pri tichu alebo vypnutých mikrofónoch svieti stabilnou farbou aktuálnej palety,
// - jas ľavej a pravej LED riadi samostatne podľa L/R RMS,
// - palety sa prepínajú tlačidlom,
// - samostatné tlačidlo zapína/vypína LED výstup.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <math.h>
#include "espnow_rx.h"
#include "packets.h"
#include "board_pins.h"

// ==========================
// Hardware config
// ==========================
constexpr bool COMMON_ANODE = true;

constexpr int BTN_PALETTE_PIN = PIN_BTN_PALETTE;

// Left RGB module pins
constexpr int L_R_PIN = 25;
constexpr int L_G_PIN = 26;
constexpr int L_B_PIN = 27;

// Right RGB module pins
constexpr int R_R_PIN = 32;
constexpr int R_G_PIN = 33;
constexpr int R_B_PIN = 14;

constexpr int PWM_FREQ = 5000;
constexpr int PWM_RES_BITS = 8;

constexpr int CH_L_R = 0;
constexpr int CH_L_G = 1;
constexpr int CH_L_B = 2;
constexpr int CH_R_R = 3;
constexpr int CH_R_G = 4;
constexpr int CH_R_B = 5;

// ==========================
// App state
// ==========================
static bool g_ledEnabled = true;
static bool g_micEnabledRemote = true;

constexpr float QUIET_RMS_THRESHOLD = 0.08f;
constexpr float IDLE_BRIGHTNESS = 0.30f;

// ==========================
// Types
// ==========================
enum PaletteId {
    PAL_SUNSET = 0,
    PAL_OCEAN,
    PAL_AURORA_SOFT,
    PAL_FOREST_WARM,
    PAL_FIRELIGHT,
    PAL_COUNT
};

PaletteId currentPalette = PAL_SUNSET;

struct ButtonState {
    int pin;
    bool lastReading;
    bool stableState;
    unsigned long lastDebounceMs;

    ButtonState(int p)
        : pin(p), lastReading(true), stableState(true), lastDebounceMs(0) {}
};

ButtonState btnPalette(BTN_PALETTE_PIN);
ButtonState btnLedEnable(PIN_BTN_LED_ENABLE);

struct Vec3 {
    float r;
    float g;
    float b;
};

struct AudioFeatures {
    float rms;
    float bass;
    float mid;
    float treble;
};

AudioFeatures leftFeatures  = {0, 0, 0, 0};
AudioFeatures rightFeatures = {0, 0, 0, 0};

unsigned long lastPacketMs = 0;

// ==========================
// Utilities
// ==========================
float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec3 lerpColor(const Vec3& a, const Vec3& b, float t) {
    return {
        lerp(a.r, b.r, t),
        lerp(a.g, b.g, t),
        lerp(a.b, b.b, t)
    };
}

float gammaCorrect(float x) {
    x = clamp01(x);
    return powf(x, 2.2f);
}

float shaped(float x) {
    x = clamp01(x);
    return powf(x, 0.65f);
}

// ==========================
// Palettes
// ==========================
void getPaletteColors(PaletteId p, Vec3& bassColor, Vec3& midColor, Vec3& trebleColor) {
    if (p == PAL_SUNSET) {
        bassColor   = {1.00f, 0.42f, 0.05f};
        midColor    = {1.00f, 0.25f, 0.18f};
        trebleColor = {1.00f, 0.72f, 0.55f};
    } else if (p == PAL_OCEAN) {
        bassColor   = {0.02f, 0.18f, 0.75f};
        midColor    = {0.00f, 0.55f, 0.75f};
        trebleColor = {0.35f, 0.90f, 0.85f};
    } else if (p == PAL_AURORA_SOFT) {
        bassColor   = {0.35f, 0.05f, 0.85f};
        midColor    = {0.75f, 0.10f, 0.75f};
        trebleColor = {1.00f, 0.45f, 0.75f};
    } else if (p == PAL_FOREST_WARM) {
        bassColor   = {0.00f, 0.38f, 0.22f};
        midColor    = {0.00f, 0.62f, 0.45f};
        trebleColor = {0.55f, 0.95f, 0.72f};
    } else {
        bassColor   = {1.00f, 0.12f, 0.02f};
        midColor    = {1.00f, 0.45f, 0.02f};
        trebleColor = {1.00f, 0.78f, 0.30f};
    }
}

const char* paletteName(PaletteId p) {
    switch (p) {
        case PAL_SUNSET: return "Sunset";
        case PAL_OCEAN: return "Ocean";
        case PAL_AURORA_SOFT: return "Aurora Soft";
        case PAL_FOREST_WARM: return "Forest Warm";
        case PAL_FIRELIGHT: return "Firelight";
        default: return "?";
    }
}

// ==========================
// Button
// ==========================
bool buttonPressed(ButtonState& b) {
    bool reading = digitalRead(b.pin);

    if (reading != b.lastReading) {
        b.lastDebounceMs = millis();
    }

    b.lastReading = reading;

    if ((millis() - b.lastDebounceMs) > 30) {
        if (reading != b.stableState) {
            b.stableState = reading;
            if (b.stableState == LOW) return true;
        }
    }

    return false;
}

// ==========================
// PWM output
// ==========================
void writeChannel(int channel, float value01) {
    value01 = clamp01(value01);
    uint8_t pwm = (uint8_t)roundf(value01 * 255.0f);

    if (COMMON_ANODE) {
        pwm = 255 - pwm;
    }

    ledcWrite(channel, pwm);
}

void setLeftColor(const Vec3& c) {
    writeChannel(CH_L_R, c.r);
    writeChannel(CH_L_G, c.g);
    writeChannel(CH_L_B, c.b);
}

void setRightColor(const Vec3& c) {
    writeChannel(CH_R_R, c.r);
    writeChannel(CH_R_G, c.g);
    writeChannel(CH_R_B, c.b);
}

// ==========================
// Color engine
// ==========================
Vec3 makeSpectrumColor(AudioFeatures a, PaletteId palette) {
    Vec3 bassColor, midColor, trebleColor;
    getPaletteColors(palette, bassColor, midColor, trebleColor);

    float bass   = clamp01(a.bass);
    float mid    = clamp01(a.mid);
    float treble = clamp01(a.treble);

    float sum = bass + mid + treble + 0.001f;

    float bassWeight   = bass / sum;
    float midWeight    = mid / sum;
    float trebleWeight = treble / sum;

    Vec3 c;
    c.r = bassColor.r   * bassWeight +
          midColor.r    * midWeight +
          trebleColor.r * trebleWeight;

    c.g = bassColor.g   * bassWeight +
          midColor.g    * midWeight +
          trebleColor.g * trebleWeight;

    c.b = bassColor.b   * bassWeight +
          midColor.b    * midWeight +
          trebleColor.b * trebleWeight;

    float whiteMix = 0.08f + 0.28f * treble;
    c = lerpColor(c, Vec3{1.0f, 1.0f, 1.0f}, whiteMix);

    return c;
}

Vec3 makeIdlePaletteColor(PaletteId palette) {
    Vec3 bassColor, midColor, trebleColor;
    getPaletteColors(palette, bassColor, midColor, trebleColor);

    Vec3 c;
    c.r = 0.40f * bassColor.r + 0.40f * midColor.r + 0.20f * trebleColor.r;
    c.g = 0.40f * bassColor.g + 0.40f * midColor.g + 0.20f * trebleColor.g;
    c.b = 0.40f * bassColor.b + 0.40f * midColor.b + 0.20f * trebleColor.b;

    return c;
}

Vec3 applyBrightness(const Vec3& baseColor, float rms) {
    rms = clamp01(rms);

    float shapedRms = powf(rms, 0.55f);
    float brightness = lerp(0.02f, 0.95f, shapedRms);

    Vec3 out;
    out.r = gammaCorrect(baseColor.r * brightness);
    out.g = gammaCorrect(baseColor.g * brightness);
    out.b = gammaCorrect(baseColor.b * brightness);
    return out;
}

Vec3 applyFixedBrightness(const Vec3& baseColor, float brightness) {
    brightness = clamp01(brightness);

    Vec3 out;
    out.r = gammaCorrect(baseColor.r * brightness);
    out.g = gammaCorrect(baseColor.g * brightness);
    out.b = gammaCorrect(baseColor.b * brightness);
    return out;
}

// ==========================
// ESP-NOW data handling
// ==========================
AudioFeatures featuresFromPacketSide(float rms, float bass, float mid, float treble) {
    return {
        shaped(rms),
        shaped(bass),
        shaped(mid),
        shaped(treble)
    };
}

void updateFeaturesFromEspNow() {
    transport::AudioPacket p;

    if (transport::espnow_rx::getLatestPacket(p)) {
        lastPacketMs = millis();

        g_micEnabledRemote = (p.flags & transport::FLAG_MIC_ENABLED);

        leftFeatures = featuresFromPacketSide(
            transport::unpack01(p.lRms),
            transport::unpack01(p.lBass),
            transport::unpack01(p.lMid),
            transport::unpack01(p.lTreble)
        );

        rightFeatures = featuresFromPacketSide(
            transport::unpack01(p.rRms),
            transport::unpack01(p.rBass),
            transport::unpack01(p.rMid),
            transport::unpack01(p.rTreble)
        );
    }

    // Ak dlho neprišiel paket, sprav stav ako ticho.
    if (millis() - lastPacketMs > 500) {
        leftFeatures  = {0, 0, 0, 0};
        rightFeatures = {0, 0, 0, 0};
        g_micEnabledRemote = false;
    }
}

// ==========================
// Setup / loop
// ==========================
void setupPwm() {
    ledcSetup(CH_L_R, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(CH_L_G, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(CH_L_B, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(CH_R_R, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(CH_R_G, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(CH_R_B, PWM_FREQ, PWM_RES_BITS);

    ledcAttachPin(L_R_PIN, CH_L_R);
    ledcAttachPin(L_G_PIN, CH_L_G);
    ledcAttachPin(L_B_PIN, CH_L_B);
    ledcAttachPin(R_R_PIN, CH_R_R);
    ledcAttachPin(R_G_PIN, CH_R_G);
    ledcAttachPin(R_B_PIN, CH_R_B);
}

void setup() {
    Serial.begin(115200);
    delay(300);

    pinMode(BTN_PALETTE_PIN, INPUT_PULLUP);
    pinMode(PIN_BTN_LED_ENABLE, INPUT_PULLUP);

    setupPwm();

    setLeftColor({0, 0, 0});
    setRightColor({0, 0, 0});

    transport::espnow_rx::init();

    Serial.println();
    Serial.println("=== Ambient RGB ESP-NOW Receiver ===");
    Serial.printf("Palette: %s\n", paletteName(currentPalette));
}

void loop() {
    if (buttonPressed(btnPalette)) {
        currentPalette = (PaletteId)((currentPalette + 1) % PAL_COUNT);
        Serial.printf("Palette changed -> %s\n", paletteName(currentPalette));
    }

    if (buttonPressed(btnLedEnable)) {
        g_ledEnabled = !g_ledEnabled;

        if (!g_ledEnabled) {
            setLeftColor({0, 0, 0});
            setRightColor({0, 0, 0});
        }
    }

    updateFeaturesFromEspNow();

    if (!g_ledEnabled) {
        setLeftColor({0, 0, 0});
        setRightColor({0, 0, 0});
        return;
    }

    AudioFeatures shared;
    shared.rms    = 0.5f * (leftFeatures.rms + rightFeatures.rms);
    shared.bass   = 0.5f * (leftFeatures.bass + rightFeatures.bass);
    shared.mid    = 0.5f * (leftFeatures.mid + rightFeatures.mid);
    shared.treble = 0.5f * (leftFeatures.treble + rightFeatures.treble);

    float avgRms = shared.rms;

    // MIC OFF alebo ticho = stabilná farba aktuálnej palety na 30 %
    if (!g_micEnabledRemote || avgRms < QUIET_RMS_THRESHOLD) {
        Vec3 idleBase = makeIdlePaletteColor(currentPalette);
        Vec3 idleColor = applyFixedBrightness(idleBase, IDLE_BRIGHTNESS);

        setLeftColor(idleColor);
        setRightColor(idleColor);
        return;
    }

    // Hudba = farba podľa spektra, jas podľa L/R RMS
    Vec3 baseColor = makeSpectrumColor(shared, currentPalette);

    Vec3 leftColor  = applyBrightness(baseColor, leftFeatures.rms);
    Vec3 rightColor = applyBrightness(baseColor, rightFeatures.rms);

    setLeftColor(leftColor);
    setRightColor(rightColor);

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
        lastPrint = millis();

        Serial.printf(
            "palette=%s | bass=%.2f mid=%.2f treble=%.2f | L_rms=%.2f R_rms=%.2f | mic=%s led=%s\n",
            paletteName(currentPalette),
            shared.bass,
            shared.mid,
            shared.treble,
            leftFeatures.rms,
            rightFeatures.rms,
            g_micEnabledRemote ? "ON" : "OFF",
            g_ledEnabled ? "ON" : "OFF"
        );
    }
}