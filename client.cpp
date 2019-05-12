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

#define PORT "80" // the port client will be connecting to 

#define MAXDATASIZE 4096 // max number of bytes we can get at once 

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
    if (argc != 2 && argc != 3) {
        fprintf(stderr,"usage: client hostname\n");
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
    
    printf("%s, %s \n",hostname.c_str(), directory.c_str());

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
    if (argc == 2)
    {
        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }else if (argv[2][1] == 'h')
    {
        sprintf(msg, "HEAD %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }else if (argv[2][1] == 't')
    {
        sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: Wed, 10 Apr 2019 13:42:00 GMT\r\nIf-None-Match: \"122c8-58631a92bfc30\"\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", directory.c_str(), hostname.c_str());
    }
    // print the HTTP request
    printf("\n%s", msg);
    // sent HTTP request to the server
    bytes_sent = send(sockfd, msg, strlen(msg), 0);

    if (argc == 2 || argv[2][1] == 't') {
        // find the position split the folder path and file name
        size_t found = directory.rfind('/');
        // split the directory into the folder path and file name
        string path = directory.substr(1, found);
        string fileName = directory.substr(1);
        // generate the command line to create folders and file
        char pathFileGen[100];
        bzero(pathFileGen, 100);
        sprintf(pathFileGen, "mkdir -p %s && touch %s", path.c_str(), fileName.c_str());
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
                // continue to fetch the data from server until everything is download
                while (1)
                {
                    size_t numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
                    if ( numbytes == -1 ) perror("recv");
                    if ( numbytes == 0 ) break; // got to the end of the stream
                    string holder = buf;
                    size_t headerFound = holder.find("HTTP/1.1");
                    // try to split the header information from the all the data send by server
                    if(headerFound == 0){ // header will appear in the beginning of the message
                        size_t endFound = holder.find("\r\n\r\n");
                        string header = holder.substr(0, endFound+4);
                        string content = holder.substr(endFound+4);
                        printf("%s", header.c_str());
                        if ( fwrite(content.c_str(), 1, strlen(content.c_str()), fptr) == -1 ) perror("file write failed");
                        total += strlen(content.c_str());
                    }else{
                        if ( fwrite(buf, sizeof(char), numbytes, fptr) == -1 ) perror("file write failed");
                        total += numbytes;
                    }
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
                size_t imgFound = search.find("<img");
                // check if any image is found
                if(imgFound != string::npos){
                    printf("%d %d\n", imgFound, string::npos);
                    // find the file directory and truncate it
                    search = search.substr(imgFound);
                    size_t srcFound = search.find("src=");
                    string imgLoc = search.substr(srcFound);
                    size_t firstQuote = imgLoc.find('\"');
                    imgLoc = imgLoc.substr(firstQuote + 1);
                    size_t secondQuote = imgLoc.find('\"');
                    imgLoc = imgLoc.substr(0, secondQuote);
                    string url = "/" + path + imgLoc;
                    // create request for image
                    bzero(msg, 1000);
                    sprintf(msg, "GET %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: 115\r\nConnection: keep-alive\r\n\r\n\r\n", url.c_str(), hostname.c_str());
                    bytes_sent = send(sockfd, msg, strlen(msg), 0);
                    printf("%s", msg);
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
                    while (1)
                    {
                        size_t numbytes = recv(sockfd, imgBuf, MAXDATASIZE-1, 0);
                        if ( numbytes == -1 ) perror("recv");
                        printf("%d", numbytes);
                        if ( numbytes == 0 ) break; // got to the end of the stream
                        string holder = imgBuf;
                        size_t headerFound = holder.find("HTTP/1.1");
                        // try to split the header information from the all the data send by server
                        if(headerFound == 0){ // header will appear in the beginning of the message
                            size_t endFound = holder.find("\r\n\r\n");
                            string header = holder.substr(0, endFound+4);
                            string content = holder.substr(endFound+4);
                            printf("%s", header.c_str());
                            if ( fwrite(content.c_str(), 1, strlen(content.c_str()), fpti) == -1 ) perror("file write failed");
                            total += strlen(content.c_str());
                        }else{
                            if ( fwrite(imgBuf, sizeof(char), numbytes, fpti) == -1 ) perror("file write failed");
                            total += numbytes;
                        }
                    }
                    delete[] temp;
                    fclose(fpti);
                    imgFound = search.find("<img");
                }
                fclose(fptr);
            }
        }
    }else if(argv[2][1] == 'h')
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