#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct Proc {
  lua_State *L;
  pthread_t thread;
  pthread_cond_t cond;
  const char *channel;
  struct Proc *prev, *next;
} Proc;

static Proc *waitsend = NULL;
static Proc *waitrecv = NULL;
/* Return Proc struct related to the lua_State */
static Proc *getself(lua_State *L)
{
  Proc *p;
  lua_getfield(L, LUA_REGISTRYINDEX, "_SELF");
  p = (Proc *) lua_touserdata(L,-1);
  lua_pop(L,1);
  return p;
}
/*-------------------------------------------------------------*/
/* Send data between states */
static void move_values(lua_State *send, lua_State *recv)
{
  int n = lua_gettop(send);
  for (int i = 2; i <= n; i++) {
    lua_pushstring(recv, lua_tostring(send, i));
  }
}
/*-------------------------------------------------------------*/
/* Find waiting for data transfer process */
static Proc *findmatch(const char *channel, Proc **list)
{
  Proc *node = *list;
  if (node == NULL) return NULL; // List is empty
  while (node != *list) {
    if (!(strcmp(channel, node->channel))) {
      if (*list == node) {
        *list = (node->next == node) ? NULL : node->next;
      }
      node->prev->next = node->next;
      node->next->prev = node->prev;
      return node;
    }
    node = node->next;
  }
  return NULL;
}
/*-------------------------------------------------------------*/
/* Add process to waiting list */
static void waitonlist(lua_State *L, const char *channel, Proc **list)
{
  Proc *p = getself(L);
  /* Link itself at the end of list */
  if (*list == NULL) {
    *list = p;
    p->prev = p->next = p;
  } else {
    p->prev = (*list)->prev;
    p->next = *list;
    p->prev->next = p->next->prev = p;
  }
  p->channel = channel;
  while (p->channel) {
    pthread_cond_wait(&p->cond, &mutex);
  }
}
/*-------------------------------------------------------------*/
/* Send string */
static int l_send(lua_State *L)
{
  Proc *p;
  const char *channel = luaL_checkstring(L, 1);
  pthread_mutex_lock(&mutex);
  p = findmatch(channel, &waitrecv);
  if (p) {
    move_values(L, p->L);
    p->channel = NULL;
    pthread_cond_signal(&p->cond);
  } else {
    waitonlist(L, channel, &waitsend);
  }
  pthread_mutex_unlock(&mutex);
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
  {"dir", l_dir},
  {NULL, NULL}
};

int luaopen_fio(lua_State *L)
{
  luaL_newlib(L, fio);
  return 1;
}
