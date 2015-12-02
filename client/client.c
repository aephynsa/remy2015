#include "../protocol/utility.h"
#include "../protocol/udp_protocol.h"

#define BUFFER_LEN 512

/* int connect_udp
 *   const char* hostname - hostname to connect to
 *   const char* port_name - 
 *   struct addringo* serv_addr
 * Returns the pointer to a socket if successful
 * errors and dies otherwise
 */
int connect_udp(char const* hostname, char const* port_name, struct addrinfo* serv_addr) {
    int sock;
    struct addrinfo addr_criteria;

    // fill out addrinfo
    memset(&addr_criteria, 0, sizeof(addr_criteria));
    addr_criteria.ai_family = AF_UNSPEC;
    addr_criteria.ai_socktype = SOCK_DGRAM;
    addr_criteria.ai_protocol = IPPROTO_UDP; 

    // decipher hostname
    if (getaddrinfo(hostname, port_name, &addr_criteria, &serv_addr)) {
        error("getaddrinfo()");
    }
    
    // get socket
    if ((sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol)) < 0) {
        error("socket()");
    }

    return sock;
}

/* buffer* compile_file
 *   int sock - socket to communicate over
 *   struct addrinfo* serv_addr - server information
 * Returns a buffer with data collected from server
 */
buffer* compile_file(int sock, struct addrinfo* serv_addr) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    buffer* buf = create_buffer(BUFFER_LEN);
    buffer* full_payload = create_buffer(BUFFER_LEN);
    header h;
    ssize_t total_bytes_recv = 0;
    int chunks = 0;

    // begin
    fprintf(stdout, "waiting for data... ");
    while (total_bytes_recv == 0 || total_bytes_recv < full_payload->len) {
        // receive message
        if ((buf->len = recvfrom(sock, buf->data, buf->size, 0,
                (struct sockaddr*)&from_addr, &from_addrlen)) < 0) {
            error("recvfrom()");
        }

        // upkeep
        total_bytes_recv += buf->len;
        h = extract_header(buf);
        full_payload->len = h.data[UP_TOTAL_SIZE];

        // resize big buffer as needed to hold all incoming data
        if (full_payload->size < h.data[UP_TOTAL_SIZE]) {
            resize_buffer(full_payload, h.data[UP_TOTAL_SIZE]);
        }

        // build into one big buffer
        assemble_datagram(full_payload, buf);
        ++chunks;

        // confirmation of receipt
        fprintf(stderr, "%d byte datagram received\n", buf->len);
    }

    // confirmation of total transfer
    fprintf(stdout, "%d bytes of data received in %d chunks\n", ((int)(total_bytes_recv - (chunks * 28))), chunks);

    // clean up
    delete_buffer(buf);
    return full_payload;
}

/* void send_request
 *   struct addrinfo* servaddr
 *   uint32_t password - password the proxy uses to identify this client
 *   uint32_t request - command being sent (GPS, dGPS, LASERS, IMAGE, MOVE, TURN, STOP)
 *   uint32_t data - any data that request requires (MOVE/TURN rate)
 * errors and dies if message could not be sent
 */
void send_request(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t request, uint32_t data) {
    fprintf(stdout, "sending request [%d:%d]\n", request, data);

    // create and send message to proxy
    buffer* message = create_message(0, password, request, 0, 0, 0, 0);
    ssize_t bytes_sent = sendto(sock, message->data, message->len,
            0, serv_addr->ai_addr, serv_addr->ai_addrlen);

    // ensure everything worked
    if (bytes_sent != UP_HEADER_LEN) {
        error("sendto()");
    }
}

/* buffer* receive_request
 *   int sock - socket to communicate through
 *   struct addrinfo* servaddr
 * returns buffer with data from proxy on success
 * errors and dies otherwise
 */
buffer* receive_request(int sock, struct addrinfo* serv_addr) {
    struct sockaddr_storage from_addr;
    socklen_t from_addrlen = sizeof(from_addr);
    buffer* buf = create_buffer(BUFFER_LEN);

    if ((buf->len = recvfrom(sock, buf->data, buf->size, 0,
            (struct sockaddr*)&from_addr, &from_addrlen)) < 0) {
        error("recvfrom()");
    }

    return buf;
}

void get_thing(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command);

int main(int argc, char** argv) {
    int i, hflag, pflag, nflag, lflag; // variables for argument parsing
    int port,                          // port number to connect with proxy
        sock,                          // udp socket to proxy
        sides,                         // number of sides of first shape
        lengths;                       // length of sides of shapes
    char hostname[BUFFER_LEN];         // hostname of server
    struct addrinfo* serv_addr;
    uint32_t password;                 // password required provided by server
    char usage[BUFFER_LEN];            // how to use this program

    // setup basics
    hostname[0] = '\0';
    snprintf(usage, BUFFER_LEN, "usage: %s -h <udp_hostname> -p <udp_port> -n <number_of_sides> -l <length_of_sides>\n", argv[0]);

    // read in arguments
    if (argc != 9) {
        error(usage);
    }

    // read in the required arguments
    hflag = pflag = nflag = lflag = 0;
    for (i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-h") == 0) {
            strncpy(hostname, argv[i + 1], BUFFER_LEN - 1);
            hflag = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[i + 1]);
            pflag = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            sides = atoi(argv[i + 1]);
            nflag = 1;
        } else if (strcmp(argv[i], "-l") == 0) {
            lengths = atoi(argv[i + 1]);
            lflag = 1;
        } else {
            error(usage);
        }
    }
    if (!(hflag && pflag && nflag && lflag)) {
        error(usage);
    }

    // connect to the UDP server
    if (port == 0) {
        error("port number");
    }
    serv_addr = NULL;
    sock = connect_udp(hostname, argv[2], serv_addr);

    // request connection to server
    for (;;) {
        send_request(sock, serv_addr, 0, CONNECT, 0);   // send request
        buffer* buf = receive_request(sock, serv_addr); // receive ack + pass
        password = ((uint32_t*)(buf->data))[UP_IDENTIFIER];
        delete_buffer(buf);
        send_request(sock, serv_addr, password, CONNECT, 0); // send ack
        /* TODO: if fail somehow, call continue */
        break;
    }

    // procees to request everything (test purposes only)
    // this should be replaced with code to control robot to draw 2 shapes
    get_thing(sock, serv_addr, password, GPS);
    get_thing(sock, serv_addr, password, dGPS);
    get_thing(sock, serv_addr, password, LASERS);

    // close connection with server
    send_request(sock, serv_addr, password, QUIT, 0);

    // clean up
    close(sock);
    freeaddrinfo(serv_addr);
    return 0;
}

/* void get_thing
 *   int sock - socket to communicate over
 *   struct addrinfo* serv_addr
 *   uint32_t password - used by proxy to identify this client
 *   uint32_t command - request being made to the proxy
 */
void get_thing(int sock, struct addrinfo* serv_addr, uint32_t password, uint32_t command) {
    // send request
    send_request(sock, serv_addr, password, command, 0);

    // receive response
    buffer* buf = receive_request(sock, serv_addr);

    // make sure it was a valid request 
    header h = extract_header(buf);
    if (h.data[UP_CLIENT_REQUEST] != command) {
        error("invalid acknowledgement");
    }

    // get requested data
    buffer* full_payload = compile_file(sock, serv_addr);

    // just print to stdout for the time being
    fprintf(stdout, "===== BEGIN DATA =====\n");
    fwrite(full_payload->data, full_payload->len, 1, stdout);
    fprintf(stdout, "\n====== END DATA ======\n");

    // clean up
    delete_buffer(buf);
    delete_buffer(full_payload);
}
