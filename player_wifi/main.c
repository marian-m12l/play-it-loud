#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"
#include "lwip/init.h"
#include "lwip/apps/httpd.h"

#include "gb_serial.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));


#define POLL_RATE_MS 5

// TODO Connect to Wi-Fi
// TODO Auth with spotify and add Connect endpoint
// TODO Stream audio

// TODO cf. pico-examples httpd with mdns !


// TODO Move to dedicated file wifi.c ?? httpd.c ? mdns.c ?
// mDNS FIXME spotify zeroconf
// spotify connect : sends PTR query for: _spotify-connect._tcp.local: type PTR, class IN, "QM" question
// should respond with: 
//      - PTR _spotify-connect._tcp.local --> <hostname>._spotify-connect._tcp.local
//      - SRV <hostname>._spotify-connect._tcp.local --> port <random-port-of-http-server> + target <hostname>
//      - TXT <hostname>._spotify-connect._tcp.local --> VERSION=1.0 + CPath=/
//      - A <hostname>: addr <ip>

static void srv_txt(struct mdns_service *service, void *txt_userdata) {
  err_t res;
  LWIP_UNUSED_ARG(txt_userdata);

  res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
}

// TODO HTTP endpoints
static const char *cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    // TODO Support actions : "addUser" (gives auth key) and "getInfo" (asks for player name and capabilities)
    // TODO Once user is added, use tcp client to authenticate with spotify api, then search tracks and ask for playback
    // TODO Playback will require AES/CTR decyption (per chunk?)
    // TODO Playback will require OGG/Vorbis decoding: https://github.com/nothings/stb/blob/master/stb_vorbis.c

/* getInfo:
REQUEST:
    GET /?action=getInfo HTTP/1.1\r\n
    Accept-Encoding: gzip\r\n
    Connection: keep-alive\r\n
    Content-Type: application/x-www-form-urlencoded\r\n
    Keep-Alive: 0\r\n
    Host: 10.0.2.192:41471\r\n
    User-Agent: Spotify/8.9.92.408 Android/33 (Pixel 4a)\r\n
    \r\n

RESPONSE:
{"accountReq":"PREMIUM","activeUser":"","brandDisplayName":"librespot","deviceID":"4dd8ca6da0fdfebe15d6b583659f40d0768925a8","deviceType":"Speaker","groupStatus":"NONE","libraryVersion":"0.4.2","modelDisplayName":"librespot","publicKey":"P1PmuBvcJauN8CRqpEHBzSYCDBeOLgmoyEmEzDUxXKLXtFs4xDH8lyksGBOMCrnJn34xFnT4fv1mGTfe0gWGR6J6t7HBzUBK/c4aFvzFUzSavXMSY+PCYA3YDUjONYVC","remoteName":"Spotifyd@marian-xps15","resolverVersion":"0","spotifyError":0,"status":101,"statusString":"ERROR-OK","version":"2.7.1","voiceSupport":"NO"}

*/

/* addUser:
REQUEST:
    POST / HTTP/1.1\r\n
    Accept-Encoding: gzip\r\n
    Content-Length: 836\r\n
    Connection: keep-alive\r\n
    Content-Type: application/x-www-form-urlencoded\r\n
    Keep-Alive: 0\r\n
    Host: 10.0.2.192:41471\r\n
    User-Agent: Spotify/8.9.92.408 Android/33 (Pixel 4a)\r\n
    \r\n
    [Response in frame: 753]
    [Full request URI: http://10.0.2.192:41471/]
    File Data: 836 bytes
HTML Form URL Encoded: application/x-www-form-urlencoded
action=addUser&userName=the.flamming.moe&blob=0rtX%2BsJBuquRfd%2FqiCVmJJOXRHMzN4NXBMReAPHbJ637xfecpZS7%2F9HqRL7QjUypp%2BNXc6%2F8ls4fchMnOcAC3yWbL5ZkUjL%2BAX3Ibbu0GL9rlnXiM7kInjs0Acs5hTp%2FMZgx%2FNfcdMPvlB6LaokmjQI6dxL0WpMhchYrwn%2Bw59A2jiveIsMN0zJH1oOovRVVpSJni0rvkJSCB%2B1yqDGK3FNQyD6ExJEhjm1pCG%2BxYG2yZMvQAzCjaG%2FJmJf50bFhIpNovc0sI9AsMeM%2FQFw4EHjue4r7mTyQYFV2WN8XWj%2BfVvdDAsa33gSUNOHA%2FNAJ%2Fli97wUp0k%2Fx%2BUkdPp08b3fFNr3%2BIp11sIGt7ebiTAanfxWKPpXBXOC3B9nZlR2ZPWG3M68JIwE4YtQXA92GanEzRr03dLYTkaoWpgjUrtH4hrmPYFPsN0RsO9xgsO5ksT5VeH986AfrtJdHAfzzE2EQn2Y%3D&clientKey=fhBcinoCh0%2FEcs2ebA%2F9v%2ByuYqfqYP5WZzfXxxQHrO1OVR1T45DG%2FerWIX3JBB89mtqCpYZ6OIS3ivsJ2PPOh4o7OLx0Ex3NIQvgaEjdUuJmL8QFaUOr1i8A6yE6P8i9&loginId=029c492228d6c9adf01d4870f792bafc&deviceName=Pixel%204a&deviceId=f1a075ee399ebfa1db3b4e567a223a24d0453c4b&version=2.7.1

RESPONSE:
{"spotifyError":0,"status":101,"statusString":"ERROR-OK"}
*/

    return "Hello, world!";
}

static tCGI cgi_handlers[] = {
    { "/", cgi_handler }
};

// TODO Add POST handler

static void *current_connection;

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
        u16_t http_request_len, int content_len, char *response_uri,
        u16_t response_uri_len, u8_t *post_auto_wnd) {
    if (memcmp(uri, "/led.cgi", 8) == 0 && current_connection != connection) {
        current_connection = connection;
        snprintf(response_uri, response_uri_len, "/ledfail.shtml");
        *post_auto_wnd = 1;
        return ERR_OK;
    }
    return ERR_VAL;
}

// Return a value for a parameter
char *httpd_param_value(struct pbuf *p, const char *param_name, char *value_buf, size_t value_buf_len) {
    size_t param_len = strlen(param_name);
    u16_t param_pos = pbuf_memfind(p, param_name, param_len, 0);
    if (param_pos != 0xFFFF) {
        u16_t param_value_pos = param_pos + param_len;
        u16_t param_value_len = 0;
        u16_t tmp = pbuf_memfind(p, "&", 1, param_value_pos);
        if (tmp != 0xFFFF) {
            param_value_len = tmp - param_value_pos;
        } else {
            param_value_len = p->tot_len - param_value_pos;
        }
        if (param_value_len > 0 && param_value_len < value_buf_len) {
            char *result = (char *)pbuf_get_contiguous(p, value_buf, value_buf_len, param_value_len, param_value_pos);
            if (result) {
                result[param_value_len] = 0;
                return result;
            }
        }
    }
    return NULL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    err_t ret = ERR_VAL;
    LWIP_ASSERT("NULL pbuf", p != NULL);
    if (current_connection == connection) {
        char buf[4];
        char *val = httpd_param_value(p, "led_state=", buf, sizeof(buf));
        if (val) {
            bool led_on = (strcmp(val, "ON") == 0) ? true : false;
            cyw43_gpio_set(&cyw43_state, 0, led_on);
            ret = ERR_OK;
        }
    }
    pbuf_free(p);
    return ret;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    snprintf(response_uri, response_uri_len, "/ledfail.shtml");
    if (current_connection == connection) {
        snprintf(response_uri, response_uri_len, "/ledpass.shtml");
    }
    current_connection = NULL;
}


int main() {
    stdio_init_all();

    printf("Wi-Fi Player\n");

    if (cyw43_arch_init()) {
        printf("Failed to init cyw43_arch\n");
        return -1;
    }

    // LED ON during setup until wifi is up
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
    
    // TODO Move to dedicated file wifi.c ??
    cyw43_arch_enable_sta_mode();

    char hostname[sizeof(CYW43_HOST_NAME) + 4];
    memcpy(&hostname[0], CYW43_HOST_NAME, sizeof(CYW43_HOST_NAME) - 1);
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);

    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        exit(1);
    } else {
        printf("Connected.\n");
    }
    printf("Ready, running httpd at %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));


    // Setup mDNS
    cyw43_arch_lwip_begin();
    mdns_resp_init();
    printf("mdns host name %s.local\n", hostname);
    printf("LWIP >= 2.2\n");
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname);
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    // Register _spotify-connect service
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd_spotify", "_spotify-connect", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    cyw43_arch_lwip_end();

    // setup http server
    cyw43_arch_lwip_begin();
    httpd_init();
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
    cyw43_arch_lwip_end();

    // Configure audio playback
    gb_serial_init();

    // Start streaming
    gb_serial_streaming_start();

    while (true) {
        cyw43_arch_poll();
        int expected;
        while (expected = gb_serial_streaming_needs_samples()) {
            int16_t *samples = malloc(sizeof(int16_t) * expected);
            // TODO Get samples from http playback incoming samples
            gb_serial_streaming_fill_buffer(samples);
            free(samples);
        }
    }
    
    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
    cyw43_arch_deinit();
}