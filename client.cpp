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
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{

    char *PORT = argv[2];
    // declare all the variable and the buffet that is needed
    int sockfd, len, bytes_sent, rv, fd;  
    char buf[MAXDATASIZE], imgBuf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    char s[INET6_ADDRSTRLEN];
    char msg[1000];
    size_t slice_point, total = 0, msgBytes = 100;
    const size_t url_len = strlen(argv[1]);
    FILE *fptr, *fpti;

    // check if the input has the correct amount of parameter
    if (argc != 3 && argc != 4 && argc != 5) {
        fprintf(stderr,"Please enter the proper numbers of argument\n");
        exit(1);
    }

    // empty the memory in hints and pass the IPv4 or IPv6 host to hints and socktype
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // 
    string fullUrl = argv[1];
    slice_point = fullUrl.find("/");;
    string hostname = fullUrl.substr(0, slice_point);
    string directory = fullUrl.substr(slice_point);

    // get the address information for the host
    if ((rv = getaddrinfo(hostname.c_str(), PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // create the sock for connection
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        // close the sockfd that is bad
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    // check if there is connection to the server
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    // save the address information for the server?
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    // clear the message buffer to hold HTTP request
    bzero(msg, 1000);

    // check if there is command lin options, -t or -h, and generate the HTTP request accordingly
    if (argc == 3)
    {
        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }else if (argv[3][1] == 'h')
    {
        sprintf(msg, "HEAD %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }else if (argv[3][1] == 't')
    {
        if(argc != 5){
            fprintf(stderr,"The numbers of arguments is not match\n");
            exit(1);
        }
        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str(), argv[4]);
    }
    // print the HTTP request
    printf("\n%s", msg);
    // sent HTTP request to the server
    bytes_sent = send(sockfd, msg, strlen(msg), 0);

    if (argc == 3 || argv[3][1] == 't') {
        size_t numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
        if ( numbytes == -1 ) perror("recv");
        string holder = buf;
        size_t headerFound = holder.find("HTTP/1.1");
        // try to split the header information from the all the data send by server
        string header;
        string content;
        size_t contentLength;
        if(headerFound == 0){ // header will appear in the beginning of the message
            size_t endFound = holder.find("\r\n\r\n");
            header = holder.substr(0, endFound+4);
            content = holder.substr(endFound+4);
            printf("%s", header.c_str());
            string check = header.substr(0, 12);
            if(check != "HTTP/1.1 200"){
                exit(1);
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
        string path = directory.substr(1, found);
        string fileName = directory.substr(1);
        // generate the command line to create folders and file
        char pathFileGen[100];
        bzero(pathFileGen, 100);
        if(path.size() == 0){
            sprintf(pathFileGen, "touch %s", fileName.c_str());
        }else{
            sprintf(pathFileGen, "mkdir -p %s && touch %s", path.c_str(), fileName.c_str());
        }
        
        // use the command line to make system call to create folders and file
        const int dir_err = system(pathFileGen);
        // Open the file allowing to read and write, create file if it doesn't exist
        fptr = fopen( fileName.c_str(), "wb+");
        // check if the command line to create folder and file was excute correctly
        if ( dir_err == -1 )
        {
            printf("Error");
            exit(1);
        }else
        {   
            // check if the the file were opened
            if (fptr == NULL)
            {
                printf("\n open() failed with error [%s}\n", strerror(errno));
            }else
            {   
                if ( fwrite(content.c_str(), 1, strlen(content.c_str()), fptr) == -1 ) perror("file write failed");
                total += strlen(content.c_str());
                // continue to fetch the data from server until everything is download
                while (1)
                {
                    numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
                    if ( numbytes == -1 ) perror("recv");
                    if ( numbytes == 0 ) break; // got to the end of the stream
                    if ( fwrite(buf, sizeof(char), numbytes, fptr) == -1 ) perror("file write failed");
                    total += numbytes;
                    // Compare with Content-Length header
                    if (total >= contentLength)
                        break;
                }
                // connection is lost therefore reconnect
                for(p = servinfo; p != NULL; p = p->ai_next) {
                    // create the sock for connection
                    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                        perror("client: socket");
                        continue;
                    }
                    // close the sockfd that is bad
                    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                        close(sockfd);
                        perror("client: connect");
                        continue;
                    }
                    break;
                }
                rewind(fptr);
                fseek(fptr, 0, SEEK_END);
                size_t size = ftell(fptr);
                char* temp = (char*) malloc(sizeof(char) * size);
                rewind(fptr);
                fread(temp, sizeof(char), size, fptr);
                string search = temp;
                size_t srcFound = search.find("src=");
                size_t preFound = srcFound;
                // check if any other object is found
                while(1){
                    preFound = srcFound;
                    // find the file directory and truncate it
                    string srcLoc = search.substr(srcFound);
                    size_t firstQuote = srcLoc.find('\"');
                    srcLoc = srcLoc.substr(firstQuote + 1);
                    size_t secondQuote = srcLoc.find('\"');
                    srcLoc = srcLoc.substr(0, secondQuote);
                    string url = "/" + path + srcLoc;
                    // create request for image
                    bzero(msg, 1000);
                    if(argc == 5){
                        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", url.c_str(), hostname.c_str(), argv[4]);
                    }else{
                        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", url.c_str(), hostname.c_str());
                    }
                    bytes_sent = send(sockfd, msg, strlen(msg), 0);
                    printf("%s", msg);

                    size_t numbytes = recv(sockfd, imgBuf, MAXDATASIZE-1, 0);
                    if ( numbytes == -1 ) perror("recv");
                    string holder = imgBuf;
                    size_t headerFound = holder.find("HTTP/1.1");
                    // try to split the header information from the all the data send by server
                    char header[300];
                    size_t headerLength;
                    size_t contentLength;
                    size_t totalLength;
                    if(headerFound == 0){ // header will appear in the beginning of the message
                        size_t endFound = holder.find("\r\n\r\n");
                        headerLength = endFound+4;
                        contentLength = numbytes-headerLength;
                        strncpy( header, imgBuf, headerLength );
                        header[headerLength]='\0';
                        string headerCK = header;
                        printf("%s", header);
                        string check = headerCK.substr(0, 12);
                        if(check != "HTTP/1.1 200"){
                            exit(1);
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
                    string path = url.substr(1, found);
                    string fileName = url.substr(1);
                    // generate the command line to create folders and file
                    char pathFileGen[100];
                    bzero(pathFileGen, 100);
                    sprintf(pathFileGen, "mkdir -p %s && touch %s", path.c_str(), fileName.c_str());
                    // use the command line to make system call to create folders and file
                    const int dir_err = system(pathFileGen);
                    fpti = fopen( fileName.c_str(), "wb+");
                    if ( fwrite(imgBuf+headerLength, 1, contentLength, fpti) == -1 ) perror("file write failed");
                    total = numbytes;
                    while (1)
                    {   
                        size_t numbytes = recv(sockfd, imgBuf, MAXDATASIZE-1, 0);
                        if ( numbytes == -1 ) perror("recv");
                        if ( numbytes == 0 ) break; // got to the end of the stream
                        if ( fwrite(imgBuf, sizeof(char), numbytes, fpti) == -1 ) perror("file write failed");
                        total += numbytes;
                        if (total >= totalLength)
                            break;
                        
                    }
                    fclose(fpti);
                    srcFound = search.find("src=");
                    if(srcFound == preFound){
                        break;
                    }
                }
                delete[] temp;
                fclose(fptr);
            }
        }
    }else if(argv[3][1] == 'h')
    {
        // get the head infromation from server and print it
        size_t numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
        buf[numbytes] = '\0';
        printf("%s", buf);
    }
    freeaddrinfo(servinfo); // all done with this structure
    // close the socket
    close(sockfd);

    return 0;
}