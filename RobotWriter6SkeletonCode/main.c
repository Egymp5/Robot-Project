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


//Main function to process a text file and convert it into G-code commands
void convertTextToGCode(const char *filename, FontCharacter *fontArray, int characterCount, double scaleFactor) 
{
    FILE *file = fopen(filename, "r"); //Open the text file for reading
    if (!file) 
    {
        printf("Error: Unable to open text file %s\n", filename); //Error message if the file cannot be opened
        return; 
    }

    double xPos = 0; //Current X-coordinate position for text drawing
    double yPos = 0; //Current Y-coordinate position for text drawing
    char word[100]; // uffer to hold words read from the file
    int ch; //Variable to store each character read from the file

    // Define states for processing text 
    enum ProcessingState
     {
        SEEKING_WORD,       //State where the program looks for the next word
        PROCESSING_WORD,    //State where the program processes a found word
        HANDLING_NEWLINE    //State for handling new lines
    } 
    state = SEEKING_WORD; //Initialise the state to SEEKING_WORD

    while ((ch = fgetc(file)) != EOF) //Read the file character by character until end of file
    {
        switch (state) 
        {
            case SEEKING_WORD:
                //Skip non-printable characters and spaces
                if (ch <= 32) 
                {
                    if (ch == 10) //Check if the character is a newline
                    {
                        xPos = 0; //Reset X position for the new line
                        yPos -= LINE_SPACING_MM + 10; // Move down the Y poition for the next line
                        printf("Line break. Moving to next line at Y position %.2f\n", yPos);
                    }
                    continue; 
                }

                //If a word is found, process it
                ungetc(ch, file); //Put the character back into the stream for word reading

                if (fscanf(file, "%99s", word) != 1) //Attmpt to read a word into the buffer
                {
                    break; 
                }

                printf("Processing word: %s\n", word); //Log the word being processed
                state = PROCESSING_WORD; //Transition to the PROCESSING_WORD state
                break;

            case PROCESSING_WORD:
                {
                    double wordWidth = calculateWordWidth(word, fontArray, characterCount, scaleFactor); //Calculate the word's width

                    if (xPos + wordWidth > MAX_LINE_WIDTH_MM) //Check if the word exceeds the line width
                    {
                        xPos = 0; 
                        yPos -= LINE_SPACING_MM + 10; 
                    }

                    //Iterate through each character in the word
                    for (int i = 0; word[i] != '\0'; i++) 
                    {
                        FontCharacter *charData = NULL; //Pointer to store character data

                        for (int j = 0; j < characterCount; j++) 
                        {
                            if (fontArray[j].asciiCode == word[i]) //Find the corresponding font data for the character
                            {
                                charData = &fontArray[j];
                                break;
                            }
                        }

                        if (charData) //If character data is found, generate G-code for it
                        {
                            for (int k = 0; k < charData->strokeTotal; k++) 
                            {
                                int x = charData->strokeData[k][0]; //X coordinate for the stroke
                                int y = charData->strokeData[k][1]; //Y coordinate for the stroke
                                int penState = charData->strokeData[k][2]; // Pen state (up/down)

                                double adjustedX = xPos + x * scaleFactor; //Adjust X coordinate by scale factor
                                double adjustedY = yPos + y * scaleFactor; //Adjust Y coordinate by scale factor

                                char buffer[100]; 

                                if (penState == 0) //Pen up movement
                                { 
                                    sprintf(buffer, "G0 X%.2f Y%.2f\n", adjustedX, adjustedY); //Change to S0 when using the robot
                                } 
                                else //Pen down movement.
                                { 
                                    sprintf(buffer, "G1 X%.2f Y%.2f\n", adjustedX, adjustedY); //Change to S1000 when using the robot
                                }
                                printGCodeLine(buffer); //Print the G-code line for debuggin
                                SendCommands(buffer); //Send the command to the robot
                            }
                            xPos += 15.0 * scaleFactor; //Update the X position for the next character
                        }
                    }
                    xPos += 5.0 * scaleFactor; //Add spacing after the word
                    state = SEEKING_WORD; //Return to SEEKING_WORD state for the next word
                }
                break;
        }
    }
    fclose(file); 
}

//Helper function to calculate word width (not necessary)
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
