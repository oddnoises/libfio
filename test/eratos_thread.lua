prime = fio.table_open("prime")
shared = fio.table_open("shared")
mtx = fio.mutex_open("mtx_name")
q = fio.queue_open("qname")

local function crossout(k)
  local i = 3
  while i * k <= n do
    prime[i * k] = false
    i = i + 2
  end
end

print("Hello from", fio.getself())

local work = 0
local lim = math.floor(math.sqrt(shared.n))
local base = 0
while base <= lim do
  mtx:lock()
  base = shared.nextbase
  shared.nextbase = base + 2
  if (prime[base]) then
      crossout(base)
      work = work + 1
  end
  mtx:unlock()
end

print(work, "values of base done")
