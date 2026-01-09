#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simple HTTP Server for HTTP Client POST Example

This server accepts POST requests and returns a random string in JSON format.
The client will display this random string in logs and on screen.

Usage:
    python3 server.py [--host HOST] [--port PORT]

Default:
    Host: 0.0.0.0 (all interfaces)
    Port: 8080
"""

import http.server
import socketserver
import json
import random
import string
import argparse
from urllib.parse import urlparse, parse_qs


class RandomStringHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP Request Handler that returns random strings"""

    def do_POST(self):
        """Handle POST requests"""
        # Parse the request path
        parsed_path = urlparse(self.path)
        
        # Read request body
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8')
        
        # Log the request
        print(f"[{self.client_address[0]}:{self.client_address[1]}] POST {self.path}")
        if body:
            print(f"Request body: {body}")
        
        # Generate a random string
        random_string = ''.join(random.choices(string.ascii_letters + string.digits, k=16))
        
        # Create response
        response_data = {
            "status": "success",
            "message": random_string,
            "text": random_string,  # Alternative field name
            "data": random_string,   # Another alternative
            "length": len(random_string)
        }
        
        response_json = json.dumps(response_data, ensure_ascii=False, indent=2)
        
        # Send response
        self.send_response(200)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Content-Length', str(len(response_json.encode('utf-8'))))
        self.end_headers()
        self.wfile.write(response_json.encode('utf-8'))
        
        print(f"Response: {response_json}")

    def do_GET(self):
        """Handle GET requests - return server info"""
        response_data = {
            "status": "running",
            "message": "HTTP POST Server is running",
            "endpoint": "/api/random",
            "method": "POST",
            "description": "Send POST request to /api/random to get a random string"
        }
        
        response_json = json.dumps(response_data, ensure_ascii=False, indent=2)
        
        self.send_response(200)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Content-Length', str(len(response_json.encode('utf-8'))))
        self.end_headers()
        self.wfile.write(response_json.encode('utf-8'))

    def log_message(self, format, *args):
        """Override to customize log format"""
        print(f"[SERVER] {format % args}")


def get_local_ip():
    """Get local IP address"""
    import socket
    try:
        # Connect to a remote address to determine local IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def main():
    parser = argparse.ArgumentParser(description='HTTP Server for HTTP Client POST Example')
    parser.add_argument('--host', default='0.0.0.0', help='Host to bind to (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=8080, help='Port to listen on (default: 8080)')
    
    args = parser.parse_args()
    
    local_ip = get_local_ip()
    
    print("=" * 60)
    print("HTTP POST Server for TuyaOpen HTTP Client Example")
    print("=" * 60)
    print(f"Server starting on {args.host}:{args.port}")
    print(f"Local IP address: {local_ip}")
    print(f"Access URL: http://{local_ip}:{args.port}")
    print(f"API Endpoint: http://{local_ip}:{args.port}/api/random")
    print("=" * 60)
    print("\nTo use this server:")
    print(f"1. Update SERVER_HOST in example_http_client_post.c to: {local_ip}")
    print(f"2. Update SERVER_PORT in example_http_client_post.c to: {args.port}")
    print("3. Compile and run the client example")
    print("\nPress Ctrl+C to stop the server\n")
    
    try:
        with socketserver.TCPServer((args.host, args.port), RandomStringHandler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\nServer stopped by user")
    except OSError as e:
        if e.errno == 48:  # Address already in use
            print(f"\nError: Port {args.port} is already in use.")
            print(f"Try using a different port: python3 server.py --port {args.port + 1}")
        else:
            print(f"\nError: {e}")


if __name__ == "__main__":
    main()

