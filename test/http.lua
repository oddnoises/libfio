local socket = require("socket")

local HOST = "0.0.0.0"
local PORT = 8080

local server = assert(socket.bind(HOST, PORT))
server:settimeout(0)
print("Server active at http://" .. HOST .. ":" .. PORT)
while true do
    local client = server:accept()
    if client then
        -- read http request
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
            print("Ошибка чтения файла:", err)
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
    socket.sleep(0.01)
end
