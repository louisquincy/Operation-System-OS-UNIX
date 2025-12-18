#include <iostream>
#include <vector>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <cerrno>

volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int r) {
    (void)r; 
    wasSigHup = 1;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;
    std::cout << "Send SIGHUP to test signal handling: kill -HUP " << getpid() << std::endl;

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigHupHandler;
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    int client_fd = -1; 

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        if (client_fd != -1) {
            FD_SET(client_fd, &readfds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        if (wasSigHup) {
            std::cout << "\n[SIGNAL] SIGHUP received (before pselect)!\n";
            wasSigHup = 0;
        }
        
        int ready = pselect(max_fd + 1, &readfds, NULL, NULL, NULL, &origMask);

        if (ready == -1) {
            if (errno == EINTR) {
                if (wasSigHup) {
                     std::cout << "\n[SIGNAL] SIGHUP received (interrupted pselect)!\n";
                     wasSigHup = 0;
                }
            } else {
                perror("pselect");
                break;
            }
            continue; 
        }

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, NULL, NULL);
            if (new_socket >= 0) {
                if (client_fd == -1) {
                    client_fd = new_socket;
                    std::cout << "[NET] New connection accepted. Socket: " << client_fd << "\n";
                    
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);
                } else {
                    std::cout << "[NET] Connection rejected (busy).\n";
                    close(new_socket);
                }
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &readfds)) {
            char buffer[1024];
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

            if (bytes_read > 0) {
                std::cout << "[DATA] Received " << bytes_read << " bytes.\n";
            } else if (bytes_read == 0) {
                std::cout << "[NET] Client disconnected.\n";
                close(client_fd);
                client_fd = -1;
            } else {
                perror("read");
                close(client_fd);
                client_fd = -1;
            }
        }
    }

    close(server_fd);
    return 0;
}
