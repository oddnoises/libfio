fio = require "fio"
num_threads = 2
num_files = 12
for i = 1, num_files do
  thr = fio.start("wget_thread.lua")
  thr:detach()
end
fio.exit()
