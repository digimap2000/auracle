#include "bridge_server.h"

#include <errno.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "uart_bridge.h"

static const char *TAG = "bridge_srv";

#define LISTEN_PORT  CONFIG_AURACLE_BRIDGE_TCP_PORT

static TaskHandle_t s_bridge_task;

static int create_listen_socket(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket failed: errno=%d", errno);
        return -1;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(LISTEN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "bind failed: errno=%d", errno);
        close(fd);
        return -1;
    }

    if (listen(fd, 1) != 0) {
        ESP_LOGE(TAG, "listen failed: errno=%d", errno);
        close(fd);
        return -1;
    }

    return fd;
}

static void configure_client_socket(int fd)
{
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 200000,
    };
    int yes = 1;

    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static int send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        int n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static void drain_uart_discard(void)
{
    uint8_t buf[UART_BRIDGE_RX_CHUNK_SIZE];

    while (uart_bridge_read(buf, sizeof(buf), 0) > 0) {
    }
}

static int accept_client(int listen_fd)
{
    fd_set rfds;
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 200000,
    };

    FD_ZERO(&rfds);
    FD_SET(listen_fd, &rfds);

    int ret = select(listen_fd + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0 || !FD_ISSET(listen_fd, &rfds)) {
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        ESP_LOGW(TAG, "accept failed: errno=%d", errno);
        return -1;
    }

    char addr_str[16];
    inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
    ESP_LOGI(TAG, "Client connected: %s:%d", addr_str, ntohs(client_addr.sin_port));

    configure_client_socket(client_fd);
    uart_bridge_reset_rx();
    return client_fd;
}

static void close_client(int *client_fd)
{
    if (*client_fd >= 0) {
        ESP_LOGI(TAG, "Client disconnected");
        close(*client_fd);
        *client_fd = -1;
    }
    uart_bridge_reset_rx();
}

static void bridge_task(void *arg)
{
    (void)arg;

    while (1) {
        int listen_fd = create_listen_socket();
        if (listen_fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Listening on TCP port %d", LISTEN_PORT);
        int client_fd = -1;

        while (1) {
            if (client_fd < 0) {
                drain_uart_discard();
                client_fd = accept_client(listen_fd);
                continue;
            }

            fd_set rfds;
            struct timeval tv = {
                .tv_sec = 0,
                .tv_usec = 20000,
            };
            FD_ZERO(&rfds);
            FD_SET(client_fd, &rfds);

            int ret = select(client_fd + 1, &rfds, NULL, NULL, &tv);
            if (ret < 0) {
                ESP_LOGW(TAG, "select failed: errno=%d", errno);
                close_client(&client_fd);
                continue;
            }

            if (ret > 0 && FD_ISSET(client_fd, &rfds)) {
                uint8_t net_buf[256];
                int n = recv(client_fd, net_buf, sizeof(net_buf), 0);
                if (n <= 0) {
                    close_client(&client_fd);
                    continue;
                }

                if (uart_bridge_write(net_buf, (size_t)n) != ESP_OK) {
                    ESP_LOGW(TAG, "UART write failed, dropping client");
                    close_client(&client_fd);
                    continue;
                }
            }

            while (1) {
                uint8_t uart_buf[UART_BRIDGE_RX_CHUNK_SIZE];
                int n = uart_bridge_read(uart_buf, sizeof(uart_buf), 0);
                if (n <= 0) {
                    break;
                }
                if (send_all(client_fd, uart_buf, (size_t)n) != 0) {
                    ESP_LOGW(TAG, "Socket send failed, dropping client");
                    close_client(&client_fd);
                    break;
                }
            }
        }
    }
}

esp_err_t bridge_server_start(void)
{
    if (s_bridge_task) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xTaskCreate(bridge_task, "bridge_server",
                    CONFIG_AURACLE_BRIDGE_TASK_STACK_SIZE,
                    NULL,
                    CONFIG_AURACLE_BRIDGE_TASK_PRIORITY,
                    &s_bridge_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bridge task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

