#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUF_SIZE 8192

void send_response(int client, const char *body) {
    char response[BUF_SIZE * 2];

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "\r\n"
        "%s",
        strlen(body), body);

    send(client, response, strlen(response), 0);
}

void url_decode(char *str) {
    char decoded[1024];
    int i = 0, j = 0;

    while (str[i] != '\0' && j < 1023) {
        if (str[i] == '%' && str[i + 1] && str[i + 2]) {
            char hex[3];

            hex[0] = str[i + 1];
            hex[1] = str[i + 2];
            hex[2] = '\0';

            decoded[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (str[i] == '+') {
            decoded[j++] = ' ';
            i++;
        } else {
            decoded[j++] = str[i++];
        }
    }

    decoded[j] = '\0';
    strcpy(str, decoded);
}

void parse_params(char *data, double *a, double *b, char *op) {
    char *token = strtok(data, "&");

    while (token != NULL) {
        if (strncmp(token, "a=", 2) == 0) {
            *a = atof(token + 2);
        } 
        else if (strncmp(token, "b=", 2) == 0) {
            *b = atof(token + 2);
        } 
        else if (strncmp(token, "op=", 3) == 0) {
            *op = token[3];
        }

        token = strtok(NULL, "&");
    }
}

void handle_calculator(int client, char *request) {
    double a = 0, b = 0, result = 0;
    char op = '+';
    char params[1024] = "";
    int has_params = 0;

    if (strncmp(request, "GET", 3) == 0) {
        char *start = strstr(request, "/?");

        if (start != NULL) {
            start += 2;

            char *end = strchr(start, ' ');
            if (end != NULL) {
                int len = end - start;

                if (len >= sizeof(params)) {
                    len = sizeof(params) - 1;
                }

                strncpy(params, start, len);
                params[len] = '\0';
                has_params = 1;
            }
        }
    } 
    else if (strncmp(request, "POST", 4) == 0) {
        char *body = strstr(request, "\r\n\r\n");

        if (body != NULL) {
            body += 4;

            strncpy(params, body, sizeof(params) - 1);
            params[sizeof(params) - 1] = '\0';

            has_params = 1;
        }
    }

    if (has_params) {
        url_decode(params);
        parse_params(params, &a, &b, &op);
    }

    int valid = 1;

    if (has_params) {
        switch (op) {
            case '+':
                result = a + b;
                break;

            case '-':
                result = a - b;
                break;

            case '*':
                result = a * b;
                break;

            case '/':
                if (b == 0) {
                    valid = 0;
                } else {
                    result = a / b;
                }
                break;

            default:
                valid = 0;
        }
    }

    char body[BUF_SIZE];

    snprintf(body, sizeof(body),
        "<html>"
        "<head>"
        "<meta charset='utf-8'>"
        "<title>HTTP Calculator</title>"
        "</head>"
        "<body>"

        "<h2>HTTP Calculator</h2>"

        "<h3>GET Method</h3>"
        "<form method='GET' action='/'>"
        "A: <input name='a' type='number' step='any' required><br><br>"
        "B: <input name='b' type='number' step='any' required><br><br>"
        "Operator: "
        "<select name='op'>"
        "<option value='+'>+</option>"
        "<option value='-'>-</option>"
        "<option value='*'>*</option>"
        "<option value='/'>/</option>"
        "</select><br><br>"
        "<button type='submit'>Calculate GET</button>"
        "</form>"

        "<hr>"

        "<h3>POST Method</h3>"
        "<form method='POST' action='/'>"
        "A: <input name='a' type='number' step='any' required><br><br>"
        "B: <input name='b' type='number' step='any' required><br><br>"
        "Operator: "
        "<select name='op'>"
        "<option value='+'>+</option>"
        "<option value='-'>-</option>"
        "<option value='*'>*</option>"
        "<option value='/'>/</option>"
        "</select><br><br>"
        "<button type='submit'>Calculate POST</button>"
        "</form>"
    );

    if (has_params) {
        char result_html[1024];

        if (valid) {
            snprintf(result_html, sizeof(result_html),
                "<hr>"
                "<h3>Result:</h3>"
                "<p>%.2lf %c %.2lf = %.2lf</p>",
                a, op, b, result);
        } else {
            snprintf(result_html, sizeof(result_html),
                "<hr>"
                "<h3>Result:</h3>"
                "<p style='color:red;'>Invalid operation</p>");
        }

        strncat(body, result_html, sizeof(body) - strlen(body) - 1);
    }

    strncat(body, "</body></html>", sizeof(body) - strlen(body) - 1);

    send_response(client, body);
}

int main() {
    int server_fd, client;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Calculator server running at http://localhost:%d\n", PORT);

    while (1) {
        client = accept(server_fd, NULL, NULL);

        if (client < 0) {
            perror("accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        recv(client, buffer, sizeof(buffer) - 1, 0);

        handle_calculator(client, buffer);

        close(client);
    }

    close(server_fd);

    return 0;
}