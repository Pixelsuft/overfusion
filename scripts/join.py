# Join all the files into a single big txt file which you can throw into DeepSeek
import os


def list_files(folder: str, ext: tuple) -> list:
    path = os.path.join(cwd, folder)
    ret = []
    for i in os.listdir(path):
        fp = os.path.join(path, i)
        if os.path.isfile(fp) and i.split('.')[-1] in ext:
            ret.append(fp)
    return ret


cwd = os.path.dirname(os.path.dirname(__file__)) or os.getcwd()
file_list = (
    list_files('src', ('cpp', 'hpp'))
    + list_files('tools', ('cpp', 'hpp'))
    + list_files('plugins', ('cpp',))
    + list_files('scripts', ('c', 'py', 'bat'))
)

out = open('joined.txt', 'w', encoding='utf-8')
for fp in file_list:
    out.write(fp + '\n')
    out.write(open(fp, 'r', encoding='utf-8').read() + '\n')
out.close()
