#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstdlib>
#include <string>

using namespace std;

#define MAXDATASIZE 4097 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) { // check if address family is IPv4
        return &(((struct sockaddr_in*)sa)->sin_addr); // set socket address to IPv4
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr); // set socket address to IPv6
}

int main(int argc, char *argv[])
{

    char *PORT = argv[2]; // declare and assign port number given by client
    // declare all the variable and the buffet that is needed
    int sockfd, len, bytes_sent, rv, fd;  // declare integer variables
    char buf[MAXDATASIZE], imgBuf[MAXDATASIZE]; // declare buffer for receiving
    struct addrinfo hints, *servinfo, *p; // declare stuct addinfo to hold host and server address
    char s[INET6_ADDRSTRLEN]; // declare a string to hold address
    char msg[1000]; // declare a string to message to server
    size_t slice_point, total = 0, msgBytes = 100; // declare size_t veriables
    const size_t url_len = strlen(argv[1]); // declare and assign url lenght
    FILE *fptr, *fpti; // declare the file decription for file saving

    // check if the input has the correct amount of parameter
    if (argc != 3 && argc != 4 && argc != 5) {
        fprintf(stderr,"Please enter the proper numbers of argument\n"); // return error if amount of parameter is not correct
        exit(1); // exit if amount of parameter is not correct
    }

    // empty the memory in hints and pass the IPv4 or IPv6 host to hints and socktype
    memset(&hints, 0, sizeof hints); // empty the memory in hints
    hints.ai_family = AF_UNSPEC;    // assign unspecific address family
    hints.ai_socktype = SOCK_STREAM; // assign SOCK_STREAM to socktype for TCP

    string fullUrl = argv[1]; // assign url to a string
    slice_point = fullUrl.find("/"); // find the slice point between hostname and file directory
    string hostname = fullUrl.substr(0, slice_point); // get hostname
    string directory = fullUrl.substr(slice_point); // get file directory

    // use getaddrinfo to get the address information for the host
    if ((rv = getaddrinfo(hostname.c_str(), PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); // return error if can't obtain address information
        exit(1); // exit as can't get host address information
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // create the sock for connection
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { // check if the socket is created successfully
            perror("client: socket"); // return an error if can't create a socket
            continue;   // continue to try to find a working socket, skip connection
        }
        // close the sockfd that is bad
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { // check if the socket can be connected
            close(sockfd); // close the socket if it can't be connected
            perror("client: connect"); // return an error if socket can't be connected
            continue; // continue the loop for looking for socket
        }

        break; // exit if the socket is found and connected
    }

    // check if there is connection to the server
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n"); // return error if there is no connection to the server
        exit(1); // exit if the connection is not corret
    }

    // save the address information for the server?
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    // print we are connect to a server with its IP address
    printf("client: connecting to %s\n", s);

    // clear the message buffer to hold HTTP request
    bzero(msg, 1000);

    // check if there is command lin options, -t or -h, and generate the HTTP request accordingly
    if (argc == 3) // regular request has 3 argument
    {
        // regular request
        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }else if (argv[3][1] == 'h') // header request has h in the fourth argument
    {
        // header request
        sprintf(msg, "HEAD %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }else if (argv[3][1] == 't') // conditional request has t in the fourth argument
    {
        if(argc != 5){ // the conditional request will have fifth argument for the date
            fprintf(stderr,"The numbers of arguments is not match\n"); // return error message for incorrect amount of argument
            exit(1); // exit for incorrect amount of argument
        }
        // conditional request
        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: %s\r\nIf-None-Match: \"122c8-58631a92bfc30\"\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str(), argv[4]);
    }
    // print the HTTP request
    printf("\n%s", msg);
    // sent HTTP request to the server
    bytes_sent = send(sockfd, msg, strlen(msg), 0);

    if (argc == 3 || argv[3][1] == 't') { // check if it regular request and conditional request
        size_t numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0); // obatin the first block of the file
        if ( numbytes == -1 ) perror("recv"); // check if the file is receive successfully
        string holder = buf; // create a string with the buf content
        size_t headerFound = holder.find("HTTP/1.1"); // check if their is a header
        // try to split the header information from the all the data send by server
        string header; // declare a string to store the header
        string content; //  declare a string to store the content
        size_t contentLength; // declare a size_t to store content length
        if(headerFound == 0){ // header will appear in the beginning of the message
            size_t endFound = holder.find("\r\n\r\n"); // find the end of the header
            header = holder.substr(0, endFound+4); // slice out the header
            content = holder.substr(endFound+4); // slice out the content
            printf("%s", header.c_str()); // print the header
            string check = header.substr(0, 12); // obtain the state of request
            if(check != "HTTP/1.1 200"){ // check if the resquest is success
                exit(1); // exit if the request is bad
            }
            size_t clenptr = header.find("Content-Length"); // Get Content-Length
            clenptr = header.find(": ", clenptr) + 2;       // Jump to colon pos + 2
            string clenstr = header.substr(                 // Substring of...
                    clenptr,                                // the number position...
                    header.find("\r\n", clenptr) - clenptr);// ... and length of number before /r/n
            contentLength = atoi(clenstr.c_str());          // Convert to number
        }
        // find the position split the folder path and file name
        size_t found = directory.rfind('/');
        // split the directory into the folder path and file name
        string path = directory.substr(1, found); // path name
        string fileName = directory.substr(1); // file name
        // generate the command line to create folders and file
        char pathFileGen[100]; // buffer to store the command line for directory creation
        bzero(pathFileGen, 100); // clean the buffer
        if(path.size() == 0){ // check if there is folder in directory
            sprintf(pathFileGen, "touch %s", fileName.c_str()); // no folder, create file directly
        }else{
            sprintf(pathFileGen, "mkdir -p %s && touch %s", path.c_str(), fileName.c_str()); // have folder, create folder then create file
        }
        
        // use the command line to make system call to create folders and file
        const int dir_err = system(pathFileGen);
        // Open the file allowing to read and write, create file if it doesn't exist
        fptr = fopen( fileName.c_str(), "wb+");
        // check if the command line to create folder and file was excute correctly
        if ( dir_err == -1 ) // check it the directory and file are created
        {
            printf("Error"); // return error and exit if fail to create directory and file are created
            exit(1);
        }else
        {   
            // check if the the file were opened
            if (fptr == NULL)
            {
                printf("\n open() failed with error [%s}\n", strerror(errno)); // return error message for failing to open file
            }else
            {   
                if ( fwrite(content.c_str(), 1, strlen(content.c_str()), fptr) == -1 ) perror("file write failed"); // store the content to the file
                total += strlen(content.c_str()); // record how many byte of content are download
                // continue to fetch the data from server until everything is download
                while (1)
                {
                    numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0); // fetch the data again
                    if ( numbytes == -1 ) perror("recv"); // return error for fail to fetch data
                    if ( numbytes == 0 ) break; // got to the end of the stream
                    if ( fwrite(buf, sizeof(char), numbytes, fptr) == -1 ) perror("file write failed"); // store the content to the file
                    total += numbytes; // record how many byte of content are download
                    // Compare with Content-Length header
                    if (total >= contentLength) // check if the full file is download
                        break; // stop downloading if everything is download
                }
                // rewind(fptr); // go to start of the download file
                fseek(fptr, 0, SEEK_END); // Sets the position indicator to the start of the file
                size_t size = ftell(fptr); // check the size of the file
                char* temp = (char*) malloc(sizeof(char) * size); // create a string to store the file
                rewind(fptr);
                fread(temp, sizeof(char), size, fptr); // read the content of the file to temp
                string search = temp; // convert temp to a string type from c string
                size_t srcFound = search.find("src="); // search for additional object that need to download 
                // check if any other object is found
                while(srcFound != string::npos){
                    // find the file directory and truncate it
                    string srcLoc = search.substr(srcFound); // cut the file to where src= is located
                    size_t firstQuote = srcLoc.find('\"'); // find the front quote for the url for the addtional object
                    srcLoc = srcLoc.substr(firstQuote + 1); // cut the file to where the front quote located
                    size_t secondQuote = srcLoc.find('\"'); // find the back quote for the url for the addtional object
                    srcLoc = srcLoc.substr(0, secondQuote); // cut the file to where the back quote located
                    string url = "/" + path + srcLoc; // create the url to request for addtional object
                    // create request for image
                    bzero(msg, 1000); // clear the message buffer
                    if(argc == 5){ // check if it is condtional request
                        // conditional request
                        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", url.c_str(), hostname.c_str(), argv[4]);
                    }else{
                        // regular request
                        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", url.c_str(), hostname.c_str());
                    }
                    bytes_sent = send(sockfd, msg, strlen(msg), 0); // send the request to server
                    printf("%s", msg); // print the request

                    size_t numbytes = recv(sockfd, imgBuf, MAXDATASIZE-1, 0); // receive the data block from server
                    if ( numbytes == -1 ) perror("recv"); // print error if receive file is failed
                    string holder = imgBuf; // assign the date in the buffer to a string
                    size_t headerFound = holder.find("HTTP/1.1"); // find the header for the response
                    // try to split the header information from the all the data send by server
                    char header[300]; // declare a buffer to store header
                    size_t headerLength; // delcare size_t to store the lenght of header
                    size_t contentLength; // delcare size_t to store the lenght of content of first block
                    size_t totalLength; // delcare size_t to store the total lenght of content
                    if(headerFound == 0){ // header will appear in the beginning of the message
                        size_t endFound = holder.find("\r\n\r\n"); // locate where the header end
                        headerLength = endFound+4; // assign the header lenght
                        contentLength = numbytes-headerLength; // assign the content lenght
                        strncpy( header, imgBuf, headerLength ); // cut the header out
                        header[headerLength]='\0'; // make the header buffer to string
                        string headerCK = header; // make the c string header to string type
                        printf("%s", header); // print header of the response
                        string check = headerCK.substr(0, 12); // get the first 12 characters of the header
                        if(check != "HTTP/1.1 200"){ // check if the request is success
                            exit(1);   // exit if the request has failed
                        }
                        size_t clenptr = headerCK.find("Content-Length"); // Get Content-Length
                        clenptr = headerCK.find(": ", clenptr) + 2;       // Jump to colon pos + 2
                        string clenstr = headerCK.substr(                 // Substring of...
                                            clenptr,                                // the number position...
                                            headerCK.find("\r\n", clenptr) - clenptr);// ... and length of number before /r/n
                        totalLength = atoi(clenstr.c_str());          // Convert to number
                    }
                    // find the position split the folder path and file name
                    size_t found = url.rfind('/');
                    // split the directory into the folder path and file name
                    string path = url.substr(1, found); // path name
                    string fileName = url.substr(1); // file name
                    // generate the command line to create folders and file
                    char pathFileGen[100]; // buffer to store the command line for directory creation
                    bzero(pathFileGen, 100); // clean the buffer
                    if(path.size() == 0){ // check if there is folder in directory
                        sprintf(pathFileGen, "touch %s", fileName.c_str()); // no folder, create file directly
                    }else{
                        sprintf(pathFileGen, "mkdir -p %s && touch %s", path.c_str(), fileName.c_str()); // have folder, create folder then create file
                    }
                    // use the command line to make system call to create folders and file
                    const int dir_err = system(pathFileGen); // use system call to create directory and file
                    fpti = fopen( fileName.c_str(), "wb+"); // open file for storing data
                    if ( fwrite(imgBuf+headerLength, 1, contentLength, fpti) == -1 ) perror("file write failed"); // write the data to file
                    total = numbytes; // store the total byte has been download
                    while (1)
                    {   
                        size_t numbytes = recv(sockfd, imgBuf, MAXDATASIZE-1, 0); // fetch the data again
                        if ( numbytes == -1 ) perror("recv"); // return error for fail to fetch data
                        if ( numbytes == 0 ) break; // got to the end of the stream
                        if ( fwrite(imgBuf, sizeof(char), numbytes, fpti) == -1 ) perror("file write failed"); // store the content to the file
                        total += numbytes; // store the total byte has been download
                        if (total >= totalLength) // check if all the whole file is download
                            break; // stop download
                        
                    }
                    fclose(fpti); // close the file for addtional object
                    search = search.substr(srcFound+4); // cut out the part that already search
                    srcFound = search.find("src="); // search of next addtional object
                }
                delete[] temp; // release the heap memory
                fclose(fptr); // close the file for main file htm or html
            }
        }
    }else if(argv[3][1] == 'h') // check if the request is a header request
    {
        // get the header infromation from server and print it
        size_t numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
        buf[numbytes] = '\0'; // make buf to a string
        printf("%s", buf); // prinr header
    }else{
        perror("unknow request");
    }
    freeaddrinfo(servinfo); // all done with this structure
    // close the socket
    close(sockfd);

    return 0;
}