
q = fio.queue_open("qname")
shared = fio.table_open("shared")
qres = fio.queue_open("qres")

--mtx = fio.mutex_open("m_name")
--print("Hello from", fio.getself())

prime = q:recv()
local work = 0
local n = shared.n
local lim = math.floor(math.sqrt(n))
local base = 0

local function crossout(k)
  local i = 3
  while i * k <= n do
    prime[i * k] = false
    i = i + 2
  end
end

while base <= lim do
  --mtx:lock()
  base = shared.nextbase
  shared.nextbase = base + 2
  if (prime[base]) then
      crossout(base)
      work = work + 1
  end
  --mtx:unlock()
end
print("Sending res from thread", fio.getself())
qres:send(-1, prime)
--print(work, "values of base done")
