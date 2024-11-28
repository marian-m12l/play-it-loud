# TODO Write a PoC in pure python (with no librespot dependency)

# TODO mDNS service
# TODO HTTP service getInfo + addUser
# TODO HTTP client for spotify API
# TODO API calls to get track
# TODO API calls to get stream
# TODO AES Decryption
# TODO OGG/Vorbis decoding
# TODO Play decoded audio samples

import json
import socket
import threading
import zeroconf
from http.server import HTTPServer, BaseHTTPRequestHandler

print('Python PoC')


_zeroconf = zeroconf.Zeroconf()
service_info = zeroconf.ServiceInfo(
    "_spotify-connect._tcp.local.",
    "_spotify-connect._tcp.local.",
    9999,
    0,
    0,
    {
        "CPath": "/",
        "VERSION": "1.0"
    },
    "my_player_py.",
    addresses=[socket.inet_aton('10.0.2.192')]
)
_zeroconf.register_service(service_info)
threading.Thread(target=_zeroconf.start, name="zeroconf-multicast-dns-server").start()




get_info_response = {
    "status": 101,
    "statusString": "OK",
    "spotifyError": 0,
    "version": "2.9.0",
    "libraryVersion": '0.0.9',
    "accountReq": "PREMIUM",
    "brandDisplayName": "kokarare1212",
    "modelDisplayName": "librespot-python",
    "voiceSupport": "NO",
    "availability": "",
    "productID": 0,
    "tokenType": "default",
    "groupStatus": "NONE",
    "resolverVersion": "0",
    "scope": "streaming,client-authorization-universal",

    # Data computed from addUser ?
    "deviceID": "",
    "remoteName": "",
    "publicKey": "",
    "deviceType": "",
    "activeUser": ""
}

# Working reference from spotifyd:
#    {
#        "accountReq":"PREMIUM",
#        "activeUser":"",
#        "brandDisplayName":"librespot",
#        "deviceID":"4dd8ca6da0fdfebe15d6b583659f40d0768925a8",
#        "deviceType":"Speaker",
#        "groupStatus":"NONE",
#        "libraryVersion":"0.4.2",
#        "modelDisplayName":"librespot",
#        "publicKey":"P1PmuBvcJauN8CRqpEHBzSYCDBeOLgmoyEmEzDUxXKLXtFs4xDH8lyksGBOMCrnJn34xFnT4fv1mGTfe0gWGR6J6t7HBzUBK/c4aFvzFUzSavXMSY+PCYA3YDUjONYVC",
#        "remoteName":"Spotifyd@marian-xps15",
#        "resolverVersion":"0",
#        "spotifyError":0,
#        "status":101,
#        "statusString":"ERROR-OK",
#        "version":"2.7.1",
#        "voiceSupport":"NO"
#    }


add_user_response = {
    "status": 101,
    "spotifyError": 0,
    "statusString": "OK",
}

# Working reference from spotifyd:
#    {
#        "spotifyError":0,
#        "status":101,
#        "statusString":"ERROR-OK"
#    }

class MyHandler(BaseHTTPRequestHandler):
    # TODO handle GET /?action=getInfo
    def do_GET(self):
        printf("GetInfo")
        # send 200 response
        self.send_response(200)
        # send response headers
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        # send the body of the response
        self.wfile.write(json.dumps(get_info_response).encode())
    # TODO handle POST /?action=addUser
    def do_POST(self):
        printf("AddUser")
        ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
        length = int(self.headers.getheader('content-length'))
        postvars = cgi.parse_qs(self.rfile.read(length), keep_blank_values=1)
        print(postvars)
        response = json.dumps(add_user_response).encode()
        # send 200 response
        self.send_response(200)
        # send response headers
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        # send the body of the response
        self.wfile.write(response)


httpd = HTTPServer(('localhost', 9999), MyHandler)
httpd.serve_forever()



