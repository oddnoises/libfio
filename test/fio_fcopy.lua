fio = require "fio"

q = fio.queue_open("qname")

for i = 1, 8 do
    local fname = "file" .. i .. ".txt"
    q:send(-1, fname)
    th = fio.start("fthread.lua")
    th:detach()
end

fio.exit()
