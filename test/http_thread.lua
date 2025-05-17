local socket = require("socket")
local fio = require("fio")
q = fio.queue_open("qname")

while true do
  local udat = fio.queue_recv(q)
  local client = fio.udata_conv(udat, "tcp{client}")
  -- Read HTTP
  local request = client:receive("*l") or ""
  --print("Request:", request)
  local path = request:match("^GET /(.-) HTTP") or "index"
  -- workload
  local file_content = "Default response"
  local file, err = io.open(path .. ".txt", "r")
  if file then
      file_content = file:read("*a")
      file:close()
  else
      print("Error reading file:", err)
  end
  -- http response
  local response = "HTTP/1.1 200 OK\r\n"
      .. "Content-Type: text/plain\r\n"
      .. "Connection: close\r\n\r\n"
      .. "Содержимое файла '" .. path .. "':\n" .. file_content
  -- sending response
  client:send(response)
  client:close()
end
