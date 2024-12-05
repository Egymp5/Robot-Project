#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

// Constants for robot operation
#define BAUD_RATE 115200        // Baud rate for RS232 communication
#define MAX_ASCII 128           // Total number of ASCII characters supported
#define LINE_SPACING_MM 5.0     // Space between lines in mm
#define MAX_LINE_WIDTH_MM 100.0 // Maximum allowable line width in mm
#define CHAR_EXTRA_SPACING 2.5  // Additional horizontal space between characters in mm

// Structure to store information about a font character
typedef struct {
    int asciiCode;         // ASCII code for the character
    int strokeTotal;       // Number of strokes used to form the character
    int (*strokeData)[3];  // Stroke data: (x offset, y offset, pen state)
} FontCharacter;

// Function declarations
double promptTextHeight();
double computeScaleFactor(double textHeight);
FontCharacter* loadFont(const char *fontPath, int *characterCount);
void cleanupFontMemory(FontCharacter *fontArray, int characterCount);
void convertTextToGCode(const char *textPath, FontCharacter *fontArray, int characterCount, double scaleFactor);
void printGCodeLine(char *gCodeLine);

int main() {
    const char *fontFilePath = "SingleStrokeFont.txt"; // Path to the font data file
    const char *inputTextPath = "test.txt";           // Path to the input text file
    double textHeight, scaleFactor;
    int characterCount = 0;

    // Load the font data into memory
    FontCharacter *fontArray = loadFont(fontFilePath, &characterCount);
    if (!fontArray) {
        return 1; // Exit if font loading fails
    }
    printf("Font data loaded successfully. Total characters: %d\n", characterCount);

    // Get the desired text height and calculate the corresponding scale factor
    textHeight = promptTextHeight();
    scaleFactor = computeScaleFactor(textHeight);
    printf("Calculated scale factor: %.4f\n", scaleFactor);

    // Convert the input text into G-code using the loaded font
    convertTextToGCode(inputTextPath, fontArray, characterCount, scaleFactor);

    // Clean up allocated memory for the font data
    cleanupFontMemory(fontArray, characterCount);
    printf("G-code generation completed successfully.\n");
    return 0;
}

// Function to read the font data file and populate the font array
FontCharacter* loadFont(const char *fontPath, int *characterCount) {
    FILE *file = fopen(fontPath, "r");
    if (!file) {
        printf("Error: Could not open font file: %s\n", fontPath);
        return NULL;
    }

    FontCharacter *fontArray = malloc(MAX_ASCII * sizeof(FontCharacter));
    if (!fontArray) {
        printf("Error: Memory allocation failed for font data.\n");
        fclose(file);
        return NULL;
    }

    *characterCount = 0; // Initialize the number of loaded characters
    while (!feof(file)) {
        int marker, asciiCode, strokeTotal;

        if (fscanf(file, "%d %d %d", &marker, &asciiCode, &strokeTotal) != 3 || marker != 999) {
            break;
        }

        FontCharacter *currentChar = &fontArray[*characterCount];
        currentChar->asciiCode = asciiCode;
        currentChar->strokeTotal = strokeTotal;
        currentChar->strokeData = malloc(strokeTotal * sizeof(int[3]));
        if (!currentChar->strokeData) {
            printf("Error: Memory allocation for character strokes failed.\n");
            fclose(file);
            return NULL;
        }

        // Read the strokes that define this character
        for (int i = 0; i < strokeTotal; i++) {
            fscanf(file, "%d %d %d", &currentChar->strokeData[i][0],
                   &currentChar->strokeData[i][1],
                   &currentChar->strokeData[i][2]);
        }

        (*characterCount)++;
    }

    fclose(file);
    return fontArray;
}

// Function to free the allocated memory for font data
void cleanupFontMemory(FontCharacter *fontArray, int characterCount) {
    for (int i = 0; i < characterCount; i++) {
        free(fontArray[i].strokeData);
    }
    free(fontArray);
}

// Function to prompt the user to enter the desired text height
double promptTextHeight() {
    double height;
    while (1) {
        printf("Enter text height (in mm, between 4 and 10): ");
        if (scanf("%lf", &height) != 1) {
            printf("Invalid input. Please enter a numeric value.\n");
            while (getchar() != '\n'); // Clear input buffer
        } else if (height < 4 || height > 10) {
            printf("Error: Text height must be between 4 mm and 10 mm.\n");
        } else {
            return height;
        }
    }
}

// Function to calculate the scale factor based on the text height
double computeScaleFactor(double textHeight) {
    return textHeight / 18.0;
}

// Function to process the input text and generate G-code for the robot
void convertTextToGCode(const char *textPath, FontCharacter *fontArray, int characterCount, double scaleFactor) {
    FILE *file = fopen(textPath, "r");
    if (!file) {
        printf("Error: Unable to open text file: %s\n", textPath);
        return;
    }

    char word[100];
    double xPos = 0.0, yPos = 0.0; // Initial offsets for drawing
    int charRead;

    while ((charRead = fgetc(file)) != EOF) {
        if (charRead == '\n') { // Handle line breaks
            xPos = 0.0;
            yPos -= LINE_SPACING_MM + 5.0; // Move down to the next line
            printf("Line break. Moving to Y position: %.2f\n", yPos);
            continue;
        } else if (charRead == '\r') { // Handle carriage returns
            xPos = 0.0;
            continue;
        }

        ungetc(charRead, file); // Return character to stream for word processing

        if (fscanf(file, "%99s", word) != 1) {
            break;
        }
        printf("Processing word: %s\n", word);

        // Process each character in the word
        for (int i = 0; word[i] != '\0'; i++) {
            FontCharacter *charData = NULL;

            // Find the font data for the character
            for (int j = 0; j < characterCount; j++) {
                if (fontArray[j].asciiCode == word[i]) {
                    charData = &fontArray[j];
                    break;
                }
            }

            if (charData) {
                // Generate G-code for each stroke in the character
                for (int k = 0; k < charData->strokeTotal; k++) {
                    int x = charData->strokeData[k][0];
                    int y = charData->strokeData[k][1];
                    int penState = charData->strokeData[k][2];

                    double adjustedX = xPos + x * scaleFactor;
                    double adjustedY = yPos + y * scaleFactor;

                    char gCodeLine[100];
                    if (penState == 0) {
                        sprintf(gCodeLine, "G0 X%.2f Y%.2f\n", adjustedX, adjustedY);
                    } else {
                        sprintf(gCodeLine, "G1 X%.2f Y%.2f\n", adjustedX, adjustedY);
                    }
                    printGCodeLine(gCodeLine);
                }

                // Update the horizontal position for the next character
                xPos += 10.0 * scaleFactor + CHAR_EXTRA_SPACING;
            }
        }

        // Add spacing between words
        xPos += 5.0 * scaleFactor;
    }

    fclose(file);
}

// Function to output G-code to the terminal
void printGCodeLine(char *gCodeLine) {
    printf("%s", gCodeLine);
}
