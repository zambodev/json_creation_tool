#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>


typedef enum _JCT_COMMAND_FLAGTYPEDEF
{
    JCT_CREATE = 1,
    JCT_ADD
} JCT_Command_FlagTypeDef;

typedef enum _JCT_DATATYPE_FLAG_TYPEDEF
{
    JCT_OBJECT = 1,
    JCT_STRING,
    JCT_NUMBER
} JCT_DataType_FlagTypeDef;

typedef struct _JCT_COMMAND_HANDLETYPEDEF
{
    int                         id;
    JCT_Command_FlagTypeDef     command;
    JCT_DataType_FlagTypeDef    dataType;
    int                         indent;
    int                         tokenArrayCount;
    int                         tokenArraySize;
    char                        *tokenArray;
} JCT_Command_HandleTypeDef;

struct
{
    JCT_Command_HandleTypeDef       **commandStack;
    int                             commandStackSize;
    int                             commandStackCount;
    int                             hOutFile;
    int                             hInFile;
} hJCT;

static void JCT_Command_Init(JCT_Command_HandleTypeDef *hCommand, int id, JCT_Command_FlagTypeDef command, int indent)
{    
    hCommand->id = id;
    hCommand->command = command;
    hCommand->dataType = 0;
    hCommand->indent = indent;
    hCommand->tokenArrayCount = 0;
    hCommand->tokenArraySize = 8;
    hCommand->tokenArray = (char *)calloc(8, sizeof(char[32]));

    fflush(stdout);
}

static void JCT_Command_AddToken(int id, char *token)
{
    if(hJCT.commandStack[id]->tokenArrayCount == hJCT.commandStack[id]->tokenArraySize)
    {
        char *newTokenArray = (char *)calloc(hJCT.commandStack[id]->tokenArraySize + 8, sizeof(char[32]));
        free(hJCT.commandStack[id]->tokenArray);
        hJCT.commandStack[id]->tokenArray = newTokenArray;
        hJCT.commandStack[id]->tokenArraySize += 8;
    }

    strcpy(hJCT.commandStack[id]->tokenArray + (hJCT.commandStack[id]->tokenArrayCount * 32), token);
    ++hJCT.commandStack[id]->tokenArrayCount;
}

static void JCT_Command_SetType(int id, JCT_DataType_FlagTypeDef type)
{
    hJCT.commandStack[id]->dataType = type;
}

void JCT_Init()
{
    hJCT.commandStackSize = 32;
    hJCT.commandStackCount = 0;
    hJCT.commandStack = (JCT_Command_HandleTypeDef **)calloc(32, sizeof(JCT_Command_HandleTypeDef *));
}

static void JCT_DeleteAllCommand(void)
{
    for(int i = 0; i < hJCT.commandStackCount; ++i)
    {
        free(hJCT.commandStack[i]->tokenArray);
        free(hJCT.commandStack[i]);
    }
}

static void JCT_AddCommand(JCT_Command_FlagTypeDef command, int indent)
{
    if(hJCT.commandStackCount == hJCT.commandStackSize)
    {
        JCT_Command_HandleTypeDef **newStack = (JCT_Command_HandleTypeDef **)calloc(hJCT.commandStackSize += 32, sizeof(JCT_Command_HandleTypeDef *));
        
        for(int i = 0; i < hJCT.commandStackCount; ++i)
        {
            newStack[i] = (JCT_Command_HandleTypeDef *)calloc(1, sizeof(JCT_Command_HandleTypeDef));
            memcpy(newStack[i], hJCT.commandStack[i], sizeof(hJCT.commandStack[i]));
        }

        JCT_DeleteAllCommand();
        free(hJCT.commandStack);
        hJCT.commandStack = newStack;
        hJCT.commandStackSize += 32;
    }

    hJCT.commandStack[hJCT.commandStackCount] = (JCT_Command_HandleTypeDef *)calloc(1, sizeof(JCT_Command_HandleTypeDef));
    JCT_Command_Init(hJCT.commandStack[hJCT.commandStackCount], hJCT.commandStackCount, command, indent);
    ++hJCT.commandStackCount;
}

void ParseCommand(char *command, int indent)
{
    if(strncmp(command, " ", 1) == 0)
        return;

#ifdef DEBUG
    printf("PRE FILTER: |%s|\n", command);
#endif

    char *ret;
    if((ret = strstr(command, "\"")) != NULL)
        command = strtok(ret, "\"");
    if(command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = 0;
    if(command[strlen(command) - 1] == ' ')
        command[strlen(command) - 1] = 0;

#ifdef DEBUG
    printf("PARSING: |%s|\n", command);
#endif

    if(strncmp(command, "create", 6) == 0)
    {
        JCT_AddCommand(JCT_CREATE, indent);
    }
    else if(strncmp(command, "add", 3) == 0)
    {
        JCT_AddCommand(JCT_ADD, indent);
    }
    else if(strncmp(command, "object", 6) == 0)
    {
        JCT_Command_SetType(hJCT.commandStackCount - 1, JCT_OBJECT);
    }
    else if(strncmp(command, "string", 6) == 0)
    {
        JCT_Command_SetType(hJCT.commandStackCount - 1, JCT_STRING);
    }
    else if(strncmp(command, "number", 6) == 0)
    {
        JCT_Command_SetType(hJCT.commandStackCount - 1, JCT_NUMBER);
    }
    else // Token
    {
        JCT_Command_AddToken(hJCT.commandStackCount - 1, command);
    }
}

void JCT_Parser(char *filename)
{
    char byte;
    char command[64];
    char commandCount = 0;
    int indentCount = 0;
    int loopSpaces = 0;

    hJCT.hInFile = open(filename, O_RDONLY);

    while(read(hJCT.hInFile, &byte, 1) == 1)
    {
        command[commandCount] = byte;
        ++commandCount;

        if(byte == ' ')
            ++indentCount;
        if(byte == '\n')
            indentCount = 0;

        if(((loopSpaces && byte == ' ') || (!loopSpaces && byte != ' ')) && byte != '\n')
            continue;        

        float indent = indentCount / 4.0;

        ParseCommand(command, (int)indent);
        memset(command, 0, 64);
        commandCount = 0;
    }

    close(hJCT.hInFile);
}

static char * GetIndentation(int lastIndent)
{
    if(lastIndent == 0)
        return NULL;

    char *spaces = (char *)calloc(lastIndent * 4, sizeof(char));
    memset(spaces, ' ', lastIndent * 4);

    return spaces;
}

void JCT_Compose(void)
{
    static int isFileCreated = 0;
    int lastIndent = 0;
    int incrementNeeded = 0;
    int closeBraces = 0;
    int openBraces = 0;
    int addComma = 0;

    for(int i = 0; i < hJCT.commandStackCount; ++i)
    {
        if(i > 0 && !isFileCreated)
            return;

        printf("COMMAND %d TYPE %d INDENT %d:\n", hJCT.commandStack[i]->command, hJCT.commandStack[i]->dataType, hJCT.commandStack[i]->indent);
        
        if(hJCT.commandStack[i]->indent > lastIndent)
        {
            ++lastIndent;
        }
        else if(hJCT.commandStack[i]->indent < lastIndent)
        {
            --lastIndent;
            closeBraces = 1;
        }
        else
        {
            addComma = 1;
        }

        if(addComma)
        {
            write(hJCT.hOutFile, ",", 1);
            addComma = 0;
        }
        write(hJCT.hOutFile, "\n", 1);

        char *indent = GetIndentation(lastIndent);        
        if(indent)
        {
            write(hJCT.hOutFile, indent, strlen(indent));
            if(closeBraces)
            {
                write(hJCT.hOutFile, "}\n", 2);
                write(hJCT.hOutFile, indent, strlen(indent));
                closeBraces = 0;
                --openBraces;
            }
            free(indent);
        }

        switch(hJCT.commandStack[i]->command)
        {
            case JCT_CREATE:
            {
                hJCT.hOutFile = open(hJCT.commandStack[i]->tokenArray, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                isFileCreated = 1;
                write(hJCT.hOutFile, "{", 1);
                ++openBraces;
            }
            break;
            case JCT_ADD:
            {
                switch(hJCT.commandStack[i]->dataType)
                {
                    case JCT_OBJECT:
                    {
                        char token[32];
                        strncpy(token, hJCT.commandStack[i]->tokenArray, 32);

                        write(hJCT.hOutFile, "\"", 1);
                        write(hJCT.hOutFile, token, strlen(token));
                        write(hJCT.hOutFile, "\": {", 4);
                        ++openBraces;
                    }
                    break;
                    case JCT_STRING:
                    {
                        char token[32];

                        strncpy(token, hJCT.commandStack[i]->tokenArray, 32);
                        write(hJCT.hOutFile, "\"", 1);
                        write(hJCT.hOutFile, token, strlen(token));
                        write(hJCT.hOutFile, "\": \"", 4);
                        strncpy(token, hJCT.commandStack[i]->tokenArray + (2 * 32), 32);
                        write(hJCT.hOutFile, token, strlen(token));
                        write(hJCT.hOutFile, "\"", 1);
                    }
                    break;
                    case JCT_NUMBER:
                    {
                        char token[32];

                        strncpy(token, hJCT.commandStack[i]->tokenArray, 32);
                        write(hJCT.hOutFile, "\"", 1);
                        write(hJCT.hOutFile, token, strlen(token));
                        write(hJCT.hOutFile, "\": ", 3);
                        strncpy(token, hJCT.commandStack[i]->tokenArray + (2 * 32), 32);
                        write(hJCT.hOutFile, token, strlen(token));
                    }
                    break;
                }
            }
            break;
        }
    }

    write(hJCT.hOutFile, "\n", 1);
    for(int i = 0; i < openBraces; ++i)
    {
        char *indent = GetIndentation(--lastIndent);
        if(indent)
        {
            write(hJCT.hOutFile, indent, strlen(indent));
        }
        write(hJCT.hOutFile, "}\n", 2); 
    }

    // Close the output file
    close(hJCT.hOutFile);
}

int main(void)
{
    JCT_Init();

    JCT_Parser("./Test/test.jct");

    puts("PRINTING OUT DATA STRUCTURE!\n");
    fflush(stdout);

    JCT_Compose();

    return 0;
}
