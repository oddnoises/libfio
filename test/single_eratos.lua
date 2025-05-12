local prime = {}
local n = 10000000
local nextbase = 3
--local nthreads = 1

local function crossout(k)
  local i = 3
  while i * k <= n do
    prime[i * k] = false
    i = i + 2
  end
end

local function worker()
  local work = 0
  local lim = math.floor(math.sqrt(n))
  local base = 0
  while base <= lim do
    base = nextbase
    nextbase = nextbase + 2
    if (prime[base]) then
      crossout(base)
      work = work + 1
    end
  end
  return work
end

for i = 3, n do
  if (i % 2 == 0) then
    prime[i] = false
  else
    prime[i] = true
  end
end

local res = 0


--for i = 1, nthreads do
res = worker()
--print(res, "values of base done")
--end

local nprimes = 1
for i = 3, n do
  if (prime[i]) then
    nprimes = nprimes + 1
    --print(i)
  end
end
print(nprimes, "prime numbers was calculated")

