# Fio

Fio is a multithreading library for Lua. It can create POSIX threads and provides basic synchronisation mechanism. Fio is designed to be as simple as posible to provide simple and easy to learn examples of using Lua API.

Fio supports Lua 5.4 and requires only C compiler and the Lua interpreter itself.

# Build

This project uses a [nob](https://github.com/tsoding/nob.h) build system, so it only requires a C compiler to build.

```bash
git clone https://github.com/oddnoises/libfio.git
```

```bash
cd fio
```

```bash
cc nob.c -o nob
```

```bash
./nob
```
> You can modify `nob.c` and binary will rebuild itself on its own.

# Module reference

| Function | Description |
| --- | --- |
| `start("thread.lua")` | Creates a new POSIX thread that will execute the code from `thread.lua` file. This function returns thread id. |
| `exit()` | Terminate current thread. This function __must__ be called only from parent thread. This is necessary to prevent early termination of child threads. |
| `join(thread)` | Join the specified thread. |
| `detach(thread)` | Detach the specified thread. |
| `getself()` | Return own thread id. |
| `mutex_init("mutex_name")` | Initializes a dynamic mutex with a name __mutex_name__. |
| `mutex_destroy("mutex_name")` | Remove specified mutex with a name __mutex_name__. |
| `mutex_lock("mutex_name")` | Locks specified mutex with a name __mutex_name__. |
| `mutex_unlock("mutex_name")` | Unlocks specified mutex with a name __mutex_name__. |
| `shm_open("shm_name")` | Create a shared memory object with a name __shm_name__. |
| `shm_close("shm_name")` | Delete shared memory object with a name __shm_name__. |

# Example of use
Create two files __proc.lua__, __main.lua__ and run: `lua main.lua`

### main.lua
```lua
--main.lua file
local fio = require "fio"
fio.mutex_init("mtxname")
for i = 1, 100 do
    res = fio.start("proc.lua")
    fio.detach(res)
end
print("Main thread finish executing")
os.execute("sleep " .. tonumber(3))
fio.mutex_destroy("mtxname")
fio.exit()
```
### proc.lua
```lua
--proc.lua file
print("Hello from thread: " .. fio.getself())
fio.mutex_lock("mtxname")
for i = -1, 1, 0.2 do
    print("i: " .. i, fio.getself())
end
fio.mutex_unlock("mtxname")
print("Goodbye from thread: " .. fio.getself())
```
