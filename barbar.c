#include <X11/Xlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

volatile sig_atomic_t stop_flag = 0;
void handle_signal(int signal) 
{
        (void)signal; // suppress compiler warning
        stop_flag = 1;
}

int main(void)
{
        /* get X11 display */
        Display *display;
        if ((display = XOpenDisplay(NULL)) == NULL ) {
                fprintf(stderr, "Cannot open display!\n");
        }
        Window root = DefaultRootWindow(display);

        /* set up signal handlers for SIGINT (Ctrl+C) and SIGTERM (kill) */
        struct sigaction sa;
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        struct dirent *entry;
        DIR *dir = opendir(DIRP);
        if (!dir) {
                perror("opendir");
                exit(1);
        }

        char path[512];
        int nel = 0;
        int buffer = 10;

        int *fd = malloc(buffer * sizeof(int)); 

        /* open all fifos in DIRP */
        while ((entry = readdir(dir))) {
                snprintf(path, sizeof(path), "%s/%s", DIRP, entry->d_name);
                struct stat st;
                if (stat(path, &st) != 0) // cant get file stats
                        continue;
                if (!S_ISFIFO(st.st_mode)) // file not fifo
                        continue;

                int new_fd = open(path, O_RDONLY | O_NONBLOCK);
                if (new_fd == -1) {
                        perror("open fifo");
                        exit(1);
                }

                if (nel >= buffer) {
                        buffer *= 2;
                        int *tmp_fd = realloc(fd, buffer * sizeof(int));
                        if (!tmp_fd) {
                                perror("realloc");
                                exit(1);
                        }
                        fd = tmp_fd;
                }
                fd[nel++] = new_fd;
        }
        closedir(dir);

        if (nel == 0) {
                fprintf(stderr, "No fifos found in %s\n", DIRP);
                exit(1);
        }

        /* determine max file descriptor for select
         * NOT nel, just highest file descriptor number + 1 */
        int maxfd = 0;
        for (int i = 0; i < nel; ++i) {
                if (fd[i] > maxfd)
                        maxfd = fd[i];
        }
        ++maxfd; // for select

        /* final string */
        char *out_str = malloc((MAX_READ + 1) * sizeof(char));
        if (!out_str) {
                perror("malloc out_str");
                exit(1);
        }

        /* individual fifos */
        char **fifo_msgs = malloc(nel * sizeof(char*));
        if (!fifo_msgs) {
                perror("malloc fifo_msgs");
                exit(1);
        }
        for (int i = 0; i < nel; i++) {
                fifo_msgs[i] = calloc(MSG_SIZE, sizeof(char)); // initialized
                if (!fifo_msgs[i]) {
                        perror("calloc fifo_msgs[i]");
                        exit(1);
                }
        }

       while (!stop_flag) { 
                /* reset buffers */
                for (int i = 0; i < nel; ++i)
                        fifo_msgs[i][0] = '\0';

                /* reset RFRSH timer */
                time_t start = time(NULL);
                while(difftime(time(NULL), start) < RFRSH) {
                        fd_set readfds;
                        FD_ZERO(&readfds);
                        for (int i = 0; i < nel; ++i)
                                FD_SET(fd[i], &readfds);

                        /* wait at most 1s or half the refresh rate for data */
                        struct timeval timeout;
                        if (RFRSH > 1) {
                                timeout.tv_sec = 1;
                                timeout.tv_usec = 0;
                        } else {
                                timeout.tv_sec = 0;
                                timeout.tv_usec = RFRSH * (1000000 / 2);
                        }

                        int ret = select(maxfd, &readfds, NULL, NULL, &timeout);
                        if (ret == -1) {
                                perror("select");
                                break;
                        }
                        
                        if (ret == 0) // no data for any fd
                                continue;

                        for (int i = 0; i < nel; ++i) {
                                if (!FD_ISSET(fd[i], &readfds)) // no data for fd[i]
                                        continue;
                                if (fifo_msgs[i][0] != '\0') { // drain stale fd
                                        char tmp[MSG_SIZE];
                                        while (read(fd[i], tmp, sizeof(tmp) - 1) > 0)
                                                ;
                                        continue;
                                }

                                char buf[MSG_SIZE];
                                ssize_t bytes_read = read(fd[i], buf, sizeof(buf) - 1);
                                if (bytes_read <= 0) {
                                        perror("read fifo");
                                        continue;
                                }

                                /* format buf: remove \n add \0 */
                                buf[bytes_read] = '\0';
                                int j = 0;
                                for (int k = 0; k < bytes_read; ++k) {
                                        if (buf[k] != '\n')
                                                buf[j++] = buf[k];
                                }
                                buf[j] = '\0';

                                /* save in msgs array */
                                strncpy(fifo_msgs[i], buf, MSG_SIZE - 1);
                                fifo_msgs[i][MSG_SIZE-1] = '\0';

                                /* drain leftovers */
                                while (read(fd[i], buf, sizeof(buf) - 1) > 0)
                                        ;
                        }
                }

                /* concatenate array into final string */
                out_str[0] = '\0'; 
                int cur_len = 0;
                for (int i = 0; i < nel; ++i) {
                        if (fifo_msgs[i][0] == '\0')
                                continue;

                        int sep_len = strlen(SEP);
                        int msg_len;
                        if (i == 0) // no separators before first element
                                msg_len = strlen(fifo_msgs[i]);
                        else
                                msg_len = strlen(fifo_msgs[i]) + sep_len;
                        if (cur_len + msg_len >= MAX_READ) {
                                fprintf(stderr, "Final string too long\n");
                                fprintf(stderr, "Breaking early...\n");
                                break;
                        }

                        if (i == 0)
                                strncat(out_str, fifo_msgs[i], MAX_READ - cur_len);
                        else {
                                strncat(out_str, SEP, MAX_READ - cur_len + sep_len);
                                strncat(out_str, fifo_msgs[i], MAX_READ - cur_len);
                        }

                        cur_len += msg_len;
                }

                /* update bar with finished string */
                XStoreName(display, root, out_str);
                XSync(display, 0);
        }

        /* clean up on signal exit */
        free(out_str);
        for (int i = 0; i < nel; i++) {
                free(fifo_msgs[i]);
                close(fd[i]);
        }
        free(fifo_msgs);
        free(fd);
        XCloseDisplay(display);
        return 0;
}
