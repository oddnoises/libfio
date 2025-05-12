-- head -c 4294967296 /dev/urandom | tr -dc 'a-zA-Z0-9' > file1.txt

local fname = "file"

for i = 1, 8 do
    local fspath = "./src_files/" .. fname .. i .. ".txt"
    --print(fspath)
    local fdpath = "./dst_files/" .. fname .. i .. ".txt"
    --print(fdpath)
    local in_file = io.open(fspath, "r")
    if not in_file then print("Error opening src file") end
    local tmp = in_file:read("*a")
    in_file:close()
    local out_file = io.open(fdpath, "w")
    if not out_file then print("Error opening dst file") end
    out_file:write(tmp)
    out_file:close()
end
