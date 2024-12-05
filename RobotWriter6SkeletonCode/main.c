#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200 // 115200 baud
#define MAX_CHARACTERS 128
#define FONT_SCALE_FACTOR 18

// Font structure to store character data
typedef struct {
    int charCode;
    int numStrokes;
    int (*strokes)[3];
} FontCharacter;

// Function prototypes
void SendCommands(char *buffer);
FontCharacter* loadFontData(const char *filename, int *numCharacters);
void freeFontData(FontCharacter *fontArray, int numCharacters);
double getTextHeight();
double calculateScaleFactor(double textHeight);

int main() {
    const char *fontFile = "SingleStrokeFont.txt"; // Font file name
    int numCharacters = 0;

    // Load font data
    FontCharacter *fontArray = loadFontData(fontFile, &numCharacters);
    if (!fontArray) {
        printf("Failed to load font data.\n");
        return 1;
    }
    printf("Loaded %d characters from font file.\n", numCharacters);

    // Get user input for text height
    double textHeight = getTextHeight();
    double scaleFactor = calculateScaleFactor(textHeight);
    printf("Scale Factor: %.4f\n", scaleFactor);

    // Free resources
    freeFontData(fontArray, numCharacters);
    printf("Iteration 1 complete.\n");
    return 0;
}

// Function to prompt and validate text height input
double getTextHeight() {
    double textHeight;
    while (1) {
        printf("Enter text height (4-10 mm): ");
        if (scanf("%lf", &textHeight) == 1 && textHeight >= 4 && textHeight <= 10) {
            return textHeight;
        }
        printf("Invalid input. Please try again.\n");
    }
}

// Calculate the scale factor based on text height
double calculateScaleFactor(double textHeight) {
    return textHeight / FONT_SCALE_FACTOR;
}

// Function to load font data from a file
FontCharacter* loadFontData(const char *filename, int *numCharacters) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open font file %s\n", filename);
        return NULL;
    }

    FontCharacter *fontArray = malloc(MAX_CHARACTERS * sizeof(FontCharacter));
    if (!fontArray) {
        printf("Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }

    *numCharacters = 0;
    while (!feof(file)) {
        int marker, charCode, numStrokes;
        if (fscanf(file, "%d %d %d", &marker, &charCode, &numStrokes) != 3 || marker != 999) {
            break;
        }

        FontCharacter *currentChar = &fontArray[*numCharacters];
        currentChar->charCode = charCode;
        currentChar->numStrokes = numStrokes;
        currentChar->strokes = malloc(numStrokes * sizeof(int[3]));
        if (!currentChar->strokes) {
            printf("Memory allocation failed for strokes.\n");
            fclose(file);
            return NULL;
        }

        for (int i = 0; i < numStrokes; i++) {
            fscanf(file, "%d %d %d", &currentChar->strokes[i][0], &currentChar->strokes[i][1], &currentChar->strokes[i][2]);
        }
        (*numCharacters)++;
    }
    fclose(file);
    return fontArray;
}

// Free allocated memory for font data
void freeFontData(FontCharacter *fontArray, int numCharacters) {
    for (int i = 0; i < numCharacters; i++) {
        free(fontArray[i].strokes);
    }
    free(fontArray);
}

// Send commands to the robot
void SendCommands(char *buffer) {
    printf("%s", buffer); // Simulate sending commands for now
}
