#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rs232.h"
#include "serial.h"

// Constants for robot operation
#define BAUD_RATE 115200        // Baud rate for RS232 communication
#define MAX_ASCII 128           // Total number of ASCII characters supported
#define LINE_SPACING_MM 12.0    // Space between lines in mm
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
void SendCommands(char *buffer);

int main() {
    const char *fontFilePath = "SingleStrokeFont.txt"; // Path to the font data file
    const char *inputTextPath = "test.txt";            // Path to the input text file
    double textHeight, scaleFactor;
    int characterCount = 0;
    char buffer[100]; // Buffer to store commands

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

    // Open the COM port for communication
    if (CanRS232PortBeOpened() == -1) {
        printf("\nUnable to open the COM port (specified in serial.h)\n");
        cleanupFontMemory(fontArray, characterCount);
        return 1; // Exit if COM port cannot be opened
    }

    // Wake up the robot by sending a new-line character
    printf("\nAbout to wake up the robot\n");
    sprintf(buffer, "\n");
    PrintBuffer(&buffer[0]);
    Sleep(100);  // Pause for 100 milliseconds

    // Wait for the robot to acknowledge with a dollar sign ($)
    WaitForDollar();
    printf("\nThe robot is now ready to draw\n");

    // Send commands to prepare the robot for drawing
    sprintf(buffer, "G1 X0 Y0 F1000\n");  // Move to the starting position
    SendCommands(buffer);
    sprintf(buffer, "M3\n");              // Turn on the spindle
    SendCommands(buffer);
    sprintf(buffer, "S0\n");              // Set speed to 0 (initial state)
    SendCommands(buffer);

    // Convert the input text into G-code using the loaded font
    convertTextToGCode(inputTextPath, fontArray, characterCount, scaleFactor);

    // Close the COM port before exiting
    CloseRS232Port();
    printf("Com port now closed\n");

    // Clean up allocated memory for the font data
    cleanupFontMemory(fontArray, characterCount);
    printf("G-code generation completed successfully.\n");

    return 0;
}

// Function to send the commands to the robot
void SendCommands(char *buffer) {
    PrintBuffer(buffer);
    WaitForReply();  // Wait for the robot to acknowledge each command
    Sleep(100);      // Add a short delay to ensure the command is processed properly
}

// Function to read the font data file and populate the font array
FontCharacter* loadFont(const char *fontPath, int *characterCount) {
    FILE *file = fopen(fontPath, "r");
    if (!file) {
        printf("Error: Could not open font file: %s\n", fontPath);
        return NULL;
    }

    // Allocate memory for the font array
    FontCharacter *fontArray = malloc(MAX_ASCII * sizeof(FontCharacter));
    if (!fontArray) {
        printf("Error: Memory allocation failed for font data.\n");
        fclose(file);
        return NULL;
    }

    *characterCount = 0; // Initialise the number of loaded characters

    // Read font data from the file until the end is reached
    while (!feof(file)) {
        int marker, asciiCode, strokeTotal;

        // Read marker, ASCII code, and the number of strokes for the character
        if (fscanf(file, "%d %d %d", &marker, &asciiCode, &strokeTotal) != 3 || marker != 999) {
            break;  // Stop if the format is incorrect or end of the file is reached
        }

        // Populate the font character details
        FontCharacter *currentChar = &fontArray[*characterCount];
        currentChar->asciiCode = asciiCode;
        currentChar->strokeTotal = strokeTotal;

        // Allocate memory for the stroke data
        currentChar->strokeData = malloc(strokeTotal * sizeof(int[3]));
        if (!currentChar->strokeData) {
            printf("Error: Memory allocation for character strokes failed.\n");
            cleanupFontMemory(fontArray, *characterCount);
            fclose(file);
            return NULL;
        }

        // Read the strokes that define this character
        for (int i = 0; i < strokeTotal; i++) {
            fscanf(file, "%d %d %d", &currentChar->strokeData[i][0],
                   &currentChar->strokeData[i][1],
                   &currentChar->strokeData[i][2]);
        }

        (*characterCount)++;  // Increment the character count
    }

    fclose(file); // Close the font file
    return fontArray;
}

// Function to free the allocated memory for font data
void cleanupFontMemory(FontCharacter *fontArray, int characterCount) {
    for (int i = 0; i < characterCount; i++) {
        free(fontArray[i].strokeData);  // Free the stroke data for each character
    }
    free(fontArray); // Free the array itself
}

// Function to prompt the user to enter the desired text height
double promptTextHeight() {
    double height;
    while (1) {
        printf("Enter text height (in mm, between 4 and 10): ");
        if (scanf("%lf", &height) == 1 && height >= 4 && height <= 10) {
            return height;  // Return valid height
        }
        printf("Invalid input. Please enter a numeric value between 4 and 10.\n");
        while (getchar() != '\n'); // Clear input buffer in case of invalid input
    }
}

// Function to calculate the scale factor based on the text height
double computeScaleFactor(double textHeight) {
    return textHeight / 18.0;  // Scale based on a reference height of 18 mm
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

    // Process the text file character by character
    while ((charRead = fgetc(file)) != EOF) {
        if (charRead == '\n') { // Handle line breaks
            xPos = 0.0;                 // Reset X position for a new line
            yPos -= LINE_SPACING_MM;    // Move down to the next line
            printf("Line break. Moving to Y position: %.2f\n", yPos);
            continue;
        } else if (charRead == '\r') { // Handle carriage returns
            xPos = 0.0; // Reset X position for a carriage return
            continue;
        }

        ungetc(charRead, file); // Put the character back for word processing

        if (fscanf(file, "%99s", word) != 1) {
            break; // Stop if there's no word to read
        }
        printf("Processing word: %s\n", word);

        // Calculate the width of the word based on character widths
        double wordWidth = 0.0;
        for (int i = 0; word[i] != '\0'; i++) {
            wordWidth += 10.0 * scaleFactor + CHAR_EXTRA_SPACING;
        }

        // Check if the word fits in the current line
        if (xPos + wordWidth > MAX_LINE_WIDTH_MM) {
            xPos = 0.0;               // Reset horizontal position
            yPos -= LINE_SPACING_MM;  // Move to the next line
            printf("Word exceeds line width. Moving to Y position: %.2f\n", yPos);
        }

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

            // Generate G-code for the character's strokes
            if (charData) {
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

    fclose(file); // Close the text file
}

// Function to output G-code to the terminal
void printGCodeLine(char *gCodeLine) {
    printf("%s", gCodeLine); // Print the G-code line to the terminal
}
