#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <strings.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#define WINDOW_X 800
#define WINDOW_Y 600

/* ---------------------------------- TODO -----------------------------------------
 * <CRITICAL>
 * Convert to real-time calculation                                             DONE
 * Phase control                                                                DONE
 * Optional relative frequencies around note                                    DONE
 * Keyboard input                                                               DONE
 * DEBUG
 *
 * <IMPORTANT>
 * Config GUI
 * Wave visualisation
 *
 * <NICE TO HAVE>
 * Noise wave
 * Multiple carriers
 * Multiple voices
 * Legato
 * Pitchbend
 * Recursive modulation
 * Restricted waveform types
 *
 * <!!!BUGS!!!>
 * Unwanted harmonics at certain frequencies
 */


/*
 * ================================== SYNTHESIS ==================================
 */


#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100
#define BIT_DEPTH 16
#define NUM_OPERATORS 4


typedef enum {
  SINE = 0,
  SQUARE,
  TRIANGLE,
  SAW,
  NONE
} WaveType;

typedef struct Operator {
  WaveType wave;
  bool freq_ratio;
  float freq;
  float amp;
  float phase;
  float pos;
  struct Operator* mods[NUM_OPERATORS - 1];
} Operator;

typedef struct {
  bool enable;
  float freq;
  Operator operators[NUM_OPERATORS];
  int carrier;
} SynthContext;


const int SAMPLE_RANGE = (1<<16) / 2 - 1;

static SynthContext main_ctx = {0};


// Initialise all operators with sensible defaults
void init_operators(SynthContext* ctx) {
  for (int i = 0; i < NUM_OPERATORS; i++) {
    Operator* o = ctx->operators + i;
    o->wave = NONE;
    o->freq_ratio = true;
    o->freq = 1.0f;
    o->amp = 0.5f;
    o->phase = 0.0f;
    o->pos = 0.0f;
    bzero(o->mods, NUM_OPERATORS - 1);
  }
}


// Set the properties of an operator
void set_operator(SynthContext* ctx, int op, WaveType wave, bool freq_ratio, float freq, float amp, float phase, int* conn) {
  Operator* o = ctx->operators + op;
  if (op < 0 || op > NUM_OPERATORS) return;
  o->wave = wave;
  o->freq_ratio = freq_ratio;
  o->freq = freq;
  o->amp = amp;
  o->phase = phase;
  for (int i = 0; i < NUM_OPERATORS; i++) {
    if (i == op || conn[i] == 0) continue;
    o->mods[i] = ctx->operators + i;
  }
}


float gen_wave(Operator* o, float freq_mod) {
  if (o->wave == SINE) {
    return o->amp * sinf(2 * PI * (o->pos + o->phase) + freq_mod);
  }
  else if (o->wave == SQUARE) {
    float p = fmodf(o->pos + o->phase + freq_mod, 1);
    return o->amp * (p < 0.5f ? 1.0f : -1.0f);
  }
  /*
  else if (o->wave == TRIANGLE) {
    table[i] = fabsf(fmodf(progress + 0.75, 1) - 0.5f) * 4 - 1;
  }
  else if (o->wave == SAW) {
    table[i] = (fmodf(progress, 1) * 2 - 1);
  }
  */
  else {
    return 0;
  }
}


// Recursively validate operator configuration
int validate_alg(SynthContext* ctx);


// Recursively calculate the result of an FM algorithm
float process_alg(SynthContext* ctx, int op) {
  Operator* o = ctx->operators + op;
  float mod_result = 0;
  for (int i = 0; i < NUM_OPERATORS - 1; i++) {
    if (o->mods[i] == 0 || i == op) continue;
    mod_result += process_alg(ctx, i);
  }

  if (!ctx->enable) {
    o->pos = 0.0f;
    return 0.0f;
  }

  float op_interval;
  if (o->freq_ratio == true) {
    op_interval = (ctx->freq * o->freq) / SAMPLE_RATE;
  }
  else {
    op_interval = o->freq / SAMPLE_RATE;
  }

  float result = gen_wave(o, mod_result);

  o->pos += op_interval;
  if (o->pos > 1.0f) o->pos -= 1.0f;
  return result;
}


// Process and write the next frames of audio in a stream
void audio_callback(void* data_buf, unsigned int frames) {
  short* d = (short*)data_buf;

  for (int i = 0; i < frames; i++) {
    if (!main_ctx.enable) {
      d[i] = 0;
      continue;
    }
    d[i] = (short)(process_alg(&main_ctx, main_ctx.carrier) * SAMPLE_RANGE);
  }
}


/*
 * ================================== INTERFACE ==================================
 */


typedef enum {
  OPERATORS,
} Page;


static Page cur_page;
static int octave = 4;


void draw_op_page(void) {
  GuiGroupBox((Rectangle){0.0f, 0.0f, WINDOW_X * 0.5f, WINDOW_Y * 0.5f}, "asdf");
  GuiGroupBox((Rectangle){WINDOW_X * 0.5f, 0.0f, WINDOW_X * 0.5f, WINDOW_Y * 0.5f}, "asdf");
};


void process_keyboard(SynthContext* ctx) {
  if (IsKeyPressed(KEY_LEFT_BRACKET)) {
    octave--;
  }
  else if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
    octave++;
  }

  const int keys[13] = {KEY_Z, KEY_S, KEY_X, KEY_D, KEY_C, KEY_V, KEY_G, KEY_B, KEY_H, KEY_N, KEY_J, KEY_M, KEY_COMMA};
  int pressed = -1;

  for (int i = 0; i < 13; i++) {
    pressed = IsKeyDown(keys[i]) ? i : pressed;
  }

  if (pressed == -1) {
    ctx->enable = false;
    return;
  }

  ctx-> enable = true;
  ctx->freq = powf(2, (((float)pressed - 9.0f) / 12.0f) + octave - 4) * 440.0f;
}


/*
 * =================================== UTILITY ===================================
 */


// Clamp a float between two values
float clampf(float f, float a, float b) {
  if (f < a) return a;
  if (f > b) return b;
  return f;
}


int main(void) {
  InitWindow(WINDOW_X, WINDOW_Y, "FM Synth");
  SetTargetFPS(60);
  InitAudioDevice();

  // Initialise synth
  init_operators(&main_ctx);
  main_ctx.carrier = 3;
  main_ctx.freq = 440.0f;
  set_operator(
    &main_ctx,
    3,                  // op #
    SINE,               // waveform
    true,               // freq ratio
    1.0f,             // freq
    0.3f,               // amp
    0.0f,               // phase
    (int[4]){0, 0, 1, 0}// mod connections
  );
  set_operator(
    &main_ctx,
    2,                  // op #
    SINE,               // waveform
    true,               // freq ratio
    2.0f,             // freq
    5.0f,               // amp
    0.0f,               // phase
    (int[4]){0, 0, 0, 0}// mod connections
  );

  // Configure audio stream
  SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
  AudioStream stream = LoadAudioStream(SAMPLE_RATE, BIT_DEPTH, 1);
  SetAudioStreamCallback(stream, audio_callback);

  PlayAudioStream(stream);

  // Initialise display text
  char wave_name[32] = "Flat";
  char freq_label[64] = "440.00 Hz";
  char amp_label[64] = "100%";

  // MAIN LOOP
  while(!WindowShouldClose()) {
    // PROCESS PHASE
    process_keyboard(&main_ctx);

    snprintf(freq_label, 64, "%.2f Hz", main_ctx.freq);
    snprintf(amp_label, 64, "%d%%", (int)(main_ctx.operators[main_ctx.carrier].amp * 100));

    // DRAW PHASE
    BeginDrawing();
    ClearBackground(DARKGRAY);

    DrawText(wave_name, 10, 10, 30, ORANGE);
    DrawText(freq_label, 10, 40, 30, ORANGE);
    DrawText(amp_label, 10, 70, 30, ORANGE);

    EndDrawing();
  };

  CloseWindow();

  return 0;
}
