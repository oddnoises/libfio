local fio = require "fio"

prime = fio.table_open("prime")
shared = fio.table_open("shared")




shared.n = 100
shared.nextbase = 3
nthreads = 12


--print("Filling odd primes")
for i = 3, shared.n do
  if (i % 2 == 0) then
    prime[i] = false
  else
    prime[i] = true
  end
end

local thr = {}

--print("Staring threads")
for i = 1, nthreads do
  thr[i] = fio.start("eratos_thread.lua")
  --thr:detach()
end

--print("Joining threads")
for i = 1, nthreads do
  thr[i]:join()
end

--print("Results output")
local nprimes = 1
for i = 3, shared.n do
  if (prime[i]) then
    nprimes = nprimes + 1
    print(i)
  end
end

print(nprimes, "prime numbers was calculated")
