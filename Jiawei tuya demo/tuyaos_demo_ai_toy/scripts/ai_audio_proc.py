# 监听5055 TCP端口，将音频数据保存到pcm文件中，文件名为当前时间戳，多个线程同时运行

import socket
import time
import threading
import wave
import pyaudio

'''
# 安装pyaudio
sudo apt install portaudio19-dev
pip install pyaudio
'''

def audio_proc():
    # 创建socket,SO_REUSEADDR 
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # 绑定IP和端口
    s.bind(('0.0.0.0', 5055))
    # 监听
    s.listen(5)
    while True:
        # 等待连接
        print('Waiting for connection...')
        sock, addr = s.accept()
        print('Connected from %s:%s' % addr)
        # thread = threading.Thread(target=recv_audio, args=(sock,))
        # thread = threading.Thread(target=recv_audio_mp3, args=(sock,))
        thread = threading.Thread(target=recv_audio_wav, args=(sock,))
        # thread = threading.Thread(target=recv_audio_pcm, args=(sock,))
        thread.start()

    # # 接受连接
    # sock, addr = s.accept()
    # print('Connected from %s:%s' % addr)
    # thread = threading.Thread(target=recv_audio, args=(sock,))
    # thread.start()
# save recv pcm data to wav
def recv_audio(conn, sample_rate=8000, num_channels=1, bits_per_sample=16):
    response = b''
    t = time.localtime()
    filename = time.strftime('%Y%m%d-%H%M%S.wav', t)
    print('Save data to file: %s' % filename)
    wav_file = wave.open(filename, 'wb')
    wav_file.setnchannels(num_channels)
    wav_file.setsampwidth(bits_per_sample // 8)
    wav_file.setframerate(sample_rate)
    # append wav header, 8k 16bit mono
    try:
        # header: 0x55 0xAA 0x55 0xAA + len(4bytes)
        data = conn.recv(8)
        if not data or len(data) != 8 or data[0] != 0x55 or data[1] != 0xAA or data[2] != 0x55 or data[3] != 0xAA:
            print('Invalid header')
            return

        # parse length
        length = int.from_bytes(data[4:], byteorder='big')
        print('Length: %d' % length)
        # max pcm time: 20s
        if length <= 0 or length > 8000 * 2 * 20:
            print('Invalid length')
            return
        response += data
        remain_len = length
        while remain_len > 0:
            data = conn.recv(min(1024, remain_len))
            if not data:
                break
            wav_file.writeframes(data)
            response += data
            remain_len -= len(data)

        print('handle data sleep 0.5s')
        time.sleep(0.5)
        # resend the data to the client
        print('Prepare to send data back to client...')
        conn.sendall(response)
        print('Send data back to client!')
    finally:
        # Close the connection and the file
        conn.close()
        wav_file.close()
        print("Finished writing WAV file.")
        print('Connection closed')

# recv mp3 from client
def recv_audio_mp3(conn):
    response = b''
    t = time.localtime()
    filename = time.strftime('%Y%m%d-%H%M%S.mp3', t)
    print('Save data to file: %s' % filename)
    mp3_file = open(filename, 'wb')
    # append wav header, 8k 16bit mono
    try:
        # break the loop when no data
        while True:
            data = conn.recv(1024)
            if not data:
                break
            mp3_file.write(data)
            response += data

        print('handle data sleep 0.5s')
        time.sleep(0.5)
        # resend the data to the client
        print('Prepare to send data back to client...')
        # conn.sendall(response)
        print('Send data back to client!')
    finally:
        # Close the connection and the file
        conn.close()
        mp3_file.close()
        print("Finished writing MP3 file.")
        print('Connection closed')

def recv_audio_wav(conn):
    sample_rate = 16000
    num_channels = 1
    bits_per_sample = 16
    response = b''
    t = time.localtime()
    filename = time.strftime('%Y%m%d-%H%M%S.wav', t)
    print('Save data to file: %s' % filename)
    wav_file = wave.open(filename, 'wb')
    wav_file.setnchannels(num_channels)
    wav_file.setsampwidth(bits_per_sample // 8)
    wav_file.setframerate(sample_rate)

    # 初始化音频播放
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16,
                   channels=num_channels,
                   rate=sample_rate,
                   output=True,
                   frames_per_buffer=6400)
    
    try:
        while True:
            data = conn.recv(3200)
            if not data:
                print('No data')
                break
            # 保存到文件
            wav_file.writeframes(data)
            # 实时播放
            print('play audio, data len: %d' % len(data))
            stream.write(data)
            response += data

        print('handle data sleep 0.5s')
    except KeyboardInterrupt:
        # KeyboardInterrupt and exit
        print('KeyboardInterrupt')
        exit(0)
    except Exception as e:
        print('Exception: %s' % e)
    finally:
        # 关闭音频流
        stream.stop_stream()
        stream.close()
        p.terminate()
        # 关闭连接和文件
        conn.close()
        wav_file.close()
        print("Finished writing WAV file.")
        print('Connection closed')


def recv_audio_pcm(conn):
    sample_rate = 16000
    num_channels = 1
    bits_per_sample = 16
    response = b''
    t = time.localtime()
    filename = time.strftime('%Y%m%d-%H%M%S.pcm', t)
    print('Save data to file: %s' % filename)
    # wav_file = wave.open(filename, 'wb')
    # wav_file.setnchannels(num_channels)
    # wav_file.setsampwidth(bits_per_sample // 8)
    # wav_file.setframerate(sample_rate)
    pcm_file = open(filename, 'wb')

    # 初始化音频播放
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16,
                   channels=num_channels,
                   rate=sample_rate,
                   output=True,
                   frames_per_buffer=6400)
    
    try:
        while True:
            data = conn.recv(3200)
            if not data:
                print('No data')
                break
            # 保存到文件
            pcm_file.write(data)
            # 实时播放
            print('play audio, data len: %d' % len(data))
            stream.write(data)
            response += data

        print('handle data sleep 0.5s')
    except KeyboardInterrupt:
        # KeyboardInterrupt and exit
        print('KeyboardInterrupt')
        exit(0)
    except Exception as e:
        print('Exception: %s' % e)
    finally:
        # 关闭音频流
        stream.stop_stream()
        stream.close()
        p.terminate()
        # 关闭连接和文件
        conn.close()
        pcm_file.close()
        print("Finished writing PCM file.")
        print('Connection closed')


if __name__ == '__main__':
    audio_proc()
