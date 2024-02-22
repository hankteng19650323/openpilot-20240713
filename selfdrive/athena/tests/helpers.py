import http.server
import threading
import socket
from functools import wraps
import time


class MockResponse:
  def __init__(self, json, status_code):
    self.json = json
    self.text = json
    self.status_code = status_code


class EchoSocket():
  def __init__(self, port):
    self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.socket.bind(('127.0.0.1', port))
    self.socket.listen(1)

  def run(self):
    conn, _ = self.socket.accept()
    conn.settimeout(5.0)

    try:
      while True:
        data = conn.recv(4096)
        if data:
          print(f'EchoSocket got {data}')
          conn.sendall(data)
        else:
          break
    finally:
      conn.shutdown(0)
      conn.close()
      self.socket.shutdown(0)
      self.socket.close()


class MockApi():
  def __init__(self, dongle_id):
    pass

  def get_token(self):
    return "fake-token"


class MockWebsocket():
  def __init__(self, recv_queue, send_queue):
    self.recv_queue = recv_queue
    self.send_queue = send_queue

  def recv(self):
    data = self.recv_queue.get()
    if isinstance(data, Exception):
      raise data
    return data

  def send(self, data, opcode):
    self.send_queue.put_nowait((data, opcode))


class HTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
  def do_PUT(self):
    print('do_PUT')
    length = int(self.headers['Content-Length'])
    should_delay = self.headers.get('X-Delay-Upload') == 'true'
    if not should_delay:
      self.rfile.read(length)
    else:
      # time.sleep(10)
      data = self.rfile.read(16 * 1024)
      while data:
      # for i in range(length):
        data = self.rfile.read(16 * 1024)
        print('reading')
        time.sleep(0.01)

    print('after rfile')
    self.send_response(201, "Created")
    self.end_headers()


def with_http_server(func, handler=http.server.BaseHTTPRequestHandler, setup=None):
  @wraps(func)
  def inner(*args, **kwargs):
    host = '127.0.0.1'
    server = http.server.HTTPServer((host, 0), handler)
    port = server.server_port
    t = threading.Thread(target=server.serve_forever)
    t.start()

    if setup is not None:
      setup(host, port)

    try:
      return func(*args, f'http://{host}:{port}', **kwargs)
    finally:
      server.shutdown()
      server.server_close()
      t.join()

  return inner
