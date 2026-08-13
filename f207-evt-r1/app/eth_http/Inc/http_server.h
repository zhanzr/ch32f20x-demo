#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

/* Start the HTTP server (port 80). Call after lwip_init() + Netif_Config(). */
void http_server_init(void);

#endif /* HTTP_SERVER_H */
