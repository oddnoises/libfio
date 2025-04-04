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
int fio_shm_open(lua_State *L);
int fio_shm_close(lua_State *L);
int fio_shm_set(lua_State *L);
int fio_shm_get(lua_State *L);
static void init_mtx_table();
static void init_shm_table();

typedef struct {
  lua_State *L;
  pthread_t thread;
} Proc;

typedef struct {
  pthread_mutex_t *mutex;
  char *name;
  size_t users;
} Mutex_s;

typedef struct {
  lua_State *shL;
  char *name;
  size_t users;
} Shm_s;

pthread_once_t mtx_once = PTHREAD_ONCE_INIT;
pthread_once_t shm_once = PTHREAD_ONCE_INIT;
lua_State *GlobalMutex = NULL;
lua_State *GlobalTable = NULL;

/*-------------------------------------------------------------*/
static const struct luaL_Reg fio_f [] = {
  {"start", fio_start},
  {"exit", fio_exit},
  {"getself", fio_getself},
  {"mutex_open", fio_mutex_create},
  {"mutex_close", fio_mutex_destroy},
  {"table_open", fio_shm_open},
  {"table_close", fio_shm_close},
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
static const struct luaL_Reg fio_m_table [] = {
  {"__newindex", fio_shm_set},
  {"__index", fio_shm_get},
  {"close", fio_shm_close},
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
  luaL_newmetatable(L, "fio.table");
  luaL_setfuncs(L, fio_m_table, 0);
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
  Mutex_s *mutex_struct, *mutex_new;
  const char *mutex_name = luaL_checkstring(L, 1);
  int res;
  res = pthread_once(&mtx_once, init_mtx_table);
  if (res)
    luaL_error(L, "Error in pthread_once return %d value: %s\n", res, strerror(errno));
  lua_pushstring(GlobalMutex, mutex_name);
  lua_rawget(GlobalMutex, LUA_REGISTRYINDEX);
  if (!lua_isnil(GlobalMutex, -1)) {  // mutex already exists
    mutex_struct = (Mutex_s*) lua_touserdata(GlobalMutex, 1);
    mutex_new = (Mutex_s*) lua_newuserdata(L, sizeof(Mutex_s));
    memcpy(mutex_new, mutex_struct, sizeof(Mutex_s));
    luaL_getmetatable(L, "fio.mutex");
    lua_setmetatable(L, -2);
    return 1;
  }
  lua_pop(GlobalMutex, 1);
  lua_pushstring(GlobalMutex, mutex_name);
  mutex_struct = (Mutex_s*) lua_newuserdata(GlobalMutex, sizeof(Mutex_s));
  mutex_struct->mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
  if (mutex_struct->mutex == NULL)
    luaL_error(L, "Error allocate mem for mutex [%s]: %s\n", mutex_name, strerror(errno));
  res = pthread_mutex_init(mutex_struct->mutex, NULL);
  if (res)
    luaL_error(L, "Error init new mutex: %s\n", strerror(errno));
  lua_rawset(GlobalMutex, LUA_REGISTRYINDEX);
  /* Для обмена стеками возможно использовать lua_xmove() */
  mutex_new = (Mutex_s*) lua_newuserdata(L, sizeof(Mutex_s));
  memcpy(mutex_new, mutex_struct, sizeof(Mutex_s));
  luaL_getmetatable(L, "fio.mutex");
  lua_setmetatable(L, -2);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_mutex_destroy(lua_State *L)
{
  Mutex_s *mutex_struct;
  const char *mutex_name = luaL_checkstring(L, 1);
  lua_pushstring(GlobalMutex, mutex_name);
  lua_rawget(GlobalMutex, LUA_REGISTRYINDEX);
  if (lua_isnil(GlobalMutex, -1)) {
    luaL_error(L, "Mutex [%s] doesn't exist!", mutex_name);
    lua_pop(GlobalMutex, 1);
    return 0;
  }
  mutex_struct = (Mutex_s*) lua_touserdata(GlobalMutex, 1);
  pthread_mutex_destroy(mutex_struct->mutex);
  free(mutex_struct->mutex);
  lua_pushstring(GlobalMutex, mutex_name);
  lua_pushnil(GlobalMutex);
  lua_rawset(GlobalMutex, LUA_REGISTRYINDEX);
  return 0;
}
/*-------------------------------------------------------------*/
int fio_mutex_lock(lua_State *L)
{
  Mutex_s *mutex_struct = (Mutex_s*) luaL_checkudata(L, 1, "fio.mutex");
  int res;
  res = pthread_mutex_lock(mutex_struct->mutex);
  if (res)
    luaL_error(L, "Error lock mutex: %s\n", strerror(errno));
  return 0;
}
/*-------------------------------------------------------------*/
int fio_mutex_unlock(lua_State *L)
{
  Mutex_s *mutex_struct = (Mutex_s*) luaL_checkudata(L, 1, "fio.mutex");
  int res;
  res = pthread_mutex_unlock(mutex_struct->mutex);
  if (res)
    luaL_error(L, "Error unlock mutex: %s\n", strerror(errno));
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
int fio_shm_open(lua_State *L)
{
  Shm_s *table_struct;
  const char *table_name = luaL_checkstring(L, 1);
  int res;
  res = pthread_once(&shm_once, init_shm_table);
  if (res)
    luaL_error(L, "Error in pthread_once return %d: %s\n", res, strerror(errno));
  lua_getglobal(GlobalTable, table_name);
  if (lua_istable(GlobalTable, -1)) {
    /* Table already exist, return obj */
    lua_pop(GlobalTable, 1);
    table_struct = (Shm_s*) lua_newuserdata(L, sizeof(Shm_s));
    table_struct->name = strdup(table_name);
    luaL_getmetatable(L, "fio.table");
    lua_setmetatable(L, -2);
    return 1;
  }
  lua_pop(GlobalTable, 1);
  lua_newtable(GlobalTable);
  lua_setglobal(GlobalTable, table_name);
  table_struct = (Shm_s*) lua_newuserdata(L, sizeof(Shm_s));
  table_struct->name = strdup(table_name);
  luaL_getmetatable(L, "fio.table");
  lua_setmetatable(L, -2);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_shm_close(lua_State *L)
{
  (void) L;
  return 0;
}
/*-------------------------------------------------------------*/
int fio_shm_set(lua_State *L)
{
  Shm_s *table_struct = (Shm_s*) luaL_checkudata(L, 1, "fio.table");
  const char *newindex = luaL_checkstring(L, 2);
  const char *str;
  int type = lua_type(L, 3);
  int val;
  switch (type) {
    case LUA_TSTRING:
      str = luaL_checkstring(L, 3);
      lua_getglobal(GlobalTable, table_struct->name);
      lua_pushstring(GlobalTable, newindex);
      lua_pushstring(GlobalTable, str);
      lua_settable(GlobalTable, -3);
      lua_pop(GlobalTable, 1);
    break;
    case LUA_TBOOLEAN:
    break;
    case LUA_TNUMBER:
    break;
    default:
    break;
  }
  
  return 0; 
}
/*-------------------------------------------------------------*/
int fio_shm_get(lua_State *L)
{
  Shm_s *table_struct = (Shm_s*) luaL_checkudata(L, 1, "fio.table");
  const char *index = luaL_checkstring(L, 2);
  const char *str;
  int type;
  lua_getglobal(GlobalTable, table_struct->name);
  lua_pushstring(GlobalTable, index);
  type = lua_gettable(GlobalTable, -2);
  switch (type) {
    case LUA_TSTRING:
      str = luaL_checkstring(GlobalTable, -1);
      lua_pushstring(L, str);
    break;
    default:
    break;
  }
  return 1;
}
/*-------------------------------------------------------------*/
static void init_mtx_table()
{
  GlobalMutex = luaL_newstate();
}
/*-------------------------------------------------------------*/
static void init_shm_table()
{
  GlobalTable = luaL_newstate();
}
/*-------------------------------------------------------------*/

