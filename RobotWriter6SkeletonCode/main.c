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

void convertTextToGCode(const char *filename, FontCharacter *fontArray, int characterCount, double scaleFactor) 
{
    FILE *file = fopen(filename, "r"); //Open the input file
    if (!file) 
    {
        printf("Error: Unable to open text file %s\n", filename); //Print error if file cannot be opened
        return;
    }

    char word[100]; //Buffer to store each word from the file
    double xPos = 0; //Horizontal position for drawing
    double yPos = 0; //Vertical position for drawing
    int charRead; //Variable to store characters read from the file

    while ((charRead = fgetc(file)) != EOF) //Read the file character by character using fgets
    {
        if (charRead == 10) 
        { 
            xPos = 0; //Reset horizontal position
            yPos -= LINE_SPACING_MM + 10; //Move to the next line
            printf("Line break. Moving to next line at Y position %.2f\n", yPos); //Notif about the line break
            continue;
        } 
        else if (charRead == 13) 
        { 
            xPos = 0; //Reset horizontal position
            continue;
        }

        ungetc(charRead, file); //Push back the character if it is part of a word

        if (fscanf(file, "%99s", word) != 1) //Read the next word from the file
        {
            break; //Exit loop if no valid word is found
        }

        printf("Processing word: %s\n", word); //log the word being processed

        double wordWidth = 0.0; //Calculate the width of the word
        for (int i = 0; word[i] != '\0'; i++) 
        {
            FontCharacter *charData = NULL;

            for (int j = 0; j < characterCount; j++) //Find the font data for the character
            {
                if (fontArray[j].asciiCode == word[i]) 
                {
                    charData = &fontArray[j];
                    break;
                }
            }

            if (charData) 
            {
                wordWidth += 15.0 * scaleFactor; //Accumulate character widths
            }
        }
        wordWidth += 5.0 * scaleFactor; //Add extra spacing after the word

        if (xPos + wordWidth > MAX_LINE_WIDTH_MM) //Check if the word fits within the crrent line
        {
            xPos = 0;
            yPos -= LINE_SPACING_MM + 10; 
        }

        for (int i = 0; word[i] != '\0'; i++) //Generate G-code for each character in the word
        {
            FontCharacter *charData = NULL;

            for (int j = 0; j < characterCount; j++) //Find the font data for the character
            {
                if (fontArray[j].asciiCode == word[i]) 
                {
                    charData = &fontArray[j];
                    break;
                }
            }

            if (charData) 
            {
                for (int k = 0; k < charData->strokeTotal; k++) //generate G-code for each stroke in the character
                {
                    int x = charData->strokeData[k][0]; //X offset for the stroke
                    int y = charData->strokeData[k][1]; //Y offset for the stroke
                    int penState = charData->strokeData[k][2]; //Pen state (up or down)

                    double adjustedX = xPos + x * scaleFactor;
                    double adjustedY = yPos + y * scaleFactor; 

                    char buffer[100]; //Buffer to store the G-code command
                    if (penState == 0) 
                    { 
                        sprintf(buffer, "G0 X%.2f Y%.2f\n", adjustedX, adjustedY); //Change to S0 when using the robot
                    } 
                    else 
                    { 
                        sprintf(buffer, "G1 X%.2f Y%.2f\n", adjustedX, adjustedY); //Change to S1000 when using the robot
                    }
                    printGCodeLine(buffer); //Output the G-code to the terminal
                    SendCommands(buffer); //Send the G-code to the robot
                }

                xPos += 15.0 * scaleFactor; //Increment horizontal position after processing the character
            }
        }

        xPos += 5.0 * scaleFactor; //Add extra spacing after the word
    }

    fclose(file); 
}
