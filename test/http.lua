-- wget --delete-after http://0.0.0.0/
local socket = require("socket")
local fio = require "fio"
q = fio.queue_open("qname")

PORT=80
BACKLOG=5

server=assert(socket.tcp())
assert(server:bind("*", PORT))
server:listen(BACKLOG)

local ip, port = server:getsockname()
print("Listening on IP="..ip..", PORT="..port.."...")
local cnt = 0
while 1 do
	-- wait for a connection from any client
	local client, err = server:accept()
	print(client)
	print(q)
	q:send(-1, client)
	local udat = q:recv()
	local newcl = fio.udata_conv(udat, "tcp{client}")
	print(newcl)

	if newcl then
		local line, err = newcl:receive()
		-- if there was no error, send it back to the client
		if not err then
			newcl:send("HTTP/1.0 200 OKnnnAnswer from server!")
		end

	else
		print("Error happened while getting the connection.nError: "..err)
	end
	cnt = cnt + 1
	-- done with client, close the object
	newcl:close()
	print("Closed: " .. cnt)
end
