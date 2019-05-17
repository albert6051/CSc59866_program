HTTP Client Software
====================

CSC 59866: Group 1 & 6

This program was written for CSC 59866 class by group 1 & 6.


## How to compile

Make sure the machine has gcc and g++ installed.  Then run:

```
g++ client.cpp -o client
```


## Usage

Usage: `./client [url] [port] [command] [date]`


Make sure you have permission to execute the program.

To run this for regular GET, you must define the parameters url and port, similar to following.

```
sudo ./client hudson.ccny.cuny.edu/CSc59866/index.htm 80
```

To run this for HEAD, you must define the parameters url, port and command (-h), similar to following.

```
sudo ./client hudson.ccny.cuny.edu/CSc59866/index.htm 80 -h
```

To run this for conditional GET, you must define the parameters url, port, command (-t) and date, similar to following.

```
sudo ./client hudson.ccny.cuny.edu/CSc59866/index.htm 80 -t "Fri, 17 May 2019 11:25:11 GMT"
```

For GET it will save the file to your current directory, and will teminate the process once the downloading is done.

Finally, to shut down the client, press `^C` (`[Ctrl] + [C]`).