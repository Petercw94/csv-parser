#include "../include/Parser.h"

#define LINE_ENDING '\n'
#define ON 1
#define OFF 0

/*
 * c_count -> the character count of the column
 * c_string -> a pointer to the character string representation of
 * */ 
char* parseColumn(FILE* fp, int c_count)
{
    int c;
    int offset = -c_count;
    offset--; // remove one from the offset to move cursor correctly

    // allocate memory for the column string
    char* s = (char*) malloc((c_count + 1) * sizeof(char));
    if (s == NULL) {
        fprintf(stderr, "Error allocating memory for column string.\n");
    }
    int error = fseek(fp, offset, SEEK_CUR);
    if (error != 0) {
        fprintf(stderr, "Error adjusting the File Pointer while parsing the column\n");
        exit(EXIT_FAILURE);
    }
    // TODO : replace the printing of the column with writing that column to a string 
    // literal stored in column array (requires a new parameter: string literal pointer)
    for (int i = 0; i<c_count; ++i) {
        c = fgetc(fp);
        s[i] = c;
    }
    s[c_count] = '\0'; // terminate the string
    return s;
}

void parseRow(FILE* fp)
{
    
    int c, colStateQuotes, prevChar, colCharCount, error;
    colStateQuotes = OFF;
    
    colCharCount = prevChar = 0;

	while ((c=fgetc(fp)) != LINE_ENDING) {
		

        /* if line ending or end of file hit, parse the column then break the loop */
        if (c == EOF) {
            parseColumn(fp, colCharCount);
            break;
        }
        /* If a " appears and we aren't currently in a 
         * column quote state, it indicates that its the 
         * beginning of a new col that is surrounded by
         * quotes. See RFC standards for explanation */
        else if (c == '"' && colStateQuotes == 0) {
            colStateQuotes = ON; // this state indicates whether we are currently in a data state or not
        }

        else if (c == ',') {
            /* if a comma appears and we aren't in a quoted data column,
             * it would indicate that its the end of the column */
            if (colStateQuotes == 0) {
                char* column = parseColumn(fp, colCharCount);
                printf("%s", column);
                free(column);
                printf("%c", ' ');
                colCharCount = 0; // reset the char counter for the next col
                error = fseek(fp, 1, SEEK_CUR);
                if (error != 0) {
                    fprintf(stderr, "Error advancing file pointer after parsing column.\n");
                    exit(EXIT_FAILURE);
                }
            }

            /* If a comma appears and the program is in the quoted 
             * data col state, a check needs to be made to the previous
             * character. If the previous character is a double quote, it would 
             * signify the end of the data state, which would also
             * indicate the end of the column. */
            else if (prevChar == '"') {
                char* column = parseColumn(fp, colCharCount);
                printf("%s", column);
                free(column);
                printf("%c", ' ');
                colCharCount = 0; // reset the char counter for the next col
                error = fseek(fp, 1, SEEK_CUR);
                if (error != 0) {
                    fprintf(stderr, "Error advancing file pointer after parsing column.\n");
                    exit(EXIT_FAILURE);
                }
            }
        
            
            /* If the character does not signify the end of the column
             * it needs to be counted so the parseCol function knows how 
             * far back it needs to move the file pointer to allocate enough
             * memory and parse the column value*/
            else {
                ++colCharCount;
            }
        }

        else {
            ++colCharCount;
        }
        prevChar = c;
	}
    char* column = parseColumn(fp, colCharCount);
    printf("%s", column);
    printf("\n");
    free(column);
}

