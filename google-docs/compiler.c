#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_KEY_PRESSED 1

#define C_LEFT_CTRL 29
#define C_S 31
#define C_RIGHT_SHIFT 54

#define C_MAX_COLS 300

#define KRED "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KNRM "\x1B[0m"

// Note: on other computers path to a keyboard buffer might differ
static const char KEYBOARD_PATH[] = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
static const char CODE_PATH[] = "./data/original_code.cc";
static const char CODE_PATH_AFTER_GREP[] = "./data/code_after_grep.cc";
static const char URL[] = "https://docs.google.com/document/d/1iOMIglqTnTHp7hwvD9YxZdv8umx-ArAQNvP2W3EkspE/export?format=txt";

static const char TEMP_FILE[] = "./data/temp.txt";
static const char CPP_SCRIPT[] = "./compile_cpp.sh";
static const char CPP_COMMENT[] = "// CODE";
static const char PYTHON_SCRIPT[] = "./compile_python.sh";
static const char PYTHON_COMMENT[] = "## CODE";

typedef struct input_event Event;

typedef enum Status_ {
    OK = 0,
    SHELL_COMMAND_ERROR = 1,
    NO_STARTING_COMMENT = 2,
} Status;

bool Error(Status status) {
    switch(status) {
        case OK:
            return false;
        case SHELL_COMMAND_ERROR:
            printf(KRED "ERROR: Shell command failed\n");
            return true;
        case NO_STARTING_COMMENT:
            printf(KRED "ERROR: No starting comment found\n");
            return true;
    }
}

bool EmptyFile(const char file_path[]) {
    FILE* file = fopen(file_path, "r");
    int size = 0;
    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
    }
    fclose(file);
    return size == 0;
}

void AddPrint(char* const line) {
    int pos = 0;
    while (pos < C_MAX_COLS && line[pos++] != '(') {}
    if (pos == C_MAX_COLS) return;

    int bracket_diff = 1;
    while (bracket_diff > 0) {
        char letter = line[pos++];
        if (letter == '(') {
            bracket_diff++;
        }
        if (letter == ')') {
            bracket_diff--;
        }
    }
    char original[C_MAX_COLS] = {0};
    memcpy(original, line, pos);
    sprintf(line, "print(%s)\n", original);
}

Status Save(const char starting_comment[]) {
    char command[300];
    sprintf(command, "wget -q \"%s\" -O %s\n", URL, CODE_PATH);
    FILE* file = popen(command, "r");
    pclose(file);

    sprintf(command, "grep \"%s\" %s -A 10000 > %s", starting_comment,
		CODE_PATH, CODE_PATH_AFTER_GREP);
    file = popen(command, "r");
    if (!file) {
        return SHELL_COMMAND_ERROR;
    }
    pclose(file);
    if (EmptyFile(CODE_PATH_AFTER_GREP)) {
        return NO_STARTING_COMMENT;
    }

    // Reformating C++ code by adding extra includes
    if (strcmp(starting_comment, CPP_COMMENT) == 0) {
	    sprintf(command, "cat ./cpp_include.cpp %s > %s",
			CODE_PATH_AFTER_GREP, TEMP_FILE);
	    system(command);
	    sprintf(command, "cp %s %s", TEMP_FILE, CODE_PATH_AFTER_GREP);
	    system(command);
    }

    // Reformatting python code by fixing identation
    if (strcmp(starting_comment, PYTHON_COMMENT) == 0) {
        FILE* unformatted = fopen(CODE_PATH_AFTER_GREP, "r");
        FILE* formatted = fopen(TEMP_FILE, "w");
        char line[C_MAX_COLS] = {0};
        while (fgets(line, sizeof(line), unformatted)) {
            int space_count = 0;
            while(space_count < sizeof(line) && line[space_count] == ' ') {
                space_count++;
            }
            int closestTo4 = ((space_count + 1) / 4) * 4;
            if (closestTo4 <= space_count) {
                memmove(&line[space_count], &line[closestTo4], 
                        sizeof(line) - space_count);
            } else {
                memmove(&line[closestTo4], &line[space_count],
                        sizeof(line) - closestTo4);
                memset(&line[space_count], ' ', closestTo4 - space_count);
            }
            // If we call a function
            if (closestTo4 == 0 && line[0] >= 'A' && line[0] <= 'Z') {
                AddPrint(line);
            }
            fprintf(formatted, "%s", line);
        }
        fclose(unformatted);
        fclose(formatted);
        sprintf(command, "mv %s %s", TEMP_FILE, CODE_PATH_AFTER_GREP);
        system(command);
    }

    printf(KGRN "New code downloaded and saved\n");
    return OK;
}

Status Compile(const char script_name[]) {
    char command[300];
    sprintf(command, "%s %s", script_name, CODE_PATH_AFTER_GREP);
    FILE* file = popen(command, "r");
    if (!file) {
        printf("Failed to run command\n");
        pclose(file);
        return SHELL_COMMAND_ERROR;
    }
    char log[1000];
    while (fgets(log, sizeof(log), file)) {
	    printf(KNRM "%s", log);
    }
    printf(KNRM "\n");
    pclose(file);
    return OK;
}

void PrintIfEvent(const Event* const event) {
    if (event->value) {
        printf("EVENT: value %d, code %d\n", event->value, event->code);
    }
}

bool LeftCtrlPressed(const Event* const event) {
    return (event->value == C_KEY_PRESSED && event->code == C_LEFT_CTRL);
}
bool LeftCtrlReleased(const Event* const event) {
    return event->value == C_LEFT_CTRL;
}
bool RightShiftPressed(const Event* const event) {
    return (event->value == C_KEY_PRESSED && event->code == C_RIGHT_SHIFT);
}

static void PrintNewLines(int number) {
    for (int i = 0; i < number; i++) {
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("ERROR: please specify language choice\n");
        printf("supported: CPP, Python3\n");
        return 1;
    }
    const char* const language = argv[1];
    bool supported = false;
    const char* starting_comment;
    const char* language_script;
    if (strcmp(language, "CPP") == 0) {
	    starting_comment = CPP_COMMENT;
	    language_script = CPP_SCRIPT;
	    supported = true;
    }
    if (strcmp(language, "Python") == 0) {
	    starting_comment = PYTHON_COMMENT;
	    language_script = PYTHON_SCRIPT;
    	supported = true;   
    }
    if (!supported) {
        printf(KRED "ERROR: we do not support this language\n");
	    return 1;
    }

    FILE* keyboard_buffer = fopen(KEYBOARD_PATH, "r");
    if (keyboard_buffer == NULL) {
        printf("Failed to open keyboard. Try running with sudo.\n");
        return 1;
    }

    Event event;
    bool ctrl_pressed = false;
    while(1) {
        fread(&event, sizeof event, 1, keyboard_buffer);
        if (!event.value) {
            continue;
        }
        if (RightShiftPressed(&event)) {
            PrintNewLines(100);
            continue;
        }
        if (LeftCtrlPressed(&event)) {
            ctrl_pressed = true;
            continue;
        }
        if (LeftCtrlReleased(&event)) {
            ctrl_pressed = false;
            continue;
        }
        if (ctrl_pressed && event.code == C_S) {
            if (Error(Save(starting_comment))) {
                continue;
            }
            if (Error(Compile(language_script))) {
                continue;
            }
        }
    }

    return 0;
}

