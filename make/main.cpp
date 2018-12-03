#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
using namespace std;


int main(int argv, char **args)
{
    if (argv < 2)
    {
        cout << "Not params" << endl;
        return 0;
    }

    int f = open(args[1], O_RDONLY);
    if (f < 0)
    {
        cout << "open fail" << endl;
        return -1;
    }

    int w = open("./miku1.h", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (w < 0)
    {
        cout << "open w fail" << endl;
        return -1;
    }
    char buf[200];
    uint8_t hex;
    int ret;
    int count = 0;

    sprintf(buf, "const unsigned char PROGMEM miku[] = {");
    write(w, buf, strlen(buf));

    while (1)
    {
        ret = read(f, &hex, 1);
        if (ret <= 0)
            break;
        ++count;
        sprintf(buf, "0x%x,", hex);
        write(w, buf, strlen(buf));
    }
    sprintf(buf, "};\r\n");
    write(w, buf, strlen(buf));
    cout << "write count : " << count << endl;
    close(f);
    close(w);
    return 0;
}