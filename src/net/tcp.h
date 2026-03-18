#ifndef TCP_H
#define TCP_H

#include <stdint.h>

/* TCP Header */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  /* upper 4 bits = header len in 32-bit words */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

/* TCP Flags */
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10
#define TCP_URG  0x20

/* TCP Baglanti Durumu */
#define TCP_STATE_CLOSED      0
#define TCP_STATE_SYN_SENT    1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_WAIT    3
#define TCP_STATE_CLOSE_WAIT  4
#define TCP_STATE_TIME_WAIT   5

/* TCP Soket */
#define TCP_RX_BUF_SIZE 4096
#define TCP_MAX_SOCKETS 4

typedef struct {
    int      state;
    uint8_t  remote_ip[4];
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t our_seq;
    uint32_t our_ack;
    uint32_t their_seq;
    uint32_t their_ack;
    uint8_t  rx_buf[TCP_RX_BUF_SIZE];
    uint16_t rx_len;
    uint16_t rx_read_pos;
    int      active;
    int      data_ready;
    int      connected;
    int      error;
} tcp_socket_t;

/* Fonksiyonlar */
void tcp_init(void);
void tcp_handle_packet(const uint8_t* data, uint16_t len, const uint8_t src_ip[4]);
int  tcp_connect(const uint8_t dst_ip[4], uint16_t dst_port);
int  tcp_send(int sock, const void* data, uint16_t len);
int  tcp_recv(int sock, void* buffer, uint16_t max_len);
int  tcp_close(int sock);
int  tcp_is_connected(int sock);
int  tcp_has_data(int sock);
int  tcp_get_error(int sock);

#endif
