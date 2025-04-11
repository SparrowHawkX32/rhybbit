#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WINDOW_X 800
#define WINDOW_Y 600

#define WAVETABLE_SIZE 2048
#define BUFFER_SIZE 4096
#define SAMPLE_RATE 48000
#define BIT_DEPTH 16
#define NUM_OPERATORS 4

typedef enum {
  NONE,
  SINE,
  SQUARE,
  TRIANGLE,
  SAW
} WaveType;

typedef struct Operator{
  float** wavetable;
  float freq;
  float amp;
  struct Operator* mods[NUM_OPERATORS - 1];
} Operator;

const int SAMPLE_RANGE = (1<<16) / 2 - 1;

float freq = 440.0f;
float amp = 0.5f;
int wave_pos = 0;

float* empty_table = 0;
float* sine_table = 0;
float* square_table = 0;
float* triangle_table = 0;
float* saw_table = 0;

float** cur_table = 0;

Operator operators[NUM_OPERATORS];


// Allocate a wavetable of size WAVETABLE_SIZE and render a wave to it
float* gen_wavetable(WaveType wave) {
  float* table = malloc(sizeof(float) * WAVETABLE_SIZE);

  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    float progress = (float)i / WAVETABLE_SIZE;

    if (wave == SINE) {
      table[i] = sinf(2.0f * PI * progress);
    }
    else if (wave == SQUARE) {
      if (i >= WAVETABLE_SIZE / 2) {
        table[i] = -1;
        continue;
      }
      table[i] = 1;
    }
    else if (wave == TRIANGLE) {
      table[i] = fabsf(fmodf(progress + 0.75, 1) - 0.5f) * 4 - 1;
    }
    else if (wave == SAW) {
      table[i] = (fmodf(progress, 1) * 2 - 1);
    }
    else {
      table[i] = 0;
    }
  }
  return table;
}


// Initialise all operators with sensible defaults
void init_operators() {
  for (int i = 0; i < NUM_OPERATORS; i++) {
    operators[i].wavetable = &empty_table;
    operators[i].freq = 440.0f;
    operators[i].amp = 0.5f;
    bzero(operators[i].mods, NUM_OPERATORS - 1);
  }
}


/*
// Recursively calculate the result of an FM algorithm
float process_alg(Operator* op) {
  float freq_result = 0;
  for (int i = 0; i < NUM_OPERATORS - 1; i++) {
    if (op->mods[i] != 0) {
      freq_result += process_alg(op->mods[i]);
    }
  }
  return op->wavetable[]
}
*/


// Process and write the next frames of audio in a stream
void audio_callback(void* data_buf, unsigned int frames) {
  short* d = (short*)data_buf;
  float interval = freq / SAMPLE_RATE * WAVETABLE_SIZE;

  for (int i = 0; i < frames; i++) {
    d[i] = (short)((*cur_table)[wave_pos] * SAMPLE_RANGE * amp);
    wave_pos += (int)interval;
    if (wave_pos > WAVETABLE_SIZE) {
      wave_pos -= WAVETABLE_SIZE;
    }
  }
}


// Clamp a float between two values
float clampf(float f, float a, float b) {
  if (f < a) return a;
  if (f > b) return b;
  return f;
}


int main(void) {
  InitWindow(WINDOW_X, WINDOW_Y, "FM Synth");
  InitAudioDevice();

  // Generate wavetables
  empty_table = gen_wavetable(NONE);
  sine_table = gen_wavetable(SINE);
  square_table = gen_wavetable(SQUARE);
  triangle_table = gen_wavetable(TRIANGLE);
  saw_table = gen_wavetable(SAW);

  cur_table = &empty_table;

  // Configure audio stream
  SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
  AudioStream stream = LoadAudioStream(SAMPLE_RATE, BIT_DEPTH, 1);
  SetAudioStreamCallback(stream, audio_callback);

  PlayAudioStream(stream);

  char wave_name[32] = "Flat";
  char freq_label[64] = "440.00 Hz";
  char amp_label[64] = "100%";
  
  // MAIN LOOP
  while(!WindowShouldClose()) {
    // Adjust frequency when mouse pressed
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      freq = ((float)GetMouseX() / WINDOW_X) * 2093.0f;
      freq = freq < 0 ? 0 : freq;
      amp = 1.0f - (float)GetMouseY() / WINDOW_Y;
      amp = clampf(amp, 0.0f, 1.0f);
    }

    // Change table
    int key = GetKeyPressed();
    if (key == KEY_ZERO) {
      cur_table = &empty_table;
      strcpy(wave_name, "Flat");
    }
    else if (key == KEY_ONE) {
      cur_table = &sine_table;
      strcpy(wave_name, "Sine");
    }
    else if (key == KEY_TWO) {
      cur_table = &square_table;
      strcpy(wave_name, "Square");
    }
    else if (key == KEY_THREE) {
      cur_table = &triangle_table;
      strcpy(wave_name, "Triangle");
    }
    else if (key == KEY_FOUR) {
      cur_table = &saw_table;
      strcpy(wave_name, "Saw");
    }


    // DRAW PHASE
    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Draw waveform
    DrawLine(0, WINDOW_Y * 0.5, WINDOW_X, WINDOW_Y * 0.5, ColorFromHSV(0.0, 0.0, 0.37f));
    int prev_y_pos = WINDOW_Y * 0.5;
    for (int i = 0; i < WINDOW_X; i++) {
      int table_pos = (int)((float)i / (float)WINDOW_X * WAVETABLE_SIZE * freq * 0.01) % WAVETABLE_SIZE;
      int y_pos = WINDOW_Y * -(*cur_table)[table_pos] * amp * 0.4 + WINDOW_Y * 0.5;

      DrawLine(i - 1, prev_y_pos, i, y_pos, ORANGE);
      prev_y_pos = y_pos;
    }

    snprintf(freq_label, 64, "%.2f Hz", freq);
    snprintf(amp_label, 64, "%d%%", (int)(amp * 100));

    DrawText(wave_name, 10, 10, 30, ORANGE);
    DrawText(freq_label , 10, 40, 30, ORANGE);
    DrawText(amp_label , 10, 70, 30, ORANGE);

    EndDrawing();
  };

  // Free heap memory
  free(empty_table);
  free(sine_table);
  free(square_table);
  free(triangle_table);
  free(saw_table);

  return 0;
}
