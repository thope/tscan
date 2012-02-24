#include <stdio.h>
#include <sys/types.h>      /* socket(), connect(), select() */
#include <sys/socket.h>     /* socket(), connect() */
#include <unistd.h>         /* fcntl(), select() */
#include <fcntl.h>          /* fcntl() */
#include <sys/time.h>       /* select() */
#include <errno.h>
#include <string.h>         /* memset() */
#include <arpa/inet.h>      /* inet_pton() */

#define MAXFDS 1024     /* todo: replace select() with poll/epoll */

int main (int argc, char *argv[])
{
    if (argc != 3) {
        fprintf (stderr, "Usage: %s <IP> <PORTSTART-PORTEND>\n", argv[0]);
        return (1);
    }
    
    char *start_port, *end_port;
    int st_p, end_p;
    int i, err, n, flags, sock, maxfd;
    socklen_t len;
    struct sockaddr_in addr;
    struct timeval tv;
    fd_set set, rset, wset;

    FD_ZERO(&set);
    FD_ZERO(&rset);
    FD_ZERO(&wset);
    
    end_port = strchr (argv[2], '-');
    if (end_port == NULL)
        return (1);
    *end_port++ = 0;
    start_port = argv[2];
    
    st_p = atoi(start_port);
    end_p = atoi(end_port);

    if (end_p - st_p + 1 > MAXFDS) {
        fprintf (stderr, "Only allowed to scan < %d ports at a time\n", MAXFDS);
        return (1);
    }
    
    fprintf (stdout, "Trying to connect to %s on ports %s to %s...\n", argv[1], start_port, end_port);
    
    for (maxfd = 0; st_p <= end_p; st_p++) {
        if ( (sock = socket (PF_INET, SOCK_STREAM, 0)) == -1 ) {
            perror ("socket");
            continue;
        }
        
        /* set non-blocking */
        flags = fcntl (sock, F_GETFL, 0);
        fcntl (sock, F_SETFL, flags | O_NONBLOCK);
        
        memset (&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons (st_p);
        inet_pton(AF_INET, argv[1], &addr.sin_addr);
            
        if ( (n = connect (sock, (struct sockaddr *)&addr, sizeof addr)) < 0 )
            if (errno != EINPROGRESS) {
                perror ("connect");
                close (sock);
                continue;
            }

        if (n == 0) { /* connected immediately */
            fprintf (stdout, "%d open\n", st_p);
            close (sock);
        } else {
            FD_SET(sock, &set);
            if (sock > maxfd)
                maxfd = sock;
            
            /* restore */
            fcntl (sock, F_SETFL, flags);
        }
    } /* for */
    
    while (1) {
        tv.tv_sec = 20;
        tv.tv_usec = 0;
        rset = wset = set;
        if (select (maxfd+1, &rset, &wset, NULL, &tv) <= 0)
            break;

        for (i = 0; i <= maxfd; i++) {
            err = 0;
            if (FD_ISSET (i, &rset) || FD_ISSET (i, &wset)) {
                len = sizeof (err);
                if (getsockopt (i, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
                    err = errno;

                if (err == 0) {
                    len = sizeof(addr);
                    if (getpeername (i, (struct sockaddr *)&addr, &len) < 0)
                        perror ("getpeername");
                    else
                        fprintf (stdout, "%d open\n", ntohs(addr.sin_port));
                }
                FD_CLR(i, &set);
                close(i);
            }
        } /* for */
    } /* while */

    return (0);
}
