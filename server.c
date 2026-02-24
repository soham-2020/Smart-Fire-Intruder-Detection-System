#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <signal.h>

#define PORT         8080
#define BUFFER_SIZE  65536
#define LOG_FILE     "/home/vikas_vashistha/security_daemon/events.log"
#define IMAGE_DIR    "/home/vikas_vashistha/security_daemon/alerts"

volatile int running = 1;

void handle_signal(int sig) { running = 0; }

void get_timestamp(char* buf, size_t len) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buf, len, "%Y%m%d_%H%M%S", t);
}

void log_event(const char* alert_type, const char* temp,
               const char* humidity, const char* distance,
               const char* filename) {
    FILE* f = fopen(LOG_FILE, "a");
    if (!f) return;
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(f, "[%s] ALERT: %s | Temp: %sC | Humidity: %s%% | Distance: %scm | Image: %s\n",
            timestamp, alert_type, temp, humidity, distance, filename);
    fclose(f);
    printf("[%s] %s ALERT!\n", timestamp, alert_type);
    printf("  Temp: %sC  Humidity: %s%%  Distance: %scm\n", temp, humidity, distance);
    printf("  Image saved: %s\n\n", filename);
}

void parse_header(const char* request, const char* header,
                  char* value, size_t len) {
    char search[64];
    snprintf(search, sizeof(search), "%s: ", header);
    const char* pos = strstr(request, search);
    if (pos) {
        pos += strlen(search);
        const char* end = strstr(pos, "\r\n");
        if (end) {
            size_t copy_len = end - pos;
            if (copy_len >= len) copy_len = len - 1;
            strncpy(value, pos, copy_len);
            value[copy_len] = '\0';
            return;
        }
    }
    strncpy(value, "unknown", len);
}

void save_image(const char* body, int body_len,
                const char* alert_type, char* saved_path) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    snprintf(saved_path, 256, "%s/%s_%s.jpg",
             IMAGE_DIR, alert_type, timestamp);
    FILE* f = fopen(saved_path, "wb");
    if (!f) {
        printf("ERROR: Cannot save image to %s\n", saved_path);
        return;
    }
    fwrite(body, 1, body_len, f);
    fclose(f);
}

void handle_request(int client_fd) {
    char* request = malloc(BUFFER_SIZE);
    if (!request) return;

    int total = 0, n;

    // Read until we have full headers
    while ((n = recv(client_fd, request + total,
                     BUFFER_SIZE - total - 1, 0)) > 0) {
        total += n;
        request[total] = '\0';
        if (strstr(request, "\r\n\r\n")) break;
    }

    // Find body start
    char* body_start = strstr(request, "\r\n\r\n");
    if (!body_start) { free(request); close(client_fd); return; }
    body_start += 4;

    // Get content length from headers
    int content_length = 0;
    char* cl = strstr(request, "Content-Length:");
    if (!cl) cl = strstr(request, "content-length:");
    if (cl) sscanf(cl + 15, "%d", &content_length);

    // How much body already received
    int header_len   = body_start - request;
    int body_already = total - header_len;

    // Allocate image buffer
    char* image_buf = malloc(content_length + 1);
    if (!image_buf) { free(request); close(client_fd); return; }

    // Copy what we already have
    memcpy(image_buf, body_start, body_already);
    int image_total = body_already;

    // Read remaining body
    while (image_total < content_length) {
        n = recv(client_fd, image_buf + image_total,
                 content_length - image_total, 0);
        if (n <= 0) break;
        image_total += n;
    }

    // Parse headers
    char alert_type[32], temp[16], humidity[16], distance[16];
    parse_header(request, "Alert-Type",  alert_type, sizeof(alert_type));
    parse_header(request, "Temperature", temp,       sizeof(temp));
    parse_header(request, "Humidity",    humidity,   sizeof(humidity));
    parse_header(request, "Distance",    distance,   sizeof(distance));

    // Save image and log
    char saved_path[256];
    save_image(image_buf, image_total, alert_type, saved_path);
    log_event(alert_type, temp, humidity, distance, saved_path);

    // Send HTTP 200
    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    send(client_fd, response, strlen(response), 0);

    free(image_buf);
    free(request);
    close(client_fd);
}

int main() {
    mkdir(IMAGE_DIR, 0755);
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    openlog("security_daemon", LOG_PID, LOG_DAEMON);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed"); return 1;
    }

    listen(server_fd, 10);
    printf("Security Daemon running on port %d\n", PORT);
    printf("Saving images to: %s\n", IMAGE_DIR);
    printf("Logging events to: %s\n\n", LOG_FILE);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr*)&client_addr,
                               &client_len);
        if (client_fd < 0) continue;
        handle_request(client_fd);
    }

    close(server_fd);
    closelog();
    return 0;
}
