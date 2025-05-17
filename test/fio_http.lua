local socket = require("socket")
local fio = require("fio")
q = fio.queue_open("qname")

local HOST = "0.0.0.0"
local PORT = 8080
local num_threads = 4

local server = assert(socket.bind(HOST, PORT))
server:settimeout(0)

for i = 1, num_threads do
    local thr = fio.start("http_thread.lua")
    thr:detach()
end
print("Server active at http://" .. HOST .. ":" .. PORT)
while true do
    local client = server:accept()
    if client then
        -- read http request
        fio.queue_send(q, -1, client)
    end
    socket.sleep(0.01)
end

