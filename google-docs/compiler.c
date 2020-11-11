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

// Note: on other computers path to a keyboard buffer might differ
static const char KEYBOARD_PATH[] = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
static const char DOCUMENT_PATH[] = "./data/document.txt";
static const char ORIGINAL_CODE_PATH[] = "./data/original.txt";
static const char FORMATTED_CODE_PATH[] = "./data/formatted_code.cc"; // works for python3 too
static const char URL[] = "https://docs.google.com/document/d/1iOMIglqTnTHp7hwvD9YxZdv8umx-ArAQNvP2W3EkspE/export?format=txt";

static const char WAIT_MESSAGE[] = "Waiting for keyboard events";
static const char PYTHON3[] = "Python3";
static const char PYTHON3_BEGIN_COMMENT[] = "# CODE";
static const char PYTHON3_COMPILE_SCRIPT[] = "./compile_python.sh";
static const char CPP[] = "CPP";
static const char CPP_BEGIN_COMMENT[] = "// CODE";
static const char CPP_COMPILE_SCRIPT[] = "./compile_cpp.sh";

typedef struct input_event Event;

typedef struct Language_ {
    const char* name;
    const char* begin_comment;
    const char* compile_script;
} Language;

static const Language SupportedLanguages[] = {
    {PYTHON3, PYTHON3_BEGIN_COMMENT, PYTHON3_COMPILE_SCRIPT}, // Python3
    {CPP, CPP_BEGIN_COMMENT, CPP_COMPILE_SCRIPT}, // CPP
};

static const int LANGUAGE_COUNT = sizeof(SupportedLanguages) / sizeof(Language);

static void PrintRed(const char message[]) {
    printf("\x1B[31m%s\x1B[0m", message);
}

void ListSupportedLanguages() {
    for (size_t l = 0; l < LANGUAGE_COUNT; l++) {
        printf("%s ", SupportedLanguages[l].name);
    }
    printf("\n");
}

Language FindLanguage(const char name[]) {
    for (size_t l = 0; l < LANGUAGE_COUNT; l++) {
        if (strcmp(SupportedLanguages[l].name, name) == 0) {
            return SupportedLanguages[l];
        }
    }
    PrintRed("Your chosen language is not supported\n");
    printf("We support: ");
    ListSupportedLanguages();
    exit(EXIT_FAILURE);
}


// for Python3 code, if we see a function call in main, we print its return value
void MakeFunctionCallPrintReturnValue(char* const line) {
    int pos = 0;
    while (pos < C_MAX_COLS && line[pos++] != '(') {}
    int bracket_diff = 1;
    while (bracket_diff > 0 && pos < C_MAX_COLS) {
        char letter = line[pos++];
        if (letter == '(') {
            bracket_diff++;
        }
        if (letter == ')') {
            bracket_diff--;
        }
    }
    if (pos == C_MAX_COLS) return;

    char temp[C_MAX_COLS] = {0};
    memcpy(temp, line, pos);
    sprintf(line, "print(%s)\n", temp);
}

static bool CommandSucceeded(const char command[]) {
    FILE* file = popen(command, "r");
    if (!file) {
        PrintRed("Failed to run command\n");
        pclose(file);
        return false;
    }
    char log[1000];
    bool success = true;
    while (fgets(log, sizeof(log), file)) {
        success = false;
        PrintRed(log);
    }
    if (WEXITSTATUS(pclose(file))) {
        return false;
    }
    return success;
}

static bool CodeExtracted(const Language* const language) {
    char command[300];
    
    // Finding the code segment inside the document
    sprintf(command, "grep \"%s\" %s -A 10000 > %s", language->begin_comment,
		DOCUMENT_PATH, ORIGINAL_CODE_PATH);
    if (!CommandSucceeded(command)) {
        PrintRed("No starting comment found\n");
        return false;
    }

    // Reformating C++ code by adding extra includes
    if (strcmp(language->name, CPP) == 0) {
	    sprintf(command, "cat ./cpp_include.cpp %s > %s",
			ORIGINAL_CODE_PATH, FORMATTED_CODE_PATH);
	    system(command);
        return true;
    }

    // Reformatting python code by fixing identation
    if (strcmp(language->name, PYTHON3) == 0) {
        FILE* unformatted = fopen(ORIGINAL_CODE_PATH, "r");
        FILE* formatted = fopen(FORMATTED_CODE_PATH, "w");
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
                MakeFunctionCallPrintReturnValue(line);
            }
            fprintf(formatted, "%s", line);
        }
        fclose(unformatted);
        fclose(formatted);
        return true;
    }

    return false;
}

void CompileAndRun(const Language* const language) {
    char command[300];
    sprintf(command, "%s %s", language->compile_script, FORMATTED_CODE_PATH);
    system(command);
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


int main(int argc, char* argv[]) {
    if (argc != 2) {
        PrintRed("ERROR: please specify language choice\n");
        ListSupportedLanguages();
        return 1;
    }
    const Language language = FindLanguage(argv[1]);

    FILE* keyboard_buffer = fopen(KEYBOARD_PATH, "r");
    if (keyboard_buffer == NULL) {
        PrintRed("Failed to open the keyboard file. Try running with sudo.\n");
        return 1;
    }

    printf("%s\n", WAIT_MESSAGE);
    Event event;
    bool ctrl_pressed = false;
    while(1) {
        fread(&event, sizeof event, 1, keyboard_buffer);
        if (!event.value) {
            continue;
        }
        if (RightShiftPressed(&event)) {
            system("clear -x");
            printf("%s\n", WAIT_MESSAGE);
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
            // Downloading the document
            char command[300];
            sprintf(command, "wget -q \"%s\" -O %s\n", 
                URL, DOCUMENT_PATH);
            system(command);
            // Removing non-ascii characters
            sprintf(command, "perl -pi -e 's/[^[:ascii:]]//g' %s", DOCUMENT_PATH);
            system(command);
            printf("Document downloaded\n");

            if (!CodeExtracted(&language)) {
                continue;
            }
            CompileAndRun(&language);
        }
    }

    return 0;
}

