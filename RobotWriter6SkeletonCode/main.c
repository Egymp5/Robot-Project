#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
//#include "serial.h"

#define baud_rate 115200               /* 115200 baud */
#define MAX_ASCII 128
#define LINE_SPACING_MM 5.0
#define MAX_LINE_WIDTH_MM 100.0
#define FONT_MARKER 999

// Structure to store font data for each ASCII value
typedef struct
{
    int asciiCode;
    int strokeTotal;
    int (*strokeData)[3];
}  FontCharacter;

// Function declarations
void SendCommands (char *buffer );
double promptTextHeight();
double computeScaleFactor(double textHeight);
FontCharacter* loadFont(const char *filename, int *characterCount);
void convertTextToGCode(const char *filename, FontCharacter *fontArray, int characterCount, double scaleFactor);
void printGCodeLine(char *buffer);

int main()
{
    const char *fontFilePath="SingleStrokeFont.txt"; // Name of font file
    const char *inputTextPath="RobotTesting.txt";
    double textHeight, scaleFactor; 
    int characterCount=0;

    // Load font data into memory
    FontCharacter *fontArray=loadFont(fontFilePath, &characterCount);
    if (!fontArray)
    {
        return 1; // If font loading fails, exit 
    }
    printf("Loaded %d characters from font file. \n", characterCount);
    char buffer[100];
    textHeight=promptTextHeight(); //Calls function to get text height
    scaleFactor=computeScaleFactor(textHeight); //Calculates scale factor
    printf("Calculated scale factor: %.4f\n", scaleFactor);
    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }
    
    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");
    
    // We do this by sending a new-line
    sprintf (buffer, "\n");
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

    // These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    printf("%s", buffer); // Print the G-code to the terminal
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

    // Call convertTextToGCode function
    convertTextToGCode(inputTextPath, fontArray, characterCount, scaleFactor);

    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Function to prompt the user for text height input
double promptTextHeight()
{
    double textHeight = 0.0; //user-provided text height
    int isInputValid = 0; //flag for valid input 

//loop to repeatedly prompt user if invalid input is entered
    do {
        printf("Enter text height in mm (between 4 and 10 mm): ");

        if (scanf("%lf", &textHeight) != 1) // Check if the input is valid
        {
            printf("Error: Please enter a numeric value.\n");
            while (getchar() != '\n');    //clear input buffer to remove invalid chars
        } else if (textHeight < 4.0 || textHeight > 10.0) 
        {
            printf("Error: Text height must be between 4 and 10 mm.\n");  //error message for invalid input
        } else 
        {
            isInputValid = 1; // Input is valid
        }

    } while (!isInputValid); //continue loop till valid input provided

    return textHeight;
}

// Function to calculate the scale factor
double computeScaleFactor(double textHeight) 
{
    return textHeight/18.0; //Scales text appropriately
}

//Function to send G-code commands to the robot
void SendCommands (char *buffer )
{
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
}

// Function to print G-code to the terminal
void printGCodeLine(char *buffer) 
{
    printf("%s", buffer); // Print the buffer content to the terminal
}


// Function to load font data from a file
// Function to load font data from a file
FontCharacter* loadFont(const char *filename, int *characterCount) 
{
    // Open the font file in read mode
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        // If the file could not be opened, print an error message and return NULL
        printf("Error: Could not open font file %s\n", filename);
        return NULL;
    }

    // Allocate memory for an array of FontCharacter structures
    FontCharacter *fontArray = malloc(MAX_ASCII * sizeof(FontCharacter));
    if (!fontArray) 
    {
        // If memory allocation fails, print an error message, close the file, and return NULL
        printf("Error: Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }

    // Initialise the character count to zero
    *characterCount = 0;

    // Buffer to store each line read from the file
    char line[256];

    // Loop to read each line from the file using fgets
    while (fgets(line, sizeof(line), file)) 
    {
        int marker, asciiCode, strokeTotal;

        // Parse the line to extract the marker, ASCII code, and stroke count
        if (sscanf(line, "%d %d %d", &marker, &asciiCode, &strokeTotal) != 3 || marker != 999) 
        {
            // Exit the loop if the line format is invalid or the marker is incorrect
            break;
        }

        // Get a pointer to the current FontCharacter structure in the array
        FontCharacter *currentChar = &fontArray[*characterCount];
        // Store the ASCII code for the character
        currentChar->asciiCode = asciiCode;
        // Store the number of strokes required to draw the character
        currentChar->strokeTotal = strokeTotal;

        // Allocate memory for the stroke data
        currentChar->strokeData = malloc(strokeTotal * sizeof(int[3]));
        if (!currentChar->strokeData) 
        {
            // If memory allocation for strokes fails, print an error message, close the file, and return NULL
            printf("Error: Memory allocation for strokes failed.\n");
            fclose(file);
            return NULL;
        }

        // Loop to read the strokes for the current character
        for (int i = 0; i < strokeTotal; i++) 
        {
            // Read the next line from the file for stroke data
            fgets(line, sizeof(line), file);
            // Parse the line to extract the stroke data (x, y, and pen state)
            sscanf(line, "%d %d %d", &currentChar->strokeData[i][0],
                   &currentChar->strokeData[i][1],
                   &currentChar->strokeData[i][2]);
        }

        // Increment the character count after successfully processing one character
        (*characterCount)++;
    }

    // Close the file after reading all data
    fclose(file);

    // Return the pointer to the array of FontCharacter structures
    return fontArray;
}



// Function to process the text file 
// Function to convert text file into G-code commands
void convertTextToGCode(const char *filename, FontCharacter *fontArray, int characterCount, double scaleFactor) 
{
    // Open the input file for reading
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        // Print an error if the file cannot be opened
        printf("Error: Unable to open text file %s\n", filename);
        return;
    }

    char word[100];       // Buffer to store each word from the file
    double xPos = 0;      // Horizontal position for drawing
    double yPos = 0;      // Vertical position for drawing
    int charRead;         // Variable to store characters read from the file

    // Read the file character by character
    while ((charRead = fgetc(file)) != EOF) 
    {
        // Handle Line Feed (LF) to move to the next line
        if (charRead == 10) 
        { 
            xPos = 0;                     // Reset horizontal position
            yPos -= LINE_SPACING_MM + 10; // Move to the next line
            printf("Line break. Moving to next line at Y position %.2f\n", yPos);
            continue;
        } 
        // Handle Carriage Return (CR) to reset horizontal position
        else if (charRead == 13) 
        { 
            xPos = 0; // Reset horizontal position
            printf("Resetting X position to %.2f\n", xPos);
            continue;
        }

        // Push back the character if it is part of a word
        ungetc(charRead, file);

        // Read the next word from the file
        if (fscanf(file, "%99s", word) != 1) 
        {
            // Break the loop if no valid word is found
            break;
        }

        printf("Processing word: %s\n", word); // Log the word being processed

        // Calculate the width of the word
        double wordWidth = 0.0;
        for (int i = 0; word[i] != '\0'; i++) 
        {
            FontCharacter *charData = NULL;

            // Find the font data for the character
            for (int j = 0; j < characterCount; j++) 
            {
                if (fontArray[j].asciiCode == word[i]) 
                {
                    charData = &fontArray[j];
                    break;
                }
            }

            if (charData) 
            {
                // Increment word width by the character width
                wordWidth += 15.0 * scaleFactor;
            }
        }
        // Add extra spacing after the word
        wordWidth += 5.0 * scaleFactor;

        // Check if the word fits within the current line
        if (xPos + wordWidth > MAX_LINE_WIDTH_MM)
        {
            xPos = 0;                     // Reset horizontal position
            yPos -= LINE_SPACING_MM + 10; // Move to the next line
            printf("Word exceeds line width. Moving to next line at Y position %.2f\n", yPos);
        }

        // Generate G-code for each character in the word
        for (int i = 0; word[i] != '\0'; i++) 
        {
            FontCharacter *charData = NULL;

            // Find the font data for the character
            for (int j = 0; j < characterCount; j++) 
            {
                if (fontArray[j].asciiCode == word[i]) 
                {
                    charData = &fontArray[j];
                    break;
                }
            }

            if (charData) 
            {
                // Generate G-code for each stroke in the character
                for (int k = 0; k < charData->strokeTotal; k++) 
                {
                    int x = charData->strokeData[k][0]; // X offset for the stroke
                    int y = charData->strokeData[k][1]; // Y offset for the stroke
                    int penState = charData->strokeData[k][2]; // Pen state (up or down)

                    // Adjust stroke coordinates based on the scale factor
                    double adjustedX = xPos + x * scaleFactor;
                    double adjustedY = yPos + y * scaleFactor;

                    char buffer[100]; // Buffer to store the G-code command
                    if (penState == 0) 
                    { 
                        // Pen up command
                        sprintf(buffer, "G0 X%.2f Y%.2f\n", adjustedX, adjustedY);
                    } 
                    else 
                    { 
                        // Pen down command
                        sprintf(buffer, "G1 X%.2f Y%.2f\n", adjustedX, adjustedY);
                    }
                    printGCodeLine(buffer); // Output the G-code to the terminal
                    SendCommands(buffer);   // Send the G-code to the robot
                }

                // Increment the horizontal position after processing the character
                xPos += 15.0 * scaleFactor;
            }
        }

        // Add extra spacing after the word
        xPos += 5.0 * scaleFactor;
    }

    fclose(file); // Close the file
}
