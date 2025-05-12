local fio = require "fio"
print("Begin test")
prime = fio.table_open("prime")
shared = fio.table_open("shared")
mtx = fio.mutex_open("mtx_name")
q = fio.queue_open("qname")


shared.n = 30
shared.nextbase = 3
nthreads = 4


print("Filling odd primes")
for i = 3, shared.n do
  if (i % 2 == 0) then
    prime[i] = false
  else
    prime[i] = true
  end
end

local thr = {}

print("Staring threads")
for i = 1, nthreads do
  thr[i] = fio.start("eratos_thread.lua")
end

for i = 1, nthreads do
  thr[i]:join()
end

local nprimes = 1
for i = 3, shared.n do
  if (prime[i]) then
    nprimes = nprimes + 1
    print(i)
  end
end

