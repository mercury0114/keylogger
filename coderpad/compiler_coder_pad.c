#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct input_event Event;

static const int MAX_SLEEP_COUNT = 6;
static const int KEY_PRESSED = 1;

static const int LEFT_CTRL = 29;
static const int S = 31;
static const int RIGHT_SHIFT = 54;

// Note: on other computers path to a keyboard buffer might differ
static const char KEYBOARD_PATH[] = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
static const char HTML_PATH[] = "/home/mlatinis/Downloads/Marius L.html";
static const char ESCAPED_PATH[] = "/home/mlatinis/Downloads/Marius\\ L.html";
static const char MOVED_HTML_PATH[] = "./data/page.html";
static const char EXTRACTED_CODE_PATH[] = "./data/extracted_code.cc";
static const char MODIFIED_CODE_PATH[] = "./data/modified_code.cc";

static const char WAIT_MESSAGE[] = "Waiting for keyboard events";

static void PrintRed(const char message[]) {
    printf("\x1B[31m%s\x1B[0m", message);
}

static bool DownloadedHTML(void) {
    struct stat buffer;
    printf("Waiting while Chrome saves HTML do ~/Downloads folder\n");
    int sleepCount = 0;
    while (access(HTML_PATH, F_OK) == -1 && sleepCount < MAX_SLEEP_COUNT) {
        sleep(1);
        sleepCount++;
    }
    if (sleepCount == MAX_SLEEP_COUNT) {
        PrintRed("Timeout waiting for HTML file\n");
        return false;
    }
    char command[300] = {0};
    // Moving HTML file to our internal data directory
    sprintf(command, "mv %s %s", ESCAPED_PATH, MOVED_HTML_PATH);
    system(command);
    printf("HTML downloaded\n");
    return true;
}

static bool CommandSucceeded(FILE* const file) {
    char log[1000];
    bool success = true;
    while (fgets(log, sizeof(log), file)) {
        success = false;
        PrintRed(log);
    }
    pclose(file);
    return success;
}

static bool ExtractedCode(void) {
    FILE* file = popen("python3 html_parser.py", "r");
    if (!file) {
        PrintRed("Failed to run command\n");
        pclose(file);
        return false;
    }
    return CommandSucceeded(file);
}

static void CompileAndRun(void) {
    char command[300];
    sprintf(command, "cat ./cpp_include.cpp %s > %s",
            EXTRACTED_CODE_PATH, MODIFIED_CODE_PATH);
    system(command);
    system("./compile_and_run_cpp.sh");
}

static bool LeftCtrlPressed(const Event* const event) {
    return (event->value == KEY_PRESSED && event->code == LEFT_CTRL);
}
static bool LeftCtrlReleased(const Event* const event) {
    return event->value == LEFT_CTRL;
}
static bool RightShiftPressed(const Event* const event) {
    return (event->value == KEY_PRESSED && event->code == RIGHT_SHIFT);
}

int main(int argc, char* argv[]) {
    FILE* keyboard_buffer = fopen(KEYBOARD_PATH, "r");
    if (keyboard_buffer == NULL) {
        PrintRed("Failed to open the keyboard. Need a root access.\n");
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
            remove(HTML_PATH);
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
        if (ctrl_pressed && event.code == S) {
            if (!DownloadedHTML()) {
                continue;
            }
            if (!ExtractedCode()) {
                continue;
            }
            CompileAndRun();
        }
    }

    return 0;
}

