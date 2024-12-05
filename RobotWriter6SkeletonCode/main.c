#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200 // 115200 baud
#define MAX_CHARACTERS 128 // Maximum ASCII characters
#define LINE_SPACING 5.0 // Space between lines in mm
#define MAX_WIDTH 100.0 // Maximum line width in mm

// Structure to store font data for each ASCII character
typedef struct {
    int asciiCode;        // ASCII code of the character
    int strokeCount;      // Number of strokes
    int (*strokes)[3];    // Array of stroke data (x, y, pen state)
} FontData;

// Function prototypes
void TransmitCommands(char *buffer);
void DisplayToTerminal(char *buffer); // Test function to output G-code to terminal
double GetTextHeight(); // Prompts user to input a height for the text between 4-10mm
double ComputeScaleFactor(double textHeight); // Calculates the scale factor based on the text height
FontData* LoadFontFile(const char *filename, int *fontCharCount);
void FreeFontData(FontData *fontArray, int fontCharCount);
void ProcessText(const char *textFileName, FontData *fontArray, int fontCharCount, double scaleFactor);

int main() {
    double textHeight, scaleFactor;
    const char *fontFile = "SingleStrokeFont.txt"; // Font file name
    const char *textFile = "test.txt"; // Text input file
    int fontCharCount = 0;

    // Load font data
    FontData *fontArray = LoadFontFile(fontFile, &fontCharCount);
    if (!fontArray) {
        return 1; // Exit if font loading fails
    }
    printf("Loaded %d characters from font file.\n", fontCharCount);

    // Get user input for text height
    textHeight = GetTextHeight();
    scaleFactor = ComputeScaleFactor(textHeight);
    printf("Scale Factor: %.4f\n", scaleFactor);

    // Test function: Process the text file and output G-code to terminal
    ProcessText(textFile, fontArray, fontCharCount, scaleFactor);

    // Cleanup
    FreeFontData(fontArray, fontCharCount);
    printf("Process completed.\n");
    return 0;
}

// Function to output G-code to the terminal for testing
void DisplayToTerminal(char *buffer) {
    printf("%s", buffer);
}

// Function to prompt and validate text height input
double GetTextHeight() {
    double textHeight;
    int valid = 0;

    while (!valid) {
        printf("Enter text height in mm (between 4 and 10 mm): ");
        if (scanf("%lf", &textHeight) != 1) {
            printf("Invalid input. Please enter a numeric value.\n");
            while (getchar() != '\n'); // Clear input buffer
        } else if (textHeight < 4 || textHeight > 10) {
            printf("Invalid text height. It must be between 4 and 10 mm.\n");
        } else {
            valid = 1; // Input is valid
        }
    }
    return textHeight;
}

// Function to calculate the scale factor
double ComputeScaleFactor(double textHeight) {
    return textHeight / 18.0;
}

// Function to load font data from a file
FontData* LoadFontFile(const char *filename, int *fontCharCount) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open font file %s\n", filename);
        return NULL;
    }

    FontData *fontArray = malloc(MAX_CHARACTERS * sizeof(FontData));
    if (!fontArray) {
        printf("Error: Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }

    *fontCharCount = 0; // Initialize character count
    while (!feof(file)) {
        int marker, asciiCode, strokeCount;

        // Read marker, asciiCode, and strokeCount
        if (fscanf(file, "%d %d %d", &marker, &asciiCode, &strokeCount) != 3 || marker != 999) {
            break;
        }

        FontData *currentChar = &fontArray[*fontCharCount];
        currentChar->asciiCode = asciiCode;
        currentChar->strokeCount = strokeCount;
        currentChar->strokes = malloc(strokeCount * sizeof(int[3]));
        if (!currentChar->strokes) {
            printf("Error: Memory allocation for strokes failed.\n");
            fclose(file);
            return NULL;
        }

        // Read strokes
        for (int i = 0; i < strokeCount; i++) {
            fscanf(file, "%d %d %d", &currentChar->strokes[i][0],
                   &currentChar->strokes[i][1],
                   &currentChar->strokes[i][2]);
        }

        (*fontCharCount)++;
    }

    fclose(file);
    return fontArray;
}

// Function to free allocated memory for font data
void FreeFontData(FontData *fontArray, int fontCharCount) {
    for (int i = 0; i < fontCharCount; i++) {
        free(fontArray[i].strokes); // Free stroke data
    }
    free(fontArray); // Free font array
}

// Function to process text and output G-code
void ProcessText(const char *textFileName, FontData *fontArray, int fontCharCount, double scaleFactor) {
    FILE *file = fopen(textFileName, "r");
    if (!file) {
        printf("Error: Could not open text file %s\n", textFileName);
        return;
    }

    char word[100];
    double xOffset = 0; // Horizontal position
    double yOffset = 0; // Vertical position
    int c;

    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') { // Handle newline
            xOffset = 0;
            yOffset -= LINE_SPACING + 5; // Move down to next line
            printf("Line Feed: Moving to next line at Y offset %.2f\n", yOffset);
            continue;
        }

        ungetc(c, file); // Push back character if it's part of a word

        // Read the word
        if (fscanf(file, "%99s", word) != 1) {
            break;
        }

        printf("Processing word: %s\n", word);

        // Word width calculation placeholder
        double wordWidth = 0.0;
        for (int i = 0; word[i] != '\0'; i++) {
            wordWidth += 10.0 * scaleFactor; // Assume each character contributes 10 units of width
        }
        wordWidth += 5.0 * scaleFactor; // Add inter-word spacing

        // Check line width
        if (xOffset + wordWidth > MAX_WIDTH) {
            xOffset = 0;
            yOffset -= LINE_SPACING + 5; // Move to next line
            printf("Word exceeds line width. Moving to next line at Y offset %.2f\n", yOffset);
        }

        // Placeholder for character processing
        for (int i = 0; word[i] != '\0'; i++) {
            xOffset += 10.0 * scaleFactor; // Increment position for each character
        }
        xOffset += 5.0 * scaleFactor; // Add spacing after word
    }

    fclose(file);
}

// Function to send commands
void TransmitCommands(char *buffer) {
    DisplayToTerminal(buffer);
}
