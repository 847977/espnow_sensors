#include <Arduino.h>
#include <math.h>

// ==========================
// Hardware config
// ==========================
constexpr bool COMMON_ANODE = true;

// One button for palette switching
constexpr int BTN_PALETTE_PIN = 19;

// Left RGB module pins
constexpr int L_R_PIN = 25;
constexpr int L_G_PIN = 26;
constexpr int L_B_PIN = 27;

// Right RGB module pins
constexpr int R_R_PIN = 32;
constexpr int R_G_PIN = 33;
constexpr int R_B_PIN = 14;

// PWM setup
constexpr int PWM_FREQ = 5000;
constexpr int PWM_RES_BITS = 8;

constexpr int CH_L_R = 0;
constexpr int CH_L_G = 1;
constexpr int CH_L_B = 2;
constexpr int CH_R_R = 3;
constexpr int CH_R_G = 4;
constexpr int CH_R_B = 5;

// ==========================
// App config
// ==========================
enum PaletteId
{
    PAL_SUNSET = 0,
    PAL_OCEAN = 1,
    PAL_AURORA_SOFT = 2,
    PAL_FOREST_WARM = 3,
    PAL_FIRELIGHT = 4,
    PAL_COUNT
};

PaletteId currentPalette = PAL_SUNSET;

struct ButtonState
{
    int pin;
    bool lastReading;
    bool stableState;
    unsigned long lastDebounceMs;

    ButtonState(int p)
        : pin(p), lastReading(true), stableState(true), lastDebounceMs(0)
    {
    }
};

ButtonState btnPalette(BTN_PALETTE_PIN);

struct Vec3
{
    float r;
    float g;
    float b;
};

struct AudioFeatures
{
    float rms;
    float bass;
    float mid;
    float treble;
};

struct SimState
{
    float rms = 0.25f;
    float bass = 0.2f;
    float mid = 0.2f;
    float treble = 0.2f;
};

SimState simLeft;
SimState simRight;

unsigned long lastSimUpdate = 0;
constexpr unsigned long SIM_INTERVAL_MS = 35;

// ==========================
// Utilities
// ==========================
float clamp01(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

Vec3 lerpColor(const Vec3& a, const Vec3& b, float t)
{
    return {
        lerp(a.r, b.r, t),
        lerp(a.g, b.g, t),
        lerp(a.b, b.b, t)
    };
}

float gammaCorrect(float x)
{
    x = clamp01(x);
    return powf(x, 2.2f);
}

float shaped(float x)
{
    x = clamp01(x);
    return powf(x, 0.65f);
}

float rand01()
{
    return (float)random(0, 10001) / 10000.0f;
}

float driftValue(float current, float amount, float minV, float maxV)
{
    float step = (rand01() * 2.0f - 1.0f) * amount;
    current += step;

    if (current < minV) current = minV;
    if (current > maxV) current = maxV;

    return current;
}

// ==========================
// Palettes
// ==========================
void getPaletteColors(PaletteId p, Vec3& bassColor, Vec3& midColor, Vec3& trebleColor)
{
    if (p == PAL_SUNSET)
    {
        bassColor   = {1.00f, 0.42f, 0.05f}; // amber
        midColor    = {1.00f, 0.25f, 0.18f}; // orange/rose
        trebleColor = {1.00f, 0.72f, 0.55f}; // warm white
    }
    else if (p == PAL_OCEAN)
    {
        bassColor   = {0.02f, 0.18f, 0.75f}; // deep blue
        midColor    = {0.00f, 0.55f, 0.75f}; // cyan-blue
        trebleColor = {0.35f, 0.90f, 0.85f}; // teal/ice
    }
    else if (p == PAL_AURORA_SOFT)
    {
        bassColor   = {0.35f, 0.05f, 0.85f}; // violet
        midColor    = {0.75f, 0.10f, 0.75f}; // magenta
        trebleColor = {1.00f, 0.45f, 0.75f}; // pink
    }
    else if (p == PAL_FOREST_WARM)
    {
        bassColor   = {0.00f, 0.38f, 0.22f}; // emerald
        midColor    = {0.00f, 0.62f, 0.45f}; // teal-green
        trebleColor = {0.55f, 0.95f, 0.72f}; // mint
    }
    else // PAL_FIRELIGHT
    {
        bassColor   = {1.00f, 0.12f, 0.02f}; // red-orange
        midColor    = {1.00f, 0.45f, 0.02f}; // amber
        trebleColor = {1.00f, 0.78f, 0.30f}; // gold
    }
}

const char* paletteName(PaletteId p)
{
    switch (p)
    {
        case PAL_SUNSET:      return "Sunset";
        case PAL_OCEAN:       return "Ocean";
        case PAL_AURORA_SOFT: return "Aurora Soft";
        case PAL_FOREST_WARM: return "Forest Warm";
        case PAL_FIRELIGHT:   return "Firelight";
        default:              return "?";
    }
}

// ==========================
// Button handling
// ==========================
bool buttonPressed(ButtonState& b)
{
    bool reading = digitalRead(b.pin);

    if (reading != b.lastReading)
    {
        b.lastDebounceMs = millis();
    }

    b.lastReading = reading;

    if ((millis() - b.lastDebounceMs) > 30)
    {
        if (reading != b.stableState)
        {
            b.stableState = reading;

            if (b.stableState == LOW)
            {
                return true;
            }
        }
    }

    return false;
}

// ==========================
// PWM output
// ==========================
void writeChannel(int channel, float value01)
{
    value01 = clamp01(value01);

    uint8_t pwm = (uint8_t)roundf(value01 * 255.0f);

    if (COMMON_ANODE)
    {
        pwm = 255 - pwm;
    }

    ledcWrite(channel, pwm);
}

void setLeftColor(const Vec3& c)
{
    writeChannel(CH_L_R, c.r);
    writeChannel(CH_L_G, c.g);
    writeChannel(CH_L_B, c.b);
}

void setRightColor(const Vec3& c)
{
    writeChannel(CH_R_R, c.r);
    writeChannel(CH_R_G, c.g);
    writeChannel(CH_R_B, c.b);
}

// ==========================
// Simulated audio
// ==========================
void updateSimulatedAudio(SimState& s)
{
    // základný pomalý pohyb
    s.rms    = driftValue(s.rms,    0.035f, 0.05f, 0.95f);
    s.bass   = driftValue(s.bass,   0.050f, 0.00f, 1.00f);
    s.mid    = driftValue(s.mid,    0.045f, 0.00f, 1.00f);
    s.treble = driftValue(s.treble, 0.060f, 0.00f, 1.00f);

    // výrazné basové impulzy
    if (random(0, 100) < 18)
    {
        s.bass = clamp01(s.bass + lerp(0.25f, 0.60f, rand01()));
        s.rms  = clamp01(s.rms  + lerp(0.10f, 0.30f, rand01()));
    }

    // stredy - pomalšie, stabilnejšie zmeny
    if (random(0, 100) < 10)
    {
        s.mid = clamp01(s.mid + lerp(0.18f, 0.40f, rand01()));
    }

    // výšky - krátke špičky
    if (random(0, 100) < 22)
    {
        s.treble = clamp01(s.treble + lerp(0.25f, 0.55f, rand01()));
    }

    // prirodzený útlm, aby špičky nezostávali visieť
    s.bass   *= 0.96f;
    s.mid    *= 0.985f;
    s.treble *= 0.93f;

    // RMS nech je čiastočne naviazané na celkovú energiu
    float spectralEnergy = 0.45f * s.bass + 0.35f * s.mid + 0.20f * s.treble;
    s.rms = clamp01(0.70f * s.rms + 0.30f * spectralEnergy);
}

AudioFeatures getSimulatedFeatures(SimState& s)
{
    return {
        shaped(s.rms),
        shaped(s.bass),
        shaped(s.mid),
        shaped(s.treble)
    };
}

// ==========================
// Color engine
// ==========================
Vec3 makeSpectrumColor(AudioFeatures a, PaletteId palette)
{
    Vec3 bassColor;
    Vec3 midColor;
    Vec3 trebleColor;

    getPaletteColors(palette, bassColor, midColor, trebleColor);

    float bass   = clamp01(a.bass);
    float mid    = clamp01(a.mid);
    float treble = clamp01(a.treble);

    // aby farba nebola čierna, keď sú všetky pásma slabé
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

    // výšky pridajú jemný lesk / bielu prímes
    float whiteMix = 0.08f + 0.28f * treble;
    c = lerpColor(c, Vec3{1.0f, 1.0f, 1.0f}, whiteMix);

    return c;
}

Vec3 applyBrightness(const Vec3& baseColor, float rms)
{
    // RMS riadi jas
    // minimum nech nie je úplná tma, maximum nech nie je rušivé
    float brightness = lerp(0.04f, 0.70f, clamp01(rms));

    Vec3 out;

    out.r = gammaCorrect(baseColor.r * brightness);
    out.g = gammaCorrect(baseColor.g * brightness);
    out.b = gammaCorrect(baseColor.b * brightness);

    return out;
}

// ==========================
// Setup / loop
// ==========================
void setupPwm()
{
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

void setup()
{
    Serial.begin(115200);
    delay(300);

    randomSeed((uint32_t)esp_random());

    pinMode(BTN_PALETTE_PIN, INPUT_PULLUP);

    setupPwm();

    setLeftColor({0, 0, 0});
    setRightColor({0, 0, 0});

    Serial.println();
    Serial.println("=== Ambient RGB Spectrum Test ===");
    Serial.printf("Palette: %s\n", paletteName(currentPalette));
}

void loop()
{
    if (buttonPressed(btnPalette))
    {
        currentPalette = (PaletteId)((currentPalette + 1) % PAL_COUNT);
        Serial.printf("Palette changed -> %s\n", paletteName(currentPalette));
    }

    if (millis() - lastSimUpdate >= SIM_INTERVAL_MS)
    {
        lastSimUpdate = millis();

        updateSimulatedAudio(simLeft);
        updateSimulatedAudio(simRight);

        AudioFeatures left  = getSimulatedFeatures(simLeft);
        AudioFeatures right = getSimulatedFeatures(simRight);

        // spoločné spektrum pre farbu
        AudioFeatures shared;
        shared.rms    = 0.5f * (left.rms + right.rms);
        shared.bass   = 0.5f * (left.bass + right.bass);
        shared.mid    = 0.5f * (left.mid + right.mid);
        shared.treble = 0.5f * (left.treble + right.treble);

        // farba podľa basov/stredov/výšok
        Vec3 baseColor = makeSpectrumColor(shared, currentPalette);

        // jas podľa RMS zvlášť pre ľavý a pravý kanál
        Vec3 leftColor  = applyBrightness(baseColor, left.rms);
        Vec3 rightColor = applyBrightness(baseColor, right.rms);

        setLeftColor(leftColor);
        setRightColor(rightColor);

        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 500)
        {
            lastPrint = millis();

            Serial.printf(
                "Palette=%s | "
                "bass=%.2f mid=%.2f treble=%.2f | "
                "L_rms=%.2f R_rms=%.2f\n",
                paletteName(currentPalette),
                shared.bass,
                shared.mid,
                shared.treble,
                left.rms,
                right.rms
            );
        }
    }
}