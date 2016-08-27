#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sys/time.h>
#include <libpub/rg_thread/rg_thread.h>
#include "rgos.h"

#define RGTHREAD_MASTER_MT 	"RGTHREAD_MASTER_MT"
#define RGTHREAD_THREAD_MT  "RGTHREAD_THREAD_MT"
enum RGTHREAD_TYPE{
	RGTHREAD_TYPE_MIN,
	RGTHREAD_TYPE_READ,
	RGTHREAD_TYPE_WRITE,
	RGTHREAD_TYPE_TIMER,
	RGTHREAD_TYPE_EVENT,
	RGTHREAD_TYPE_MAX
};

typedef struct {
    struct rg_thread_master *master;
    lua_State *L;
    int error;
}lua_rgthread_master;

typedef struct {
	lua_rgthread_master *master;
    struct rg_thread *t;
    char *name;
    int type;
    struct timeval tv;
    int fd;
	int is_run;
    int lua_ref;	/*table{name, sock, call_func, data}*/
}lua_rgthread;


static int get_socket_fd(lua_State* L, int idx) {
	int fd;
	if(lua_isnumber(L, idx)) {
		fd = lua_tonumber(L, idx);
	} else {
		luaL_checktype(L, idx, LUA_TUSERDATA);
		lua_getfield(L, idx, "getfd");
		if(lua_isnil(L, -1))
			return luaL_error(L, "Socket type missing 'getfd' method");
		lua_pushvalue(L, idx - 1);/* socket */
		lua_call(L, 1, 1);
		fd = lua_tointeger(L, -1);
		lua_pop(L, 1);
	}
	return fd;
}

static void load_timeval(double time, struct timeval *tv) {
	tv->tv_sec = (int) time;
	tv->tv_usec = (int)( (time - tv->tv_sec) * 1000000 );
}


static lua_rgthread_master * get_master(lua_State *L, int idx)
{
	lua_rgthread_master *lm  = (lua_rgthread_master *)luaL_checkudata(L, idx, RGTHREAD_MASTER_MT);
	if (lm == NULL) {
		luaL_error(L, "Need Master");
	}
	return lm;
}

static lua_rgthread * get_thread(lua_State *L, int idx)
{
	lua_rgthread *lt  = (lua_rgthread *)luaL_checkudata(L, idx, RGTHREAD_THREAD_MT);
	if (lt == NULL) {
		luaL_error(L, "Need thread");
	}
	return lt;
}

static int rgthread_thread_callback(struct rg_thread *t)
{
    lua_rgthread *lt = RG_THREAD_ARG(t);
    lua_rgthread_master *lm = lt->master;
	lua_State *L = lm->L;

	lt->t = NULL;
	/* get lua_rgthread. */
	lua_rawgeti(L, LUA_REGISTRYINDEX, lt->lua_ref);
    lua_getuservalue(L, -1);
    lua_getfield(L, -1, "func");
    lua_remove(L, -2);  /* remove table. */
    lua_pushvalue(L, -2);   /* arg1 = lt */
    /* call read func (lua)  */
	lua_pcall(L, 1, 0, 0);	/* callback(lt) */
    lua_pop(L, 1);  /*  */

	if(!lt->is_run)
		return 0;

	switch (lt->type) {
		case RGTHREAD_TYPE_TIMER:
			lt->t = rg_thread_add_timer_timeval(lm->master, rgthread_thread_callback, lt, lt->tv);
			break;
		case RGTHREAD_TYPE_READ:
			lt->t = rg_thread_add_read(lm->master, rgthread_thread_callback, lt, lt->fd);
			break;
		case RGTHREAD_TYPE_WRITE:
			lt->t = rg_thread_add_write(lm->master, rgthread_thread_callback, lt, lt->fd);
			break;
	}

    return 0;
}

static int rgthread_thread_change_timer(lua_State *L)
{
	lua_rgthread * lt = get_thread(L, 1);
	double time;

	if(lt->type != RGTHREAD_TYPE_TIMER)
		luaL_error(L, "must be timer thread");
	time = luaL_checknumber(L, 2);
    load_timeval(time, &(lt->tv));
	return 0;
}

static int rgthread_thread_stop(lua_State *L)
{
	lua_rgthread * lt = get_thread(L, 1);
	RG_THREAD_OFF(lt->t);
	lt->is_run = RGOS_FALSE;
	return 0;
}

static int rgthread_thread_restart(lua_State *L)
{
	lua_rgthread * lt = get_thread(L, 1);
	if(lt->is_run || lt->t)
		return 0;

	lt->is_run = 1;
	switch (lt->type) {
		case RGTHREAD_TYPE_TIMER:
			lt->t = rg_thread_add_timer_timeval(lt->master->master, rgthread_thread_callback, lt, lt->tv);
			break;
		case RGTHREAD_TYPE_READ:
			lt->t = rg_thread_add_read(lt->master->master, rgthread_thread_callback, lt, lt->fd);
			break;
		case RGTHREAD_TYPE_WRITE:
			lt->t = rg_thread_add_write(lt->master->master, rgthread_thread_callback, lt, lt->fd);
			break;
	}

	return 0;
}


lua_rgthread *rgthread_thread_new(lua_rgthread_master *lm)
{
	lua_rgthread * lt = lua_newuserdata(lm->L, sizeof(lua_rgthread));

    lt->master = lm;
    luaL_getmetatable(lm->L, RGTHREAD_THREAD_MT);
    lua_setmetatable(lm->L, -2);
	return lt;
}


int rgthread_thread_gc(lua_State *L)
{
	lua_rgthread * lt = get_thread(L, 1);
	RG_THREAD_OFF(lt->t);
	lt->is_run = RGOS_FALSE;
	luaL_unref(L, LUA_REGISTRYINDEX, lt->lua_ref);
	if(lt->name)
		rgos_free(lt->name);
	return 0;
}


/* master:add_thread{name, sock, read,  data} */
static int rgthread_add_thread(lua_State *L)
{
	lua_rgthread_master * lm = get_master(L, 1);
	lua_rgthread *lt;
	int type;
	struct timeval tv;
    double time;
	int fd;
	int event;

	if (!lua_istable(L, 2))
		luaL_error(L, "arg 2 must be a table");
	/* check table format must be {name="",type="", sock=sock, func=read, data = data }
	 * name and type must exist.
	 */
	lua_getfield(L, 2, "type");
	lua_getfield(L, 2, "func");
	if(!lua_isfunction(L, -1) || !lua_isinteger(L, -2))
		luaL_error(L, "type and func must exist");
	type = (int)lua_tointeger(L, -2);
	if (type >= RGTHREAD_TYPE_MAX || type <= RGTHREAD_TYPE_MIN)
		luaL_error(L, "type is error");
	lua_pop(L, 2);

	if (type == RGTHREAD_TYPE_READ || type == RGTHREAD_TYPE_WRITE) {
		lua_getfield(L, 2, "sock");
		if (lua_isnoneornil(L, -1))
			luaL_error(L, "socket must be exist");
		fd = get_socket_fd(L, -1);
		lua_pop(L, 1);
	} else if (type == RGTHREAD_TYPE_TIMER) {
		lua_getfield(L, 2, "timer");
		if (lua_isnoneornil(L, -1))
			luaL_error(L, "timer must be exist");
		time = luaL_checknumber(L, -1);
        load_timeval(time, &tv);

		lua_pop(L, 1);
	} else {
		lua_getfield(L, 2, "event");
		if (lua_isnoneornil(L, -1))
			luaL_error(L, "event must be exist");
		event = luaL_checkinteger(L, -1);
		lua_pop(L, 1);
	}

	lt = rgthread_thread_new(lm);	/* */
	lua_pushvalue(L, 2);	        /* table  */
	lua_setuservalue(L, -2);
    lua_pushvalue(L, -1);   /* push lua_rgthread. */
	lt->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lt->type = type;
    lt->fd = fd;
    lt->tv = tv;
	lt->is_run = RGOS_TRUE;
	switch (type) {
		case RGTHREAD_TYPE_TIMER:
			lt->t = rg_thread_add_timer_timeval(lm->master, rgthread_thread_callback, lt, tv);
			break;
		case RGTHREAD_TYPE_READ:
			lt->t = rg_thread_add_read(lm->master, rgthread_thread_callback, lt, fd);
			break;
		case RGTHREAD_TYPE_WRITE:
			lt->t = rg_thread_add_write(lm->master, rgthread_thread_callback, lt, fd);
			break;
		case RGTHREAD_TYPE_EVENT:
			lt->t = rg_thread_add_event(lm->master, rgthread_thread_callback, lt, event);
			break;
	}
    /* get name, get sock */
    return 1;   /* return lua_rgthread */
}

/* master:delete_thread(name) */
static int rgthread_del_thread(lua_State *L)
{
    return 0;
}

/* master:loop() */
static int rgthread_loop(lua_State *L)
{
	lua_rgthread_master * lm = get_master(L, 1);
	struct rg_thread thread;

	while(rg_thread_fetch(lm->master, &thread)) {
		rg_thread_call(&thread);
	}
    return 0;
}

int rgthread_master_tostring(lua_State *L)
{
	lua_rgthread_master *lm = get_master(L, 1);
	lua_pushfstring(L, "rgthread_master(%p)", lm->master);
	return 1;
}


int rgthread_master_gc(lua_State *L)
{
	lua_rgthread_master *lm = get_master(L, 1);

	lm->L = NULL;
	rg_thread_master_finish(lm->master);
	/* 最好还要把所有lua_rgthread一并清理干净... */
	return 0;
}


static luaL_Reg master_funcs[] = {
    {"add_thread", rgthread_add_thread},
    {"del_thread", rgthread_del_thread},
    {"loop", rgthread_loop},
    {"__gc", rgthread_master_gc},
    {"__tostring", rgthread_master_tostring},
    {NULL, NULL}
};

static int rgthread_thread_socket(lua_State *L)
{
	lua_getuservalue(L, 1);
	lua_getfield(L, -1, "socket");
	lua_remove(L, -2);	/* ??? */
	return 1;
}
static int rgthread_thread_name(lua_State *L)
{
	lua_getuservalue(L, 1);
	lua_getfield(L, -1, "name");
	lua_remove(L, -2);	/* ??? */
	return 1;
}

static int rgthread_thread_type(lua_State *L)
{
	lua_getuservalue(L, 1);
	lua_getfield(L, -1, "type");
	lua_remove(L, -2);	/* ??? */
	return 1;
}

static int rgthread_thread_data(lua_State *L)
{
	lua_getuservalue(L, 1);
	lua_getfield(L, -1, "data");
	lua_remove(L, -2);	/* ??? */
	return 1;
}

static int rgthread_thread_tostring(lua_State *L)
{
	lua_rgthread *lt = get_thread(L, 1);
	lua_pushfstring(L, "rgthread{name:%s,type:%d,thread:%p}", lt->name, lt->type, lt->t);
	return 1;
}

static luaL_Reg thread_funcs[] = {
    {"get_socket", rgthread_thread_socket},
    {"get_name", rgthread_thread_name},
    {"get_type", rgthread_thread_type},
    {"get_data", rgthread_thread_data},
    {"stop", rgthread_thread_stop},
    {"restart", rgthread_thread_restart},
    {"change_timer", rgthread_thread_change_timer},
    {"__gc", rgthread_thread_gc},
    {"__tostring", rgthread_thread_tostring},
    {NULL, NULL}
};


int rgthread_master_new(lua_State *L)
{
    lua_rgthread_master *master= lua_newuserdata(L, sizeof(lua_rgthread_master));
    master->L = L;
    master->master = rg_thread_master_create();
    luaL_getmetatable(L, RGTHREAD_MASTER_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static luaL_Reg funcs[] = {
    {"new_master", rgthread_master_new},
    {NULL, NULL}
};

static void create_meta (lua_State *L, char *mt, luaL_Reg *funcs) {
  luaL_newmetatable(L, mt);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, funcs, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


static void rgthread_master_mt_init(lua_State *L)
{
	create_meta(L, RGTHREAD_MASTER_MT, master_funcs);
}

static void rgthread_thread_mt_init(lua_State *L)
{
	create_meta(L, RGTHREAD_THREAD_MT, thread_funcs);
}


int luaopen_rgos_rgthread(lua_State *L)
{
    rgthread_master_mt_init(L);
	rgthread_thread_mt_init(L);
    luaL_newlib(L, funcs);
    return 1;
}
