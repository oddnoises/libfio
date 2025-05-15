fio = require "fio"
q = fio.queue_open("qname")
tbl = fio.table_open("tname")
local num_threads = 6
local num_files = 1000
local fname
local th = {}
tbl.copy = true
print("Duplicate", num_files - 10, "files with threads =", num_threads)
for i = 1, num_threads do
  th[i] = fio.start("copy_thread.lua")
  --th[i]:detach()
end

for i = 1, num_files do
  fname = "file" .. i .. ".txt"
  --print("Sending vals", i)
  q:send(-1, fname)
  --print("Vals sended")
end
tbl.copy = false

for i = 1, num_threads do
  th[i]:join()
end
tbl = fio.table_close("tname")
fio.queue_close("qname")
fio.exit()
