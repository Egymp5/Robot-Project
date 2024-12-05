#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */
#define MAX_WORD_LENGTH 100 
#define FONT_DATA_SIZE 128 // Number of ASCII characters
#define MAX_LINE_WIDTH 100 // Max writing width in mm
#define FONT_SCALE_FACTOR 18 // S.F to reduce size

// Declare functions
int UserInputTextHeight();
float ScaleFactorAdjustment(int textHeight);
void SendCommands(char *buffer);
void LoadFontFile(const char *fontFilePath); // Placeholder for font loading
void GenerateGCodeForWord(const char *word, FILE *outputFile, float scaleFactor); // Placeholder for G-code generation

int main() {
    char buffer[100];
    int textHeight;
    float scaleFactor;

    // Initialize RS232 communication
    if (CanRS232PortBeOpened() == -1) {
        printf("\nUnable to open the COM port (specified in serial.h)");
        exit(0);
    }

    printf("\nThe robot is now ready to draw\n");

    // Initialize robot
    sprintf(buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf(buffer, "M3\n");
    SendCommands(buffer);
    sprintf(buffer, "S0\n");
    SendCommands(buffer);

    // Get text height from user and calculate scale factor
    textHeight = UserInputTextHeight();
    if (textHeight == -1) {
        printf("Exiting program.\n");
        return 0;
    }
    scaleFactor = ScaleFactorAdjustment(textHeight);

    // Placeholder for processing text
    printf("Text height: %d mm, Scale factor: %.2f\n", textHeight, scaleFactor);

    // Close communication
    CloseRS232Port();
    printf("COM port now closed\n");
    return 0;
}

int UserInputTextHeight() {
    int textHeight;
    printf("Enter text height (4-10 mm): ");
    scanf("%d", &textHeight);

    if (textHeight < 4 || textHeight > 10) {
        printf("Invalid text height.\n");
        return -1;
    }
    return textHeight;
}

float ScaleFactorAdjustment(int textHeight) {
    return (float)textHeight / FONT_SCALE_FACTOR;
}

void SendCommands(char *buffer) {
    PrintBuffer(&buffer[0]);
    WaitForReply();
    Sleep(100);
}
