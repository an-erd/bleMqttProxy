import http.server, ssl

server_address = ('192.168.2.130', 8070)
httpd = http.server.HTTPServer(server_address, http.server.SimpleHTTPRequestHandler)
httpd.socket = ssl.wrap_socket (httpd.socket,
		server_side=True,
        keyfile= "ca_key.pem",
        certfile= "ca_cert.pem",
	   ssl_version=ssl.PROTOCOL_TLS)
httpd.serve_forever()
