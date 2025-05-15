-- head -c 4294967296 /dev/urandom | tr -dc 'a-zA-Z0-9' > file1.txt
local function progress(cur, total, len)
    local ratio = cur / total
    local filled = math.floor(ratio * len)
    local bar = "[" .. string.rep("=", filled) .. string.rep(" ", len - filled) .. "]"
    io.write("\r" .. bar .. " " .. math.floor(ratio * 100) .. "%")
    io.flush()
end


local fname = "file"
local cli = "head -c 1048576 /dev/urandom | tr -dc 'a-zA-Z0-9' > "
for i = 1, 10000 do
    local fspath = "./src_files/" .. fname .. i .. ".txt"
    --print(cli .. fspath)
    progress(i, 10000, 50)
    os.execute(cli .. fspath)
end
print(" ")
print("Files added in /src_files directory")

