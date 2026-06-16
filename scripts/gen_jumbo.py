import os

cwd = os.path.dirname(os.path.dirname(__file__)) or os.getcwd()

f = open(os.path.join(cwd, 'jumbo', 'main.cpp'), 'w', encoding='utf-8')
f.write("""#define JUMBO_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define JSON_NOEXCEPTION
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#define _WINSOCKAPI_
#include "../imgui/backends/imgui_impl_dx9.cpp"
#include "../imgui/backends/imgui_impl_win32.cpp"
#include "../imgui/imgui.cpp"
#include "../imgui/imgui_draw.cpp"
#include "../imgui/imgui_tables.cpp"
#include "../imgui/imgui_widgets.cpp"
#include "../minhook/src/buffer.c"
#include "../minhook/src/hde/hde32.c"
#include "../minhook/src/hook.c"
#include "../minhook/src/trampoline.c"
#include "../spdlog/src/async.cpp"
#include "../spdlog/src/bundled_fmtlib_format.cpp"
#include "../spdlog/src/cfg.cpp"
#include "../spdlog/src/color_sinks.cpp"
#include "../spdlog/src/file_sinks.cpp"
#include "../spdlog/src/spdlog.cpp"
#include "../spdlog/src/stdout_sinks.cpp"
""")

plug_list = []
for fn in os.listdir(os.path.join(cwd, 'plugins')):
    if not fn.endswith('.cpp'):
        continue
    pf = open(os.path.join(cwd, 'plugins', fn)).read().split('PLUG_REG(')
    assert len(pf) == 2
    plug_list.append(pf[1].split(')')[0].strip())
    f.write(f'#include "../plugins/{fn}"\n')

f.write('#define JUMBO_PLUGIN_DETECTION() ')
for i in plug_list:
    f.write(f'else if (auto val = {i}::on_plugin_check())')
    f.write(' { _cur_plug = val.value(); } ')
f.write('\n')

for fn in os.listdir(os.path.join(cwd, 'src')):
    if not fn.endswith('.cpp'):
        continue
    f.write(f'#include "../src/{fn}"\n')

for fn in os.listdir(os.path.join(cwd, 'tools')):
    if not fn.endswith('.cpp'):
        continue
    f.write(f'#include "../tools/{fn}"\n')
