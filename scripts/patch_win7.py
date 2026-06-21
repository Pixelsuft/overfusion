import sys


def patch_win7(fn: str):
    data = open(fn, 'rb').read()
    if data.find(b'GetSystemTimePreciseAsFileTime') == -1:
        print('Failed to patch')
        return
    data = data.replace(
        b'GetSystemTimePreciseAsFileTime', b'GetSystemTimeAsFileTime\0\0\0\0\0\0\0'
    )
    open(fn, 'wb').write(data)
    print('Patched successfully')


if __name__ == '__main__':
    patch_win7(sys.argv[1])
