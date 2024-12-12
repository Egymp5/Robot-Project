#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
//#include "serial.h"

#define baud_rate 115200 //The baud rate for serial communication
#define MAX_ASCII 128 //Maximum number of ASCII characters supported
#define LINE_SPACING_MM 5.0 //Line spacing in mm for text output
#define MAX_LINE_WIDTH_MM 100.0 //Maximum line width in mm for text output
#define FONT_MARKER 999 //Marker used to identify font data in the input file

typedef struct
{
    int asciiCode; //ascii code of the character
    int strokeTotal; //Number of strokes required to draw the character
    int (*strokeData)[3]; //Pointer to the stroke data, where each stroke has three components
} FontCharacter; //structure to hold character data

void SendCommands(char *buffer); //Function to send G-code commnds to the robot
double promptTextHeight(); //Function to prompt the user for text height input
double computeScaleFactor(double textHeight); //Function to calculate the scale factor for text height
FontCharacter* loadFont(const char *filename, int *characterCount); //Function to load font data from a file into memory
void convertTextToGCode(const char *filename, FontCharacter *fontArray, int characterCount, double scaleFactor); //Function to process the text file and generate G-code
void printGCodeLine(char *buffer); //Function to print G code commands to the terminal
double calculateWordWidth(const char* word, FontCharacter *fontArray, int characterCount, double scaleFactor) 

int main()
{
    const char *fontFilePath="SingleStrokeFont.txt"; //Name of font file
    const char *inputTextPath="RobotTesting.txt"; //Name of text path
    double textHeight, scaleFactor; //Define text height and scalefactor
    int characterCount=0; 

    //Load font data into memory
    FontCharacter *fontArray=loadFont(fontFilePath, &characterCount);
    if (!fontArray)
    {
        return 1; //If font loading fails, exit 
    }
    printf("Loaded %d characters from font file. \n", characterCount);
    
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    //Calls TextHeight function
    textHeight=promptTextHeight(); 

    //Calls ScaleFactor function
    scaleFactor=computeScaleFactor(textHeight); 
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
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

    //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

    //Call processTextFileTest function
    convertTextToGCode(inputTextPath, fontArray, characterCount, scaleFactor);
    
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

double promptTextHeight()
{
    double textHeight = 0.0; //User-provided text height
    int isInputValid = 0; //Flag to track whether input is valid

    do {
        printf("Enter text height in mm (between 4 and 10 mm): "); //Prompt user for text height

        if (scanf("%lf", &textHeight) != 1) //Check if input is valid
        {
            printf("Error: Please enter a numeric value.\n"); //Print error for invalid input
            while (getchar() != '\n'); 
        } 
        else if (textHeight < 4.0 || textHeight > 10.0) //Check if input is within range
        {
            printf("Error: Text height must be between 4 and 10 mm.\n"); //Print error for out-of-range input
        } 
        else 
        {
            isInputValid = 1; //Mark input as valid
        }
    } while (!isInputValid); //Repeat until valid input is provided

    return textHeight; 
}

double computeScaleFactor(double textHeight) 
{
    return textHeight / 18.0; //Scale factor calculated 
}

void SendCommands(char *buffer)
{
    PrintBuffer(&buffer[0]); // Print the buffer to the robot
    WaitForReply(); //Wait for the robot's response
    Sleep(100); //Add a slight delay for command execution
}

// Helper function to calculate word width
double calculateWordWidth(const char* word, FontCharacter *fontArray, int characterCount, double scaleFactor) 
{
    double wordWidth = 0.0;
    for (int i = 0; word[i] != '\0'; i++) 
    {
        for (int j = 0; j < characterCount; j++) 
        {
            if (fontArray[j].asciiCode == word[i]) 
            {
                wordWidth += 15.0 * scaleFactor;
                break;
            }
        }
    }
    wordWidth += 5.0 * scaleFactor;
    return wordWidth;
}

// Main conversion function
void convertTextToGCode(const char *filename, FontCharacter *fontArray, int characterCount, double scaleFactor) 
{
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        printf("Error: Unable to open text file %s\n", filename);
        return;
    }

    double xPos = 0;
    double yPos = 0;
    char word[100];
    int charRead;

    while ((charRead = fgetc(file)) != EOF) 
    {
        // Skip line feeds and carriage returns
        if (charRead == 10 || charRead == 13) 
        {
            if (charRead == 10) 
            {
                xPos = 0;
                yPos -= LINE_SPACING_MM + 10;
                printf("Line break. Moving to next line at Y position %.2f\n", yPos);
            }
            continue;
        }

        // Rewind and read the full word
        ungetc(charRead, file);
        if (fscanf(file, "%99s", word) != 1) 
        {
            break;
        }

        printf("Processing word: %s\n", word);

        // Calculate word width using the helper function
        double wordWidth = calculateWordWidth(word, fontArray, characterCount, scaleFactor);

        // Line wrapping logic
        if (xPos + wordWidth > MAX_LINE_WIDTH_MM) 
        {
            xPos = 0;
            yPos -= LINE_SPACING_MM + 10;
        }

        // G-code generation
        for (int i = 0; word[i] != '\0'; i++) 
        {
            FontCharacter *charData = NULL;

            // Find character data
            for (int j = 0; j < characterCount; j++) 
            {
                if (fontArray[j].asciiCode == word[i]) 
                {
                    charData = &fontArray[j];
                    break;
                }
            }

            // Only process if character is found
            if (charData) 
            {
                // Direct G-code generation within the same loop
                for (int k = 0; k < charData->strokeTotal; k++) 
                {
                    int x = charData->strokeData[k][0];
                    int y = charData->strokeData[k][1];
                    int penState = charData->strokeData[k][2];

                    double adjustedX = xPos + x * scaleFactor;
                    double adjustedY = yPos + y * scaleFactor; 

                    char buffer[100];
                    if (penState == 0) 
                    { 
                        sprintf(buffer, "G0 X%.2f Y%.2f\n", adjustedX, adjustedY);
                    } 
                    else 
                    { 
                        sprintf(buffer, "G1 X%.2f Y%.2f\n", adjustedX, adjustedY);
                    }
                    printGCodeLine(buffer);
                    SendCommands(buffer);
                }

                // Horizontal positioning after character
                xPos += 15.0 * scaleFactor;
            }
        }

        // Word spacing
        xPos += 5.0 * scaleFactor;
    }

    fclose(file); 
}
