#include <debugger/luathread.h>
#include <debugger/impl.h>
#include <debugger/thunk.h>

namespace vscode
{
	static int get_stacklevel(lua_State* L)
	{
		lua::Debug ar;
		int n;
		for (n = 0; lua_getstack(L, n + 1, (lua_Debug*)&ar) != 0; ++n)
		{
		}
		return n;
	}

	static int get_stacklevel(lua_State* L, int pos)
	{
		lua::Debug ar;
		if (lua_getstack(L, pos, (lua_Debug*)&ar) != 0) {
			for (; lua_getstack(L, pos + 1, (lua_Debug*)&ar) != 0; ++pos)
			{
			}
		}
		else if (pos > 0) {
			for (--pos; pos > 0 && lua_getstack(L, pos, (lua_Debug*)&ar) == 0; --pos)
			{
			}
		}
		return pos;
	}

	static void debugger_hook(luathread* thread, lua_State *L, lua::Debug *ar)
	{
		if (!thread->enable) return;
		thread->dbg->hook(thread, L, ar);
	}

	static void debugger_panic(luathread* thread, lua_State *L)
	{
		if (!thread->enable) return;
		thread->dbg->panic(thread, L);
	}

	luathread::luathread(int id, debugger_impl* dbg, lua_State* L)
		: id(id)
		, enable(true)
		, release(false)
		, dbg(dbg)
		, L(L)
		, oldpanic(lua_atpanic(L, 0))
		, thunk_hook((lua_Hook)thunk_create_hook(
			reinterpret_cast<intptr_t>(this),
			reinterpret_cast<intptr_t>(&debugger_hook)
		))
		, thunk_panic((lua_CFunction)thunk_create_panic(
			reinterpret_cast<intptr_t>(this),
			reinterpret_cast<intptr_t>(&debugger_panic),
			reinterpret_cast<intptr_t>(oldpanic)
		))
		, step_(step::in)
		, stepping_target_level_(0)
		, stepping_current_level_(0)
		, stepping_lua_state_(NULL)
		, cur_function(0)
		, has_function(false)
		, has_breakpoint(false)
		, ob_(id)
	{
		install_hook(LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKEXCEPTION);
		lua_atpanic(L, thunk_panic);
	}

	luathread::~luathread()
	{
		if (release) return;
		lua_sethook(L, 0, 0, 0);
		lua_atpanic(L, oldpanic);
		thunk_destory(thunk_hook);
		thunk_destory(thunk_panic);
	}

	void luathread::install_hook(int mask)
	{
		lua_sethook(L, thunk_hook, mask, 0);
	}

	void luathread::release_thread()
	{
		release = true;
	}

	void luathread::enable_thread()
	{
		enable = true;
	}

	void luathread::disable_thread()
	{
		enable = false;
	}

	void luathread::set_step(step step)
	{
		step_ = step;
	}

	bool luathread::is_step(step step)
	{
		return step_ == step;
	}

	bool luathread::check_step(lua_State* L, lua::Debug* ar)
	{
		if (is_step(luathread::step::in)) return true;
		return stepping_lua_state_ == L && stepping_current_level_ <= stepping_target_level_;
	}

	void luathread::step_in()
	{
		set_step(step::in);
	}

	void luathread::step_over(lua_State* L, lua::Debug* ar)
	{
		set_step(step::over);
		stepping_target_level_ = stepping_current_level_ = get_stacklevel(L);
		stepping_lua_state_ = L;
	}

	void luathread::step_out(lua_State* L, lua::Debug* ar)
	{
		set_step(step::out);
		stepping_target_level_ = stepping_current_level_ = get_stacklevel(L);
		stepping_target_level_--;
		stepping_lua_state_ = L;
	}

	void luathread::hook_callret(lua_State* L, lua::Debug* ar)
	{
		has_function = false;
		has_breakpoint = false;
		cur_function = nullptr;
		switch (ar->event) {
		case LUA_HOOKTAILCALL:
			break;
		case LUA_HOOKCALL:
			if (stepping_lua_state_ == L) {
				stepping_current_level_++;
			}
			break;
		case LUA_HOOKRET:
			if (stepping_lua_state_ == L) {
				stepping_current_level_ = get_stacklevel(L, stepping_current_level_) - 1;
			}
			break;
		}
	}

	void luathread::hook_line(lua_State* L, lua::Debug* ar, breakpoint& breakpoint)
	{
		if (!has_function) {
			has_function = true;
			has_breakpoint = false;
			cur_function = breakpoint.get_function(L, ar);
			if (cur_function) {
				has_breakpoint = cur_function->src->has_breakpoint();
			}
		}
	}

	void luathread::update_breakpoint()
	{
		if (cur_function) {
			has_function = false;
			has_breakpoint = false;
		}
	}

	void luathread::reset_session(lua_State* L)
	{
		ob_.reset(L);
	}

	void luathread::evaluate(lua_State* L, lua::Debug *ar, debugger_impl* dbg, rprotocol& req, int frameId)
	{
		ob_.evaluate(L, ar, dbg, req, frameId);
	}

	void luathread::new_frame(lua_State* L, debugger_impl* dbg, rprotocol& req, int frameId)
	{
		ob_.new_frame(L, dbg, req, frameId);
	}

	void luathread::get_variable(lua_State* L, debugger_impl* dbg, rprotocol& req, int64_t valueId, int frameId)
	{
		ob_.get_variable(L, dbg, req, valueId, frameId);
	}

	void luathread::set_variable(lua_State* L, debugger_impl* dbg, rprotocol& req, int64_t valueId, int frameId)
	{
		ob_.set_variable(L, dbg, req, valueId, frameId);
	}
}
