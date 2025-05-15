local fio = require "fio"

q = fio.queue_open("qname")
qres = fio.queue_open("qres")
shared = fio.table_open("shared")

shared.n = 1000
n = shared.n
shared.nextbase = 3
nthreads = 8
prime = {}

local function get_res(pr)
  local nprimes = 0
  for i = 3, n do
    if (pr[i]) then
      nprimes = nprimes + 1
    end
  end
  return nprimes
end

--print("Filling odd primes")
for i = 3, n do
  if (i % 2 == 0) then
    prime[i] = false
  else
    prime[i] = true
  end
end

local thr

--print("Staring threads")
for i = 1, nthreads do
  thr = fio.start("qeratos_thread.lua")
  print("sending data to thread", i)
  q:send(-1, prime)
  thr:detach()
end
--[[
--print("Joining threads")
for i = 1, nthreads do
  thr[i]:join()
end
]]--
local sum = 0
for i = 1, nthreads do
  print("recv data from thread", i)
  res = qres:recv()
  sum = get_res(res)
end

--[[
--print("Results output")
local nprimes = 1
for i = 3, n do
  if (prime[i]) then
    nprimes = nprimes + 1
    --print(i)
  end
end
]]--
print(sum, "prime numbers was calculated")
