#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

//	This script:

//	Accepts the following arguments: the first argument is a full path to a file (including filename) on the filesystem, referred to below as writefile; the second argument is a text string which will be written within this file, referred to below as writestr

//   Exits with value 1 error and print statements if any of the arguments above were not specified

//   Creates a new file with name and path writefile with content writestr, overwriting any existing file. Exits with value 1 and error print statement if the file could not be created.

//   Syslog is used to log errors and writes with the LOG_USER facility

int main(int argc, char *argv[]){
	//Start a logger
	openlog("Assignment2", 0, LOG_USER);
	
	//Check that the right amount of arguments was provided
	if (argc != 3){
		syslog(LOG_ERR, "Error: You must provide exactly 2 arguments.");
		closelog();
		return 1;
	}
	
	//Assign arguments to writefile and writestr
	char* writefile = argv[1];
	char* writestr = argv[2];
	int file = 0;
	size_t count;
	ssize_t nr;	
	

	//Open a new file with O_WRONLY, O_CREAT, O_TRUNC 
	//O_TRUNC - If the file exists, the file will be truncated to 0 length
	//O_CREAT - if the file does not exist, the kernel will create it
	//Permissions 0644 = readable and writable by the owner, group members and other users read-only
	file = open (writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	
	//Check if the file was able to be opened 
	if (file == -1){
		syslog(LOG_ERR, "Error: Failed to open file: %s", writefile);
		closelog();
		return 1;
	}
	
	//Write string to file
	count = strlen(writestr);
	nr = write (file, writestr, count);

	//Check for errors writing string
	if (nr == -1){
		syslog(LOG_ERR, "Error: Unable to write to file %s, error code %s", writefile, strerror(errno));
		close(file);
		closelog();
		return 1;
	}

	//Check if able to write whole string
	else if (nr != count){
		syslog(LOG_ERR, "Error: Unable to write full string to file: %s", writefile);
		close(file);
		closelog();
		return 1;
	}


	//otherwise, the file was written successfully
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	closelog();
	close(file);
	return 0;

}
