import os
import sys


def patch(exe_name):
    a = open(exe_name, 'rb').read()
    b = (
        a.replace(b'comdlg32.dll', b'comovf32.dll')
        .replace(b'COMDLG32.dll', b'COMOVF32.dll')
        .replace(b'GetOpenFileNameW', b'OvfOpenFileNameT')
        .replace(b'GetOpenFileNameA', b'OvfOpenFileNameT')
        .replace(b'GetSaveFileNameW', b'OvfSaveFileNameT')
        .replace(b'GetSaveFileNameA', b'OvfSaveFileNameT')
    )
    assert a != b
    if not os.path.isdir('patched'):
        os.mkdir('patched')
    open(os.path.join('patched', exe_name), 'wb').write(b)
    print('patched')


if __name__ == '__main__':
    patch(sys.argv[1])
