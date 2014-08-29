#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>


#define FILEPATH_MAX_SIZE 256
#define FILENAME_MAX_SIZE 30 
#define HOST_PORT 8001
// #define HOST_NAME "127.0.0.1"
// #define HOST_NAME "143.183.189.78"

int	fileSize = 0;
char    *ReadBuffer = NULL;
char    fileName1[FILENAME_MAX_SIZE];
char	destFilePath[FILEPATH_MAX_SIZE];

int MySocketHandler(int);
int readFromFile (int);
void extract_filename (char *);

// Main
int main(int argc, char** argv)
{
	int ret = 0;
	int err;
	int hsock;
	struct sockaddr_in my_addr;
	int file_desc;
	struct stat buf;

	if (argc != 4 ) {
		printf("\nFile_sender: \nUSAGE: ./file_sender <Dest IP> <FileName> <destination Directory> \n");
		return -1;
	}

	if (argv[2] == NULL) {
		printf("\nFile_sender: [[Error]] FileName NULL ! \n");
		return -1;
	}

	if ( strlen(argv[3]) > FILEPATH_MAX_SIZE ) {
		printf("\nFile_sender: [[Error]] Please, keep the Destination File Path lesser than : %d chars \n", FILEPATH_MAX_SIZE);
                return -1;
	}

        memset(fileName1, '\0', FILENAME_MAX_SIZE);
	memset(destFilePath, '\0', FILEPATH_MAX_SIZE);
	memcpy(destFilePath, argv[3], strlen(argv[3]));
	
        extract_filename(argv[2]);

        if (fileName1 == NULL) {
                printf("\n\nFile_sender: [[Error] extracting File Name from Path: %s \n", argv[2]);
                return -1;
        }

        printf("\n\n File_sender: [[Info]] Extracted File Name from Path: %s \n", fileName1);
	printf("\n\n File_sender: [[Info]] Destination Directory on the target machine: '%s' \n", destFilePath);

	// Initialize sockets and set any options    
	hsock = socket(AF_INET, SOCK_STREAM, 0);
	if(hsock == -1) {
        	printf("\n\nFile_sender: [[Error]] Error initializing socket %d\n", errno);
		return -1;
	}
    
	// Connect to the server
	my_addr.sin_family = AF_INET ;
	my_addr.sin_port = htons(HOST_PORT);
	    
	memset(&(my_addr.sin_zero), 0, 8);
	my_addr.sin_addr.s_addr = inet_addr(argv[1]);
	// my_addr.sin_addr.s_addr = inet_addr(HOST_NAME);

	if ( connect( hsock, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0 ) {
		printf("\n\nFile_sender: [[Error]] Error connecting socket %d\n", errno);
		return -1;
	}

	// Opening the file which needs to be transfered
	file_desc = open(argv[2], O_RDONLY);  
	if ( file_desc < 0 ) {
		printf("\n\nFile_sender: [[Error]] Unable to open file '%s' for read. Errno: %d \n", argv[2], errno);
		close(file_desc);
		return -1; 
	}

	fstat(file_desc, &buf);
	fileSize = buf.st_size;
	// printf("\n\nFile_sender: [[Info]] File: '%s' Size: %d\n", argv[2], fileSize);
	
	if(fileSize) {
		ReadBuffer = (char *)malloc(fileSize);
		if (ReadBuffer == NULL) {
			printf("\n\nFile_sender: [[Error]] Malloc Failed !\n");
			close(file_desc);
			close(hsock);
			return -1;
		}
	}

	// Open the File and Read the contents ...
	ret = ReadFromFile(file_desc);

	if( (ReadBuffer) && (ret == 0) ) {
		// printf("\n\nFile_sender: [[Info]] Data (File Contents) is READY to be sent ..");
	}
	else {
		printf("\n\nFile_sender:  [[Error]] Buffer EMPTY! - File contents not available \n");
	}

	MySocketHandler(hsock);

	printf("\n\nFile_sender: File_sender Done! : Check the Destination Machine \n\n ");
	
	// Clean-up
	close(hsock);
	free(ReadBuffer);
	return 0;
}



int MySocketHandler (int sock)
{
	char buffer[10];
	char buffer2[FILENAME_MAX_SIZE + 2];
	int bytecount;
	char tempBuf[10];
	int size;	
	int size2;
	
	memset(buffer, 0, 10);
	memset(tempBuf, 0, 10);
	

	// *********** Sending the size of the file ************
	sprintf(tempBuf, "%d", fileSize);
	size = (int)strlen(tempBuf);
	sprintf(buffer, "%d%s", size, tempBuf);
	// printf("\n\nFile_sender: [[Info]] Sending the FILESIZE: '%d' to the Server \n", fileSize);
	
	if ((bytecount = send(sock, buffer, size + 1, 0)) <= 0 ) {
		printf("\n\nFile_sender: [[Error]] Issue while sending to the data (SIZE of File) !! ErrCode: %d\n", errno);
		return -1;
	}

	// printf("\n\nFile_sender: [[Info]] Done with Sending the FILESIZE to the Server; Return Value: %d \n", bytecount);


	// *********** Sending the File name ***************
	memset(buffer2, 0, FILENAME_MAX_SIZE + 2);
	size2 = strlen(fileName1);
	if ( size2 < 10 )
		sprintf(buffer2, "0%d%s", size2, fileName1);
	else 
		sprintf(buffer2, "%d%s", size2, fileName1); 

	// printf("\n\nFile_sender: [[Info]] Sending the FileName with (strlen): '%s' to the Server \n", buffer2);

	if((bytecount = send(sock, buffer2, size2 + 2, 0)) < 0 ) {
		printf("\n\nFile_sender: [[Error]] Issue while sending to the data (File name) !! ErrCode: %d\n", errno);
		return -1;
	}

	// printf("\n\nFile_sender: [[Info]] Done with Sending the FILENAME to the Server; Return Value: %d \n", bytecount);



        // ********** Sending the Destination File Directory to Server *****************
        if((bytecount = send(sock, destFilePath, FILEPATH_MAX_SIZE, 0)) < 0 ) {
                printf("\n\nFile_sender: [[Error]] Issue while sending to the data (FILE)!! ErrCode: %d\n", errno);
                return -1;
        }

        // printf("\n\nFile_sender: [[Info]] Number of Sent bytes: %d\n", bytecount);



	// ********** Sending the FILE contents to Server *****************
	if (ReadBuffer) {
		if((bytecount = send(sock, ReadBuffer, fileSize, 0)) < 0 ) {
			printf("\n\nFile_sender: [[Error]] Issue while sending to the data (FILE)!! ErrCode: %d\n", errno);
			return -1;
		}
		// printf("\n\nFile_sender: [[Info]] Number of Sent bytes: %d\n", bytecount);		
	}
	else {
		printf("\n\nFile_sender: [[Error]] Buffer EMPTY!! Cannot send anything ...");
	}

	return 0;
}



// Reading from the File
int ReadFromFile (int file_desc)
{
	int bytes_read = 0;
    
	bytes_read = read (file_desc, ReadBuffer, fileSize);

	if (bytes_read == 0) {
		printf("\n\nFile_sender: [[Error]] No data read from file\n");
		close(file_desc);
                return -1;
        }
	else {
		// printf("\n\nFile_sender: [[Info]] Bytes Read : %d\n", bytes_read);
	}

	close(file_desc);
	return 0;
}


// Extracting the File Name
void extract_filename (char *str)
{
	// int     ch = '\\';
	int ch = '/';
	size_t  len;
	char   *pdest;

	// Search backwards for last backslash in filepath
	pdest = strrchr(str, ch);

	// if backslash not found in filepath
	if (pdest == NULL ) {
		printf("\n\n File_sender: [[Info]] extract_filename:\t%c not found\n", ch );
                pdest = str;  // The whole name is a file in current path?
	}
	else {
                pdest++; // Skip the backslash itself.
	}

	// extract filename from file path
	len = strlen(pdest);

        if ( len >= FILENAME_MAX_SIZE )
                printf("\n\n: File_sender [[Error]] Please Keep the FileName Size Lesser than or Equal to %d Chars; Current File Name Size is: %d \n FILENAME: '%s' \n", FILENAME_MAX_SIZE, (int)len, pdest);
        else
                strncpy(fileName1, pdest, len+1);   // Copy including zero.

}

// END
