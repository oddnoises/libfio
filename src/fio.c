#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include "hashtable.h"

int luaopen_fio(lua_State *L);
static void *fio_thread(void *arg);
int fio_start(lua_State *L);
int fio_join(lua_State *L);
int fio_detach(lua_State *L);
int fio_getself(lua_State *L);
int fio_mutex_create(lua_State *L);
int fio_mutex_destroy(lua_State *L);
int fio_mutex_lock();
int fio_mutex_unlock();
int fio_exit();
static void init_mtx_table();
static void init_shm_table();

typedef struct {
  lua_State *L;
  pthread_t thread;
} Proc;

typedef struct {
  pthread_mutex_t mutex;
  char *name;
  size_t users;
} Mutex_s;

typedef struct {
  lua_State *shL;
  size_t users;
} Shm_s;

static pthread_mutex_t xmove_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t mtx_once = PTHREAD_ONCE_INIT;
pthread_once_t shm_once = PTHREAD_ONCE_INIT;
//HashTable_s *MutexTable = NULL;
lua_State *GlobalMutex = NULL;
HashTable_s *ShmTable = NULL;

/*-------------------------------------------------------------*/
static const struct luaL_Reg fio_f [] = {
  {"start", fio_start},
  {"exit", fio_exit},
  {"getself", fio_getself},
  {"mutex_open", fio_mutex_create},
  {"mutex_close", fio_mutex_destroy},
  {NULL, NULL}
};
/*-------------------------------------------------------------*/
static const struct luaL_Reg fio_m_thread [] = {
  {"join", fio_join},
  {"detach", fio_detach},
  {NULL, NULL}
};
/*-------------------------------------------------------------*/
static const struct luaL_Reg fio_m_mutex [] = {
  {"lock", fio_mutex_lock},
  {"unlock", fio_mutex_unlock},
  {NULL, NULL}
};
/*-------------------------------------------------------------*/
int luaopen_fio(lua_State *L)
{
  luaL_newmetatable(L, "fio.thread");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, fio_m_thread, 0);
  luaL_newmetatable(L, "fio.mutex");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, fio_m_mutex, 0);
  luaL_newlib(L, fio_f);
  return 1;
}
/*-------------------------------------------------------------*/
void *fio_thread(void *arg)
{
  lua_State *L = (lua_State *) arg;
  luaL_openlibs(L);
  luaL_requiref(L, "fio", luaopen_fio, 1);
  lua_pop(L, 1);
  if (lua_pcall(L, 0, 0, 0))
    fprintf(stderr, "Fio error: %s\n", lua_tostring(L, -1));
  lua_close(L);
  return NULL;
}
/*-------------------------------------------------------------*/
/* Запускает новый поток и возвращает его идентификатор (в виде ligth userdata) */
int fio_start(lua_State *L)
{
  Proc *new_thread;
  const char *fname = luaL_checkstring(L, 1);
  lua_State *L1 = luaL_newstate();  
  if (L1 == NULL) luaL_error(L, "Can't create new Lua state!\n");
  if (luaL_loadfile(L1, fname)) // Возможно, стоит заменить на loadbuffer в будущем
    luaL_error(L, "Error running new Lua state: %s\n", lua_tostring(L1, -1));
  new_thread = (Proc*) lua_newuserdata(L, sizeof(Proc));
  new_thread->L = L;
  if (pthread_create(&new_thread->thread, NULL, fio_thread, L1))
    luaL_error(L, "Can't create new thread: %s\n", strerror(errno));
  //pthread_detach(thread);
  luaL_getmetatable(L, "fio.thread");
  lua_setmetatable(L, -2);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_join(lua_State *L)
{
  Proc *this_thread = (Proc*) luaL_checkudata(L, 1, "fio.thread");
  pthread_join(this_thread->thread, NULL);
  return 0;
}
/*-------------------------------------------------------------*/
int fio_detach(lua_State *L)
{
  Proc *this_thread = (Proc*) luaL_checkudata(L, 1, "fio.thread");
  pthread_detach(this_thread->thread);
  return 0;
}
/*-------------------------------------------------------------*/
int fio_getself(lua_State *L)
{
  pthread_t thread;
  thread = pthread_self();
  lua_pushinteger(L, thread);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_mutex_create(lua_State *L)
{
  Mutex_s *mutex_struct;
  const char *mutex_name = luaL_checkstring(L, 1);
  int res;
  res = pthread_once(&mtx_once, init_mtx_table);
  if (res)
    luaL_error(L, "Error in pthread_once return %d value: %s\n", res, strerror(errno));
  lua_pushstring(GlobalMutex, mutex_name);
  lua_rawget(GlobalMutex, LUA_REGISTRYINDEX);
  if (!lua_isnil(GlobalMutex, -1)) {  // mutex already exists
    pthread_mutex_lock(&xmove_mutex);
    lua_xmove(GlobalMutex, L, 1);
    pthread_mutex_unlock(&xmove_mutex);
    return 1;
  }
  lua_pop(GlobalMutex, 1);

  mutex_struct = (Mutex_s*) lua_newuserdata(GlobalMutex, sizeof(Mutex_s));
  return 1;
}
/*-------------------------------------------------------------*/
int fio_mutex_destroy(lua_State *L)
{
  Mutex_s *mutex_struct;
  const char *mutex_name = luaL_checkstring(L, 1);
  int res;
  return 0;
}
/*-------------------------------------------------------------*/
int fio_mutex_lock(lua_State *L)
{
  Mutex_s *mutex_struct;
  const char *mutex_name = luaL_checkstring(L, 1);
  int res;
  return 0;
}
/*-------------------------------------------------------------*/
int fio_mutex_unlock(lua_State *L)
{
  Mutex_s *mutex_struct;
  const char *mutex_name = luaL_checkstring(L, 1);
  int res;
  return 0;
}
/*-------------------------------------------------------------*/
int fio_shm_open(lua_State *L)
{
  Shm_s *mem_struct;
  const char *shm_name = luaL_checkstring(L, 1);
  int res;
  res = pthread_once(&shm_once, init_shm_table);
  if (res)
    luaL_error(L, "Error in pthread_once: %s: %d\n", strerror(errno), __LINE__);
  res = ht_contains_key(ShmTable, (void*) shm_name);
  if (res)
    luaL_error(L, "Shm with name [%s] is already exists!\n", shm_name);
  mem_struct = malloc(sizeof(Shm_s));
  mem_struct->shL = luaL_newstate();
  mem_struct->users = 1;
  if (!mem_struct)
    luaL_error(L, "Error allocate memory for shm struct: %s\n", strerror(errno));
  res = ht_add(ShmTable, (void*) shm_name, (void*) &mem_struct);
  if (res)
    luaL_error(L, "Can't add new shm entry by key [%s]: %d\n", shm_name, res);
  return 0;
}
/*-------------------------------------------------------------*/
int fio_shm_close(lua_State *L)
{
  Shm_s *mem_struct;
  const char *shm_name = luaL_checkstring(L, 1);
  int res;
  res = ht_get(ShmTable, (void*) shm_name, (void*) mem_struct);
  if (res)
    luaL_error(L, "Error getting shm [%s] from table: %d\n", shm_name, res);
  lua_close(mem_struct->shL);
  free(mem_struct);
  res = ht_remove(ShmTable, (void*) shm_name, (void*) &mem_struct);
  if (res)
    luaL_error(L, "Error deleting shm entry [%s] from table: %d\n", shm_name, res);
  return 0;
}
/*-------------------------------------------------------------*/
/* Если главный поток завершит работу, дочерние не перестанут выполняться */
int fio_exit()
{
  pthread_exit(NULL);
  return 0;
}
/*-------------------------------------------------------------*/
static void init_mtx_table()
{
  GlobalMutex = luaL_newstate();
}
/*-------------------------------------------------------------*/
static void init_shm_table()
{
  ht_new(&ShmTable);
}
/*-------------------------------------------------------------*/

