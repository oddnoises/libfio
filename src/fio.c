#include <bits/time.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>

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
int fio_queue_open(lua_State *L);
int fio_queue_close(lua_State *L);
int fio_queue_send(lua_State *L);
int fio_queue_recv(lua_State *L);

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
  char *name;
  size_t users;
} Shm_s;

struct Msg_s {
  int type;
  size_t len;
  lua_Integer lint;
  lua_Number lnum;
  bool isint;
  void *data;
  struct Msg_s *next;
};

struct Queue_s {
  struct Msg_s *msg_head, *msg_tail;
  int limit, count;
  pthread_mutex_t mtx;
  pthread_cond_t send_sig, recv_sig;
};

pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t mtx_once = PTHREAD_ONCE_INIT;
pthread_once_t shm_once = PTHREAD_ONCE_INIT;
pthread_once_t que_once = PTHREAD_ONCE_INIT;
lua_State *GlobalMutex = NULL;
lua_State *GlobalTable = NULL;
lua_State *GlobalQueue = NULL;

static int queue_send(struct Queue_s *q, struct Msg_s *msg, int timeout);
static struct Msg_s *queue_recv(struct Queue_s *q, int timeout);
static void init_mtx_table();
static void init_shm_table();
static void init_queue_table();
static struct Msg_s *pack_data(lua_State *L);
static inline int pack_set(lua_State *L, int index, struct Msg_s *msg);
static inline void pack_table(lua_State *L, int index, struct Msg_s *msg);
static int unpack_data(lua_State *L, struct Msg_s *msg);
static inline int unpack_set(lua_State *L, struct Msg_s *msg);
static inline void unpack_table(lua_State *L, struct Msg_s *msg);
/*-------------------------------------------------------------*/
static const struct luaL_Reg fio_f [] = {
  {"start", fio_start},
  {"exit", fio_exit},
  {"getself", fio_getself},
  {"mutex_open", fio_mutex_create},
  {"mutex_close", fio_mutex_destroy},
  {"table_open", fio_shm_open},
  {"table_close", fio_shm_close},
  {"queue_open", fio_queue_open},
  {"queue_close", fio_queue_close},
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
static const struct luaL_Reg fio_m_queue [] = {
  {"send", fio_queue_send},
  {"recv", fio_queue_recv},
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
  luaL_newmetatable(L, "fio.queue");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, fio_m_queue, 0);
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
/* Если главный поток завершит работу, дочерние не перестанут выполняться */
int fio_exit()
{
  pthread_exit(NULL);
  return 0;
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
    mutex_struct = (Mutex_s*) lua_touserdata(GlobalMutex, 1);
    lua_pushlightuserdata(L, mutex_struct);
    luaL_getmetatable(L, "fio.mutex");
    lua_setmetatable(L, -2);
    return 1;
  }
  lua_pop(GlobalMutex, 1);
  lua_pushstring(GlobalMutex, mutex_name);
  mutex_struct = (Mutex_s*) lua_newuserdata(GlobalMutex, sizeof(Mutex_s));
  res = pthread_mutex_init(&mutex_struct->mutex, NULL);
  if (res)
    luaL_error(L, "Error init new mutex: %s\n", strerror(errno));
  lua_rawset(GlobalMutex, LUA_REGISTRYINDEX);
  /* Для обмена стеками возможно использовать lua_xmove() */
  lua_pushlightuserdata(L, mutex_struct);
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
  pthread_mutex_destroy(&mutex_struct->mutex);
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
  res = pthread_mutex_lock(&mutex_struct->mutex);
  if (res)
    luaL_error(L, "Error lock mutex: %s\n", strerror(errno));
  return 0;
}
/*-------------------------------------------------------------*/
int fio_mutex_unlock(lua_State *L)
{
  Mutex_s *mutex_struct = (Mutex_s*) luaL_checkudata(L, 1, "fio.mutex");
  int res;
  res = pthread_mutex_unlock(&mutex_struct->mutex);
  if (res)
    luaL_error(L, "Error unlock mutex: %s\n", strerror(errno));
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
  pthread_mutex_lock(&shm_mutex);
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
      val = lua_toboolean(GlobalTable, 3);
      lua_getglobal(GlobalTable, table_struct->name);
      lua_pushstring(GlobalTable, newindex);
      lua_pushboolean(GlobalTable, val);
      lua_settable(GlobalTable, -3);
      lua_pop(GlobalTable, 1);
    break;
    case LUA_TNUMBER:
      val = luaL_checknumber(L, 3);
      lua_getglobal(GlobalTable, table_struct->name);
      lua_pushstring(GlobalTable, newindex);
      lua_pushnumber(GlobalTable, val);
      lua_settable(GlobalTable, -3);
      lua_pop(GlobalTable, 1);
    break;
    case LUA_TNIL:
      lua_getglobal(GlobalTable, table_struct->name);
      lua_pushstring(GlobalTable, newindex);
      lua_pushnil(GlobalTable);
      lua_settable(GlobalTable, -3);
      lua_pop(GlobalTable, 1);
    break;
    default:
      luaL_error(L, "Wrong type. Use only TSTRING, TBOOL, TNUMBER or TNIL!\n"); 
    break;
  }
  pthread_mutex_unlock(&shm_mutex);
  return 0; 
}
/*-------------------------------------------------------------*/
int fio_shm_get(lua_State *L)
{
  Shm_s *table_struct = (Shm_s*) luaL_checkudata(L, 1, "fio.table");
  const char *index = luaL_checkstring(L, 2);
  const char *str;
  int type, val;
  pthread_mutex_lock(&shm_mutex);
  lua_getglobal(GlobalTable, table_struct->name);
  lua_pushstring(GlobalTable, index);
  type = lua_gettable(GlobalTable, -2);
  switch (type) {
    case LUA_TSTRING:
      str = luaL_checkstring(GlobalTable, -1);
      lua_pushstring(L, str);
    break;
    case LUA_TBOOLEAN:
      val = lua_toboolean(GlobalTable, -1);
      lua_pushboolean(L, val);
    break;
    case LUA_TNUMBER:
      val = luaL_checknumber(GlobalTable, -1);
      lua_pushnumber(L, val);
    break;
    default:
      lua_pushnil(L);
    break;
  }
  pthread_mutex_unlock(&shm_mutex);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_queue_open(lua_State *L)
{
  struct Queue_s *queue_struct;
  const char *qname = luaL_checkstring(L, 1);
  int res, limit;
  if (lua_gettop(L) == 2)
    limit = luaL_checkinteger(L, 2);
  else
   limit = 10;
  res = pthread_once(&que_once, init_queue_table);
  if (res)
    luaL_error(L, "pthread_once return %d value: %s\n", res, strerror(errno));
  lua_pushstring(GlobalQueue, qname);
  lua_rawget(GlobalQueue, LUA_REGISTRYINDEX);
  /* If queue obj already exist */
  if (!lua_isnil(GlobalQueue, -1)) {
    queue_struct = (struct Queue_s*) lua_touserdata(GlobalQueue, 1);
    lua_pushlightuserdata(L, queue_struct);
    luaL_getmetatable(L, "fio.queue");
    lua_setmetatable(L, -2);
    return 1;
  }
  lua_pop(GlobalQueue, 1);
  lua_pushstring(GlobalQueue, qname);
  queue_struct = (struct Queue_s*) lua_newuserdata(GlobalQueue, sizeof(struct Queue_s));
  if (!queue_struct)
    luaL_error(L, "New userdata is NULL!\n");
  queue_struct->msg_head = queue_struct->msg_tail = NULL;
  queue_struct->limit = limit;
  queue_struct->count = 0;
  pthread_mutex_init(&queue_struct->mtx, NULL);
  pthread_cond_init(&queue_struct->send_sig, NULL);
  pthread_cond_init(&queue_struct->recv_sig, NULL);
  lua_rawset(GlobalQueue, LUA_REGISTRYINDEX);
  lua_pushlightuserdata(L, queue_struct);
  luaL_getmetatable(L, "fio.queue");
  lua_setmetatable(L, -2);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_queue_close(lua_State *L)
{
  struct Queue_s *queue_struct;
  const char *qname = luaL_checkstring(L, 1);
  struct Msg_s *msg = NULL, *last;
  lua_pushstring(GlobalQueue, qname);
  lua_rawget(GlobalQueue, LUA_REGISTRYINDEX);
  if (lua_isnil(GlobalQueue, -1)) {
    luaL_error(L, "Queue [%s] doesn't exist!\n", qname);
    return 0;
  }
  queue_struct = (struct Queue_s*) lua_touserdata(GlobalQueue, 1);
  pthread_mutex_destroy(&queue_struct->mtx);
  pthread_cond_destroy(&queue_struct->recv_sig);
  pthread_cond_destroy(&queue_struct->send_sig);
  lua_pushstring(GlobalQueue, qname);
  lua_pushnil(GlobalQueue);
  lua_rawset(GlobalQueue, LUA_REGISTRYINDEX);
  msg = queue_struct->msg_head;
  while (msg) {
    last = msg;
    msg = msg->next;
    free(last);
  }
  return 0;
}
/*-------------------------------------------------------------*/
int fio_queue_send(lua_State *L)
{
  int timeout, res;
  struct Msg_s *msg;
  struct Queue_s *queue_struct = luaL_checkudata(L, 1, "fio.queue");
  if (!queue_struct)
    luaL_error(L, "Udata is nil!\n");
  res = lua_gettop(L);
  if (res < 3)
    return luaL_error(L, "Too few arguments! Usage: q:send(timeout, value)\n");
  timeout = lua_tointeger(L, 2);
  msg = pack_data(L);
  res = queue_send(queue_struct, msg, timeout);
  if (res)
    lua_pushboolean(L, 1);
  else
    lua_pushboolean(L, 0);
  return 1;
}
/*-------------------------------------------------------------*/
int fio_queue_recv(lua_State *L)
{
  struct Queue_s *queue_struct = (struct Queue_s*) luaL_checkudata(L, 1, "fio.queue");
  struct Msg_s *msg;
  int timeout, res;
  if (lua_gettop(L) < 2)
    timeout = -1;
  else
    timeout = (int) luaL_checkinteger(L, 2);
  msg = queue_recv(queue_struct, timeout);
  res = unpack_data(L, msg);
  if (res > 1 || !res) {
    luaL_error(L, "Error after unpacking\n");
    lua_pushnil(L);
  }
  return 1;
}
/*-------------------------------------------------------------*/
static int queue_send(struct Queue_s *q, struct Msg_s *msg, int timeout)
{
  struct timespec ts;
  int res = 0;
  pthread_mutex_lock(&q->mtx);
  if (timeout > 0 && q->limit >= 0 && q->count + 1 > q->limit) {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += timeout * 1000000L;
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
  }
  while (timeout != 0 && q->limit >= 0 && q->count + 1 > q->limit) {
    if (timeout > 0) {
      if (pthread_cond_timedwait(&q->send_sig, &q->mtx, &ts))
        break;
    } else
      pthread_cond_wait(&q->send_sig, &q->mtx);
  }
  if (q->limit < 0 || q->count + 1 <= q->limit) {
    msg->next = NULL;
    if (q->msg_tail)
      q->msg_tail->next = msg;
    q->msg_tail = msg;
    if (!q->msg_head)
      q->msg_head = msg;
    q->count++;
    pthread_cond_signal(&q->recv_sig);
    res = 1;
  } else
    msg = NULL;
  pthread_mutex_unlock(&q->mtx);
  return res;
}
/*-------------------------------------------------------------*/
static struct Msg_s *queue_recv(struct Queue_s *q, int timeout)
{
  struct Msg_s *msg = NULL;
  struct timespec ts;
  pthread_mutex_lock(&q->mtx);
  if (q->limit >= 0) {
    q->limit++;
    pthread_cond_signal(&q->send_sig);
  }
  if (timeout > 0 && q->count <= 0) {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += timeout * 1000000L;
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
  }
  while (timeout != 0 && q->count <= 0) {
    if (timeout > 0) {
      if (pthread_cond_timedwait(&q->recv_sig, &q->mtx, &ts))
        break;
    } else
      pthread_cond_wait(&q->recv_sig, &q->mtx);
  }
  if (q->count > 0) {
    msg = q->msg_head;
    if (msg) {
      q->msg_head = msg->next;
      if (!q->msg_head)
        q->msg_tail = NULL;
      msg->next = NULL;
    }
    q->count--;
    pthread_cond_signal(&q->send_sig);
  }
  if (q->limit > 0)
    q->limit--;
  pthread_mutex_unlock(&q->mtx);
  return msg;
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
static void init_queue_table()
{
  GlobalQueue = luaL_newstate();
}
/*-------------------------------------------------------------*/
static struct Msg_s *pack_data(lua_State *L)
{
  struct Msg_s *parent = NULL, *msg = NULL;
  for (int i = 3; i <= lua_gettop(L); i++) {
    if (!msg)
      msg = parent = malloc(sizeof(struct Msg_s));
    else
     msg = msg->next = malloc(sizeof(struct Msg_s));
    msg->type = msg->len = 0;
    msg->data = msg->next = NULL;
    pack_set(L, i, msg);
  }
  return msg;
}
/*-------------------------------------------------------------*/
static inline int pack_set(lua_State *L, int index, struct Msg_s *msg)
{
  const char *str;
  int type = lua_type(L, index);
  msg->type = type;
  switch (type) {
    case LUA_TNIL:

    break;
    case LUA_TBOOLEAN:
      msg->data = lua_toboolean(L, index) ? (void*) 1 : NULL;
    break;
    case LUA_TNUMBER:
      if (lua_isinteger(L, index)) {
        msg->lnum = 0;
        msg->isint = true;
        msg->lint = lua_tointeger(L, index);
      } else {
        msg->lint = 0;
        msg->isint = false;
        msg->lnum = lua_tonumber(L, index);
      }
    break;
    case LUA_TSTRING:
      str = lua_tolstring(L, index, (size_t*)&msg->len);
      if (msg->len > 0)
        msg->data = memmove(malloc(msg->len), str, msg->len);
    break;
    case LUA_TLIGHTUSERDATA:
      msg->data = (void*) lua_touserdata(L, index);
    break;
    case LUA_TTABLE:
      pack_table(L, index, msg);
    break;
    default:
      luaL_error(L, "Type is not supported!\n");
      return -1;
    break;
  }
  return 0;
}
/*-------------------------------------------------------------*/
static inline void pack_table(lua_State *L, int index, struct Msg_s *msg)
{
  struct Msg_s *parent = NULL, *key = NULL, *val = NULL;
  int top = lua_gettop(L);
  int kpos = top + 1;
  int vpos = top + 2;
  int res;
  lua_pushnil(L);
  while (lua_next(L, index)) {
    if (!parent) {
      key = parent = malloc(sizeof(struct Msg_s));
      val = parent->next = malloc(sizeof(struct Msg_s));
    } else {
      key = val->next = malloc(sizeof(struct Msg_s));
      val = key->next = malloc(sizeof(struct Msg_s));
    }
    key->data = val->data = val->next = NULL;
    res = pack_set(L, kpos, key);
    if (res) {

    }
    pack_set(L, vpos, val);
    lua_pop(L, 1);
  }
  msg->data = parent;
}
/*-------------------------------------------------------------*/
static int unpack_data(lua_State *L, struct Msg_s *msg)
{
  int res = 0;
  struct Msg_s *head = NULL;
  while (msg) {
    unpack_set(L, msg);
    head = msg;
    msg = msg->next;
    free(head);
    res++;
  }
  return res;
}
/*-------------------------------------------------------------*/
static inline int unpack_set(lua_State *L, struct Msg_s *msg)
{
  switch (msg->type) {
    case LUA_TNIL:
      lua_pushnil(L);
    break;
    case LUA_TBOOLEAN:
      lua_pushboolean(L, msg->data ? 1 : 0);
    break;
    case LUA_TNUMBER:
      if (msg->isint)
        lua_pushinteger(L, msg->lint);
      else
        lua_pushnumber(L, msg->lnum);
    break;
    case LUA_TSTRING:
      lua_pushlstring(L, msg->data, msg->len);
      if (msg->len > 0)
        free(msg->data);
    break;
    case LUA_TLIGHTUSERDATA:
      lua_pushlightuserdata(L, msg->data);
    break;
    case LUA_TTABLE:
      lua_newtable(L);
        if (msg->data)
          unpack_table(L, msg->data);
    break;
    default:
      return luaL_error(L, "Unpack error\n");
    break;
  }
  return 0;
}
/*-------------------------------------------------------------*/
static inline void unpack_table(lua_State *L, struct Msg_s *msg)
{
  struct Msg_s *pkey = NULL, *pval = NULL;
  struct Msg_s *key = msg;
  struct Msg_s *val = msg->next;
  while (key && val) {
    unpack_set(L, key);
    unpack_set(L, val);
    lua_rawset(L, -3);
    pkey = key;
    pval = val;
    key = val->next;
    if (key)
      val = key->next;
    free(pkey);
    free(pval);
  }
}
/*-------------------------------------------------------------*/

