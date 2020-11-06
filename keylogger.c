#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KL_KEY_PRESSED 1
#define KL_KEY_HELD 2

#define KL_BACKSPACE 14
#define KL_TAB 15
#define KL_R 19
#define KL_ENTER 28
#define KL_LEFT_CTRL 29
#define KL_A 30
#define KL_S 31
#define KL_LEFT_SHIFT 42
#define KL_C 46
#define KL_RIGHT_SHIFT 54
#define KL_SPACE 57

#define KL_UP_ARROW 103
#define KL_LEFT_ARROW 105
#define KL_RIGHT_ARROW 106
#define KL_DOWN_ARROW 108

#define KL_UP_ARROW_RELEASE 200
#define KL_LEFT_ARROW_RELEASE 203
#define KL_RIGHT_ARROW_RELEASE 205
#define KL_DOWN_ARROW_RELEASE 208

#define KL_MAX_ROWS 300
#define KL_MAX_COLS 150

#define KRED "\x1B[31m"
#define KNRM "\x1B[0m"

static const char KEY_TO_LETTER[] = {0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,
                                     '\t','q','w','e','r','t','y','u','i','o','p','[',']',0,0,
                                     'a','s','d','f','g','h','j','k','l',';',39,96,0,'\\',
                                     'z','x','c','v','b','n','m',',','.','/'};
static const char SHIFT_TO_LETTER[] = {0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,
                                     '\t','Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,
                                     'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
                                     'Z','X','C','V','B','N','M','<','>','?'};

// Note: on other computers path to a keyboard buffer might differ
static const char KEYBOARD_PATH[] = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
static const char CODE_PATH[] = "./code.py";

typedef struct State_ {
    int current_row, current_col;
    bool shift_pressed;
    bool ctrl_pressed;
} State;

typedef struct input_event Event;

typedef struct Source_ {
    char text[KL_MAX_ROWS][KL_MAX_COLS];
    int rows_count;
} Source;

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
}

void Compile() {
    char command[100];
    sprintf(command, "python %s\n", CODE_PATH);
    FILE* file = popen(command, "r");
    if (!file) {
        printf("Failed to run command\n");
        pclose(file);
        return;
    }
    char log[1000];
    printf("COMPILE OUTPUT\n");
    while (fgets(log, sizeof(log), file)) {
        printf("%s", log);
    }
    printf("\n");
    pclose(file);
    return;
}

int RowLength(int row, const Source* const source) {
    for (int col = 0; col < KL_MAX_COLS; col++) {
        if (source->text[row][col] == 0) {
            return col;
        }
    }
    return 0;
}

void Flush(const Source* const source) {
    FILE* code_file = fopen(CODE_PATH, "w");
    if (source->rows_count > 1) {
        for (int row = 0; row < source->rows_count; row++) {
            fprintf(code_file, "%s\n", source->text[row]);
        }
        fclose(code_file);
        return;
    }
    if (RowLength(0, source) > 0) {
        fprintf(code_file, "%s\n", source->text[0]);
    }
    fclose(code_file);
}

void Reset(Source* const source, State* const state) {
    memset(source, 0, sizeof(*source));
    source->rows_count = 1;
    memset(state, 0, sizeof(*state));
    Flush(source);
}

void Print(const Source* const source, const State* const state) {
    printf("TOTAL LINES: %d\n", source->rows_count);
    for (int row = 0; row < source->rows_count; row++) {
        if (row == state->current_row) {
            char row_with_cursor[KL_MAX_COLS];
            memcpy(row_with_cursor, source->text[row], KL_MAX_COLS);
            int current_col = min(state->current_col, RowLength(row, source));
            char cursor = source->text[row][current_col];
            row_with_cursor[current_col] = 0;
            printf("%s", row_with_cursor);
            printf(KRED "|");
            printf(KNRM "%c", cursor);
            printf(KNRM "%s\n", &row_with_cursor[current_col + 1]);
        } else {
            printf("%s\n", source->text[row]);
        }
    }
    printf("\n");
}

void Refresh(Source* const source, State* const state) {
    FILE* file = fopen(CODE_PATH, "r");
    source->rows_count = 0;
    while (fgets(source->text[source->rows_count], KL_MAX_COLS, file)) {
        int col = 0;
        while (source->text[source->rows_count][col] != '\n') {
            col++;
        }
        source->text[source->rows_count][col] = 0;
        source->rows_count++;
    }
    source->rows_count = max(source->rows_count, 1);
    fclose(file);
    state->current_row = state->current_col = 0;

    Print(source, state);
}

void PrintIfEvent(const Event* const event, const State* const state,
		  const Source* const source) {
    if (event->value) {
        printf("EVENT: value %d, code %d\n", event->value, event->code);
        printf("STATE: %d %d, ctrl %d, shift %d\n", state->current_row, state->current_col,
                                                    state->ctrl_pressed, state->shift_pressed);
        printf("CODE: rows_count %d\n", source->rows_count);
    }
}

char GetLetter(int code, const State* const state) {
    if (code == KL_SPACE) {
        return ' ';
    }
    return state->shift_pressed ? SHIFT_TO_LETTER[code] : KEY_TO_LETTER[code];
}

bool Supported(const Event* const event) {
    int code = (event->value == KL_KEY_PRESSED || event->value == KL_KEY_HELD) ?
            event->code : event->value;

    if (code > 1 && code < sizeof(KEY_TO_LETTER)) {
        return true;
    }

    if (code == KL_SPACE || code == KL_RIGHT_SHIFT) {
        return true;
    }

    if (code == KL_LEFT_ARROW || code == KL_RIGHT_ARROW ||
        code == KL_DOWN_ARROW || code == KL_UP_ARROW) {
        return true;
    }

    if (code == KL_LEFT_ARROW_RELEASE || code == KL_RIGHT_ARROW_RELEASE || 
        code == KL_DOWN_ARROW_RELEASE || code == KL_UP_ARROW_RELEASE) {
        return true;
    }

    printf("WARNING: this button is not supported\n");
    return false;
}

int main() {
    FILE* keyboard_buffer = fopen(KEYBOARD_PATH, "r");
    if (keyboard_buffer == NULL) {
        printf("Failed to open keyboard. Try running with sudo.\n");
        return 1;
    }

    Source source = {0};
    source.rows_count = 1;
    State state = {0};

    Event event;
    while(1) {
        fread(&event, sizeof event, 1, keyboard_buffer);
        if (!event.value || !Supported(&event)) {
            continue;
        }

        if (event.code == KL_RIGHT_SHIFT) {
            PrintIfEvent(&event, &state, &source);
            continue;
        }

        if (event.code == KL_LEFT_CTRL) {
            state.ctrl_pressed = true;
            continue;
        }
        if (state.ctrl_pressed && event.value != KL_LEFT_CTRL) {
            if (event.code == KL_S) {
                Flush(&source);
            }
            if (event.code == KL_A) {
                Reset(&source, &state);
            }
            if (event.code == KL_R) {
                Refresh(&source, &state);
            }
            if (event.code == KL_C) {
                Compile();
            }
            continue;
        }

        if (event.value == KL_KEY_PRESSED) {
            const int row_length = RowLength(state.current_row, &source);
            switch(event.code) {
                case KL_LEFT_SHIFT: {
                    state.shift_pressed = true;
                    break;
                }
                case KL_LEFT_ARROW: {
                    if (state.current_col > 0) {
                        state.current_col = min(state.current_col - 1, row_length - 1);
                    } else {
                        if (state.current_row > 0) {
                            state.current_row--;
                            state.current_col = RowLength(state.current_row, &source);
                        }
                    }
                    break;
                }
                case KL_RIGHT_ARROW: {
                    if (state.current_col < row_length) {
                        state.current_col++;
                    } else {
                        if (state.current_row + 1 < source.rows_count) {
                            state.current_row++;
                            state.current_col = 0;
                        }
                    }
                    break;
                }
                case KL_UP_ARROW: {
                    if (state.current_row == 0) {
                        state.current_col = 0;
                    }
                    state.current_row = max(state.current_row - 1, 0);
                    break;
                }
                case KL_DOWN_ARROW: {
                    if (state.current_row == source.rows_count-1) {
                        state.current_col = row_length;
                    }
                    state.current_row = min(state.current_row + 1, source.rows_count-1);
                    break;
                }
                case KL_BACKSPACE: {
                    state.current_col = min(state.current_col, row_length);
                    if (state.current_col > 0) {
                        memmove(&source.text[state.current_row][state.current_col-1],
                                &source.text[state.current_row][state.current_col],
                                KL_MAX_COLS - state.current_col);
                        source.text[state.current_row][KL_MAX_COLS-1] = 0;
                        state.current_col--;
                    } else {
                        for (int row = state.current_row; row < source.rows_count - 1; row++) {
                            memmove(&source.text[row],
                                    &source.text[row + 1],
                                    KL_MAX_COLS);               
                        }
                        if (state.current_row > 0) {                            
                            state.current_row--;
                            source.rows_count--;
                            state.current_col = RowLength(state.current_row, &source);        
                        } 
                    }
                    break;
                }
                case KL_ENTER: {
                    for (int row = source.rows_count - 1; row > state.current_row; row--) {
                        memmove(&source.text[row+1],
                                &source.text[row],
                                KL_MAX_COLS);
                    }
                    state.current_col = min(state.current_col, row_length);
                    memmove(&source.text[state.current_row+1][0],
                            &source.text[state.current_row][state.current_col],
                            KL_MAX_COLS - state.current_col);
                    memset(&source.text[state.current_row][state.current_col],
                            0, KL_MAX_COLS - state.current_col);
                    source.rows_count++;
                    state.current_row++;
                    state.current_col = 0;
                    break;
                }

                default: {
                    if (state.shift_pressed && event.code == KL_TAB) {
                        break;
                    }
                    const char letter = GetLetter(event.code, &state);
                    const int row_length = RowLength(state.current_row, &source);
                    state.current_col = min(row_length, state.current_col);
                    memmove(&source.text[state.current_row][state.current_col + 1],
                            &source.text[state.current_row][state.current_col],
                            row_length - state.current_col);
                    source.text[state.current_row][state.current_col] = letter;
                    state.current_col++;
                    break;
                }
            }
	        Print(&source, &state);
            continue;
        }

        // release value specific for each key
        if (event.value == KL_LEFT_SHIFT) {
            state.shift_pressed = false;
        }
        if (event.value == KL_LEFT_CTRL) {
            state.ctrl_pressed = false;
        }
    }

    return 0;
}

