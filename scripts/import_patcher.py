a = open('FiveNightsatFreddys.exe', 'rb').read()
b = (
    a.replace(b'comdlg32.dll', b'comovf32.dll')
    .replace(b'COMDLG32.dll', b'COMOVF32.dll')
    .replace(b'GetOpenFileNameW', b'OvfOpenFileNameT')
    .replace(b'GetOpenFileNameA', b'OvfOpenFileNameT')
    .replace(b'GetSaveFileNameW', b'OvfSaveFileNameT')
    .replace(b'GetSaveFileNameA', b'OvfSaveFileNameT')
)
assert a != b
open('patched/FiveNightsatFreddys.exe', 'wb').write(b)
print('patched')
