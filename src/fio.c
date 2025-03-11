#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>

int luaopen_fio(lua_State *L);

//static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct Proc {
  lua_State *L;
  pthread_t thread;
  pthread_cond_t cond;
  const char *channel;
  struct Proc *prev, *next;
} Proc;

/*-------------------------------------------------------------*/
static void *fio_thread(void *arg)
{
  lua_State *L = (lua_State *) arg;
  luaL_openlibs(L);
  luaL_requiref(L, "fio", luaopen_fio, 1);
  lua_pop(L, 1);
  if (lua_pcall(L, 0, 0, 0))
    fprintf(stderr, "Fio error: %s", lua_tostring(L, -1));
  lua_close(L);
  return NULL;
}
/*-------------------------------------------------------------*/
static int fio_start(lua_State *L)
{
  pthread_t thread;
  const char *fname = luaL_checkstring(L, 1);
  lua_State *L1 = luaL_newstate();
  if (L1 == NULL) luaL_error(L, "Can't create new Lua state!");
  if (luaL_loadfile(L1, fname))
    luaL_error(L, "Error running new Lua state: %s", lua_tostring(L1, -1));
  if (pthread_create(&thread, NULL, fio_thread, L1))
    luaL_error(L, "Can't create new thread");
  pthread_detach(thread);
  return 0;
}
/*-------------------------------------------------------------*/
static int fio_exit()
{
  pthread_exit(NULL);
  return 0;
}
/*-------------------------------------------------------------*/
static int l_dir(lua_State *L)
{
  DIR *dir;
  struct dirent *entry;
  int i = 1;
  const char *path = luaL_checkstring(L, 1);
  dir = opendir(path);
  if (dir == NULL) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  }
  lua_newtable(L);
  while ((entry = readdir(dir)) != NULL) {
    lua_pushnumber(L, i++);
    lua_pushstring(L, entry->d_name);
    lua_settable(L, -3);
  }
  closedir(dir);
  return 1;
}
/*-------------------------------------------------------------*/
static const struct luaL_Reg fio [] = {
  {"start", fio_start},
  {"exit", fio_exit},
  {"dir", l_dir},
  {NULL, NULL}
};
/*-------------------------------------------------------------*/
int luaopen_fio(lua_State *L)
{
  Proc *self = (Proc *) lua_newuserdata(L, sizeof(Proc));
  lua_setfield(L, LUA_REGISTRYINDEX, "_SELF");
  self->L = L;
  self->thread = pthread_self();
  self->channel = NULL;
  pthread_cond_init(&self->cond, NULL);
  luaL_newlib(L, fio);
  return 1;
}
