q = fio.queue_open("qname")
tbl = fio.table_open("tname")

local fname = "str"
local cnt = 0
while true do
  if not tbl.copy then
    break
  end
  fname = q:recv()
  --print("recv:", fname)
  local fspath = "./src_files/" .. fname
  local fdpath = "./dst_files/" .. fname
  local in_file = io.open(fspath, "r")
  local tmp = in_file:read("*a")
  in_file:close()
  local out_file = io.open(fdpath, "w")
  if not out_file then print("Error opening dst file") end
  out_file:write(tmp)
  out_file:close()
  cnt = cnt + 1
end

print(cnt, "files duplicated")
