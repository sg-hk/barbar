#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../util.h"

static const char *mname = "ip";

// Configuration for the IP fetching service
#define IP_SERVICE_HOST "api.ipify.org"
#define IP_SERVICE_PORT "80" // Must be a string for getaddrinfo
#define REQUEST_TIMEOUT_SEC 5 // Timeout for socket send/receive operations

// Buffer sizes
#define IP_BUFFER_SIZE 64       // Max IPv6 is 39 chars + "IP: " prefix + nulls.
#define RESPONSE_BUFFER_SIZE 512 // For the HTTP response (headers + small body)

// Forward declaration of the IP fetching function
static int fetch_public_ip(char *ip_buf, size_t ip_buf_len);

int
main(void)
{
    char current_ip_display[IP_BUFFER_SIZE + 4]; // To hold "IP: " + actual IP
    char fetched_ip[IP_BUFFER_SIZE];
    char last_displayed_ip[IP_BUFFER_SIZE + 4] = ""; // Store last displayed string to avoid redundant w2s calls

    // Initial fetch and display
    if (fetch_public_ip(fetched_ip, sizeof(fetched_ip)) == 0) {
        snprintf(current_ip_display, sizeof(current_ip_display), "IP: %s", fetched_ip);
    } else {
        // fetch_public_ip already put an error string like "N/A" or "Error" in fetched_ip
        snprintf(current_ip_display, sizeof(current_ip_display), "IP: %s", fetched_ip);
    }
    w2s(mname, "%s", current_ip_display);
    strncpy(last_displayed_ip, current_ip_display, sizeof(last_displayed_ip) -1);
    last_displayed_ip[sizeof(last_displayed_ip)-1] = '\0';


    for (;;) {
        sleep(10); // Wait for 10 seconds

        if (fetch_public_ip(fetched_ip, sizeof(fetched_ip)) == 0) {
            snprintf(current_ip_display, sizeof(current_ip_display), "IP: %s", fetched_ip);
        } else {
            snprintf(current_ip_display, sizeof(current_ip_display), "IP: %s", fetched_ip);
        }

        // Only call w2s if the displayed string has changed
        if (strcmp(last_displayed_ip, current_ip_display) != 0) {
            w2s(mname, "%s", current_ip_display);
            strncpy(last_displayed_ip, current_ip_display, sizeof(last_displayed_ip) -1);
            last_displayed_ip[sizeof(last_displayed_ip)-1] = '\0';
        }
    }

    return 0; // Should be unreachable in this continuous loop
}

/**
 * Fetches the public IP address.
 * On success, fills ip_buf with the null-terminated IP string and returns 0.
 * On failure, fills ip_buf with a null-terminated error string (e.g., "N/A") and returns -1.
 */
static int
fetch_public_ip(char *ip_buf, size_t ip_buf_len)
{
    if (ip_buf_len == 0) return -1; // Must have space for at least null terminator

    struct addrinfo hints, *servinfo, *p;
    int sockfd = -1;
    int rv;
    char request_str[256];
    char response_buffer[RESPONSE_BUFFER_SIZE];

    // Prepare the HTTP GET request
    snprintf(request_str, sizeof(request_str),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: barbar-ip-module/1.0\r\n"
             "Connection: close\r\n\r\n", IP_SERVICE_HOST);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the hostname
    if ((rv = getaddrinfo(IP_SERVICE_HOST, IP_SERVICE_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "ip_module: getaddrinfo: %s\n", gai_strerror(rv));
        strncpy(ip_buf, "DNS_Err", ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0';
        return -1;
    }

    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            // perror("ip_module: socket"); // Too noisy for repeated attempts
            continue;
        }

        // Set send and receive timeouts for the socket
        struct timeval timeout;
        timeout.tv_sec = REQUEST_TIMEOUT_SEC;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 ||
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            // perror("ip_module: setsockopt timeout failed"); // Can be noisy
            close(sockfd);
            sockfd = -1; // Mark as closed
            continue;
        }


        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            // perror("ip_module: connect"); // Too noisy
            close(sockfd);
            sockfd = -1; // Mark as closed
            continue;
        }
        break; // Successfully connected
    }

    freeaddrinfo(servinfo); // All done with this structure

    if (p == NULL || sockfd == -1) { // Could not connect to any address
        fprintf(stderr, "ip_module: Failed to connect to %s\n", IP_SERVICE_HOST);
        strncpy(ip_buf, "Connect_Err", ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0';
        return -1;
    }

    // Send the HTTP request
    ssize_t bytes_sent = send(sockfd, request_str, strlen(request_str), 0);
    if (bytes_sent == -1) {
        perror("ip_module: send");
        close(sockfd);
        strncpy(ip_buf, "Send_Err", ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0';
        return -1;
    }

    // Receive the HTTP response
    ssize_t bytes_received = recv(sockfd, response_buffer, sizeof(response_buffer) - 1, 0);
    close(sockfd); // Close socket after send/recv (Connection: close)

    if (bytes_received == -1) {
        perror("ip_module: recv");
        strncpy(ip_buf, "Recv_Err", ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0';
        return -1;
    }
    if (bytes_received == 0) { // Connection closed by peer prematurely
        fprintf(stderr, "ip_module: Connection closed by peer prematurely\n");
        strncpy(ip_buf, "Peer_Close", ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0';
        return -1;
    }

    response_buffer[bytes_received] = '\0'; // Null-terminate the received data

    // Parse the HTTP response: find the body after "\r\n\r\n"
    // Very basic parsing: assumes "200 OK" and plain text IP in body.
    char *body_start = strstr(response_buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4; // Skip the "\r\n\r\n"
        
        // Check if response starts with "HTTP/1.1 200 OK" for minimal validation
        if (strncmp(response_buffer, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK")) != 0) {
             fprintf(stderr, "ip_module: HTTP status not 200 OK. Got: %.*s\n", (int) (body_start - response_buffer), response_buffer);
             strncpy(ip_buf, "HTTP_Err", ip_buf_len -1);
             ip_buf[ip_buf_len-1] = '\0';
             return -1;
        }

        // Remove trailing newline characters from the IP address if any
        char *newline_char = strpbrk(body_start, "\r\n");
        if (newline_char) {
            *newline_char = '\0';
        }

        // Copy the IP address to the output buffer
        strncpy(ip_buf, body_start, ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0'; // Ensure null termination
        return 0; // Success
    } else {
        fprintf(stderr, "ip_module: Could not find body in HTTP response from %s\n", IP_SERVICE_HOST);
        strncpy(ip_buf, "Parse_Err", ip_buf_len -1);
        ip_buf[ip_buf_len-1] = '\0';
        return -1;
    }
}
