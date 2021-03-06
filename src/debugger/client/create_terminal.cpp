#include <debugger/io/namedpipe.h>
#include <Windows.h>
#include <debugger/client/run.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/client/stdinput.h>
#include <base/util/format.h>
#include <base/path/self.h>
#include <functional>

void request_runInTerminal(vscode::io::base* io, std::function<void(vscode::wprotocol&)> args)
{
	vscode::wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("request");
		//res("seq").Int64(seq++);
		res("command").String("runInTerminal");
		for (auto _ : res("arguments").Object())
		{
			args(res);
		}
	}
	vscode::io_output(io, res);
}

bool create_terminal_with_debugger(stdinput& io, vscode::rprotocol& req, const std::wstring& port)
{
	auto& args = req["arguments"];

	request_runInTerminal(&io, [&](vscode::wprotocol& res) {
		fs::path dbg_path = base::path::self().remove_filename();
		std::string luaexe;
		if (args.HasMember("luaexe") && args["luaexe"].IsString()) {
			luaexe = args["luaexe"].Get<std::string>();
		}
		else {
			luaexe = base::w2u((dbg_path / L"lua.exe").wstring());
		}

		res("kind").String(args["console"] == "integratedTerminal" ? "integrated" : "external");
		res("title").String("Lua Debug");
		if (args.HasMember("cwd") && args["cwd"].IsString()) {
			res("cwd").String(args["cwd"]);
		}
		else {
			res("cwd").String(base::w2u(fs::path(luaexe).remove_filename().wstring()));
		}
		if (args.HasMember("env") && args["env"].IsObject()) {
			for (auto _ : res("env").Object()) {
				for (auto& v : args["env"].GetObject()) {
					if (v.name.IsString()) {
						if (v.value.IsString()) {
							res(v.name).String(v.value);
						}
						else if (v.value.IsNull()) {
							res(v.name).Null();
						}
					}
				}
			}
		}

		for (auto _ : res("args").Array()) {
			res.String(luaexe);

			std::string script = create_install_script(req, dbg_path, port, true);
			res.String("-e");
			res.String(script);

			if (args.HasMember("arg0")) {
				if (args["arg0"].IsString()) {
					auto& v = args["arg0"];
					res.String(v);
				}
				else if (args["arg0"].IsArray()) {
					for (auto& v : args["arg0"].GetArray()) {
						if (v.IsString()) {
							res.String(v);
						}
					}
				}
			}

			std::string program = ".lua";
			if (args.HasMember("program") && args["program"].IsString()) {
				program = args["program"].Get<std::string>();
			}
			res.String(program);

			if (args.HasMember("arg") && args["arg"].IsArray()) {
				for (auto& v : args["arg"].GetArray()) {
					if (v.IsString()) {
						res.String(v);
					}
				}
			}
		}
	});
	return true;
}
