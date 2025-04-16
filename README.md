# Fio

Fio is a multithreading library for Lua. It can create POSIX threads and provides basic synchronisation mechanism. Fio is designed to be as simple as posible to provide simple and easy to learn examples of using Lua API.

Fio supports Lua 5.4 and requires only C compiler and the Lua interpreter itself.

# Build

This project uses a [nob](https://github.com/tsoding/nob.h) build system, so it only requires a C compiler to build.

```bash
git clone https://github.com/oddnoises/libfio.git
cd fio
cc nob.c -o nob
./nob
```
> You can modify `nob.c` and binary will rebuild itself on its own.

# Module reference

| Function | Description |
| --- | --- |
| `start("thread.lua")` | Creates a new POSIX thread that will execute the code from `thread.lua` file. This function returns thread id. |
| `exit()` | Terminate current thread. This function __must__ be called only from parent thread. This is necessary to prevent early termination of child threads. |
| `:join()` | Join the specified thread. |
| `:detach()` | Detach the specified thread. |
| `getself()` | Return own thread id. |
| `mutex_open("mutex_name")` | Initialize (or open existed) a dynamic mutex with a name __mutex_name__. |
| `mutex_close("mutex_name")` | Remove specified mutex with a name __mutex_name__. |
| `:lock()` | Locks specified mutex with a name __mutex_name__. |
| `:unlock()` | Unlocks specified mutex with a name __mutex_name__. |
| ` table_open("name")` | Create a shared (or open existed) table object with a name __shm_name__ and return it to a variable tb. |
| `table_close("name")` | Delete shared table object with a name __shm_name__. |
| `tb.key1 = "String"` | Insert string value into a table __tb__ by key __key1__. |
| `str = tb.key1` | Get string value from table __tb__ by key __key1__. |
| `queue_open("qname", size)` | Create shared message queue with a name __qname__, default capacity is 10. This function returns userdata object of created queue. |
| `queue_close("qname")` | Delete shared message queue with a name __qname__. |
| `:send(value, timeout)` | Send a message in queue with timeout in ms. Default timeout is -1. Supporting values is string, bool and numbers. |
| `:recv(timeout)` | Receive a message from queue with timeout. Default timeout is -1 |



# Example of use
## Creating threads and using mutexes

Create two files __proc.lua__, __main.lua__ and run: `lua main.lua`

### main.lua
```lua
--main.lua file
local fio = require "fio"
mtx = fio.mutex_open("mtxname")
for i = 1, 100 do
    res = fio.start("proc.lua")
    res:detach()
end
print("Main thread finish executing")
os.execute("sleep " .. tonumber(3))
print("Closing mutex")
print(mtx)
fio.mutex_close("mtxname")
fio.exit()
```
### proc.lua
```lua
--proc.lua file
mtx = fio.mutex_open("mtxname")
print("Hello from thread: " .. fio.getself())
mtx:lock()
for i = -1, 1, 0.2 do
    print("i: " .. i, fio.getself())
end
mtx:unlock()
print("Goodbye from thread: " .. fio.getself())
```
## Using shared tables
```lua
local fio = require "fio"
tbl = fio.table_open("tblname")

tbl.stringval = "Test string"
tbl.numval = 123
tbl.boolval = true

tmp = tbl.numval
print("tmp = ", tmp)

tmp = tbl.stringval
print("tmp = ", tmp)

tmp = tbl.boolval
print("tmp = ", tmp)
fio.table_close("tblname")
```

## Using message queues
```lua
local fio = require "fio"

q = fio.queue_open("qname")	-- 10 elements by default
q:send("string") -- without timeout
q:send(123, 1000) -- with timeout = 1000 ms
q:send(true)

print("Received:", q:recv())
print("Received:", q:recv())
print("Received:", q:recv())
fio.queue_close("qname")
```
