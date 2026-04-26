import os
import struct
import sys


def convert_wtf_to_overfusion(input_file, output_file):
    if not os.path.exists(input_file):
        print(f'Error: {input_file} not found.')
        return

    with open(input_file, 'rb') as f:
        header = f.read(1024)
        if header[0:4] != b'\x66\x54\x77\x02':
            print('Error: Invalid .wtf signature')
            return

        total_frames = struct.unpack('<I', header[4:8])[0]
        rerecords = struct.unpack('<I', header[8:12])[0]
        events = []
        last_frame_keys = set()
        f.seek(1024)
        for frame_idx in range(total_frames):
            frame_data = f.read(8)
            current_keys = set(b for b in frame_data if b != 0)
            for vk in current_keys:
                if vk not in last_frame_keys:
                    events.append(f'{frame_idx + 1},1,{vk},1')
            for vk in last_frame_keys:
                if vk not in current_keys:
                    events.append(f'{frame_idx + 1},1,{vk},0')
            last_frame_keys = current_keys

    with open(output_file, 'w') as f:
        f.write('-4,pixelsuft_overfusion,1\n')
        f.write(f'-3,total,{total_frames}\n')
        f.write(f'-2,rerecords,{rerecords}\n')
        f.write('-1,events_begin,0\n')
        for ev in events:
            f.write(ev + '\n')

    print(f'Successfully converted {total_frames} frames to {output_file}')


if __name__ == '__main__':
    convert_wtf_to_overfusion(sys.argv[1], sys.argv[2])
