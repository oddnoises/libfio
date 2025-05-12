q = fio.queue_open("qname")

fname = q:recv()

--print("Copying file:", fname)

local fspath = "./src_files/" .. fname
local fdpath = "./dst_files/" .. fname

--print(fspath)
--print(fdpath)

local in_file = io.open(fspath, "r")
if not in_file then print("Error opening src file") end
local tmp = in_file:read("*a")
in_file:close()

local out_file = io.open(fdpath, "w")
if not out_file then print("Error opening dst file") end
out_file:write(tmp)
out_file:close()
