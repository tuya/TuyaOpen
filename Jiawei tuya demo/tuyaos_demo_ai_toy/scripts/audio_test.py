import numpy as np
import soundfile as sf
from scipy.signal import coherence, find_peaks
from scipy.fft import rfft, rfftfreq
import matplotlib.pyplot as plt
import argparse

def load_signals(mic1_path, mic2_path, ref_path, sr=16000, dtype='int16'):
    def read_pcm(path):
        data = np.fromfile(path, dtype=dtype)
        return data.astype(np.float32) / np.iinfo(dtype).max
    mic1 = read_pcm(mic1_path)
    mic2 = read_pcm(mic2_path)
    ref = read_pcm(ref_path)
    min_len = min(len(mic1), len(mic2), len(ref))
    def split_pcm(data):
        frame = 640
        blocks = data.size//frame
        idx = 0
        for i in range(blocks):
            chunk = data[i*frame:i*frame+frame]
            energy = np.mean(chunk**2)            
            if energy>=0.0001:
                idx = i*frame
                break
        return idx
    loc = split_pcm(ref)      
    return mic1[loc:loc+16000*2], mic2[loc:loc+16000*2], ref[loc:loc+16000*2], sr

def dc_offset(signal):
    if signal is None:
        return 0
    else:
        dc = np.mean(signal)
        if dc > 0.01:
            print("dc offset: 不通过")
        else:
            print("dc offset: 通过")
        return dc

def mic_coherence(mic1, mic2, ref, sr):

    plt.figure(figsize=(8, 4))
    # 对齐信号（以ref为基准，mic1/mic2与ref做互相关，找到最大相关位置进行对齐）
    mic1_aligned, ref1 = align_signal(mic1, ref)
    f, Cxy_1 = coherence(mic1_aligned, ref1, fs=sr, window='hann', nperseg=1024, noverlap=512)
    f, Cxy_3 = coherence(ref, ref, fs=sr, window='hann', nperseg=1024, noverlap=512)
    plt.semilogx(f, Cxy_1, label='Mic1-Ref')
    plt.semilogx(f, Cxy_3, label='Ref-Ref', linestyle='--', color='gray')
    if mic2 is not None:
        mic2_aligned, ref2 = align_signal(mic2, ref)
        f, Cxy_2 = coherence(mic2_aligned, ref2, fs=sr, window='hann', nperseg=1024, noverlap=512) 
        plt.semilogx(f, Cxy_2, label='Mic2-Ref')

    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Coherence')
    plt.title('Microphone Coherence')
    plt.legend()
    plt.grid(True, which='both', ls='--')
    # plt.show()
    plt.savefig('mic_coherence.png')
    if mic2 is not None:
        Cxy = (Cxy_1 + Cxy_2) / 2
    else:
        Cxy = Cxy_1

    if np.mean(Cxy[(f >= 100) & (f < 4000)]) > 0.7:
        print(" coherence: 通过")
    else:
        print(" coherence: 不通过")
    return f, Cxy
# 对齐信号
def align_signal(sig, ref):
    # Downsample for faster cross-correlation
    ds_factor = 8
    sig_ds = sig[::ds_factor]
    ref_ds = ref[::ds_factor]
    corr = np.correlate(sig_ds, ref_ds, mode='full')
    delay_ds = np.argmax(corr) - len(ref_ds) + 1
    delay = delay_ds * ds_factor
    if delay > 0:
        aligned = sig[delay:]
        ref_aligned = ref[:len(aligned)]
    elif delay < 0:
        aligned = sig[:len(sig)+delay]
        ref_aligned = ref[-delay:len(ref)+delay]
    else:
        aligned = sig
        ref_aligned = ref
    min_len = min(len(aligned), len(ref_aligned))
    return aligned[:min_len], ref_aligned[:min_len]

# 底噪
def noise_floor(signal):
    # 计算RMS噪声的dB值（相对于满刻度，FS）
    rms = np.sqrt(np.mean(np.abs(signal)**2))
    db = 20 * np.log10(rms + 1e-12)  # 防止log(0)
    # print(f"Noise floor: {db:.2f} dBFS")
    if db > -50:
        print("Noise floor: 不通过")
    else:
        print("Noise floor: 通过")
    return db

# 削波检测
def clip_detect(signal):
    # Return the number of clipped samples (>= 99% of full scale)
    threshold = 0.99
    clipped_samples = np.sum(np.abs(signal) >= threshold)
    if clipped_samples > 0:
        print("Clipping: 不通过")
    else:
        print("Clipping: 通过")
    return clipped_samples
# 总谐波失真
def thd(signal, sr, freq):
    # Calculate THD at given fundamental frequency
    N = len(signal)
    yf = np.abs(rfft(signal))
    xf = rfftfreq(N, 1/sr)
    fund_idx = np.argmin(np.abs(xf - freq))
    fund_power = yf[fund_idx]
    harmonics = [2, 3, 4, 5]
    harm_power = 0
    for h in harmonics:
        idx = np.argmin(np.abs(xf - h*freq))
        harm_power += yf[idx]**2
    thd_value = np.sqrt(harm_power) / fund_power
    if thd_value > 0.05:
        print("THD: 不通过")
    else:
        print("THD: 通过")
    return thd_value

# 延时稳定性
def plot_delay_stability_over_time(mic1, mic2, ref, sr, window_ms=300, hop_ms=100, save_path='delay_stability.png'):
    """
    每window_ms毫秒计算一次延时,窗移hop_ms毫秒,画出延时随时间变化的曲线,并保存图片。
    """
    window_size = int((window_ms / 1000) * sr)
    hop_size = int((hop_ms / 1000) * sr)
    num_windows = (len(mic1) - window_size) // hop_size + 1
    times = []
    delays1 = []
    delays2 = []
    for i in range(num_windows):
        start = i * hop_size
        end = start + window_size
        mic1_win = mic1[start:end]
        mic2_win = mic2[start:end]
        ref_win = ref[start:end]
        corr1 = np.correlate(mic1_win, ref_win, mode='full')
        delay_samples1 = np.argmax(corr1) - len(ref_win) + 1
        corr2 = np.correlate(mic2_win, ref_win, mode='full')
        delay_samples2 = np.argmax(corr2) - len(ref_win) + 1
        delays1.append(delay_samples1)
        delays2.append(delay_samples2)
        times.append(start / sr)
    diff1 = np.diff(delays1,1)
    diff2 = np.diff(delays2,1)
    if np.any(np.abs(diff1) > 2) or np.any(np.abs(diff2) > 2):
        print("延迟波动过大: 不通过")
    else:
        print("延迟波动正常: 通过")
    print
    plt.figure(figsize=(8, 4))
    plt.plot(times, delays1, label='Mic1-Ref')
    plt.plot(times, delays2, label='Mic2-Ref')
    plt.xlabel('Time (s)')
    plt.ylabel('Delay (samples)')
    plt.title(f'Delay Stability Over Time (win_len:300ms, overlap:100ms)')
    plt.legend()
    plt.grid(True)
    plt.savefig(save_path)
    plt.show()
    print("\nfigure saved at delay_stability.png")
def main():

    parser = argparse.ArgumentParser(description='audio_test')
    parser.add_argument('--mic1', type=str, help='Mic1 PCM file path')
    parser.add_argument('--ref', type=str, help='Path to ref signal file')
    # ['dc_offset', 'coherence', 'noise_floor', 'clip_detect', 'thd', 'delay_stability', 'all']
    parser.add_argument('--test_case', type=str, choices=['1k_single', 'white_noise', 'silence'], default='silence', help='Test item to run')
    args = parser.parse_args()
    # 读取wav文件
    # import soundfile as sf
    # data, sr = sf.read('test_1k.wav')  # 假设test_1k.wav是双通道
    # mic1 = data[:, 0]   #mic1信号
    # ref = data[:, 1]    #参考信号
    # mic2 = mic1  # 根据实际情况设置mic2
    mic1, mic2, ref, sr = load_signals(args.mic1, args.mic1, args.ref)    
    if args.test_case == '1k_single':
        print("直流偏置 (DC Offset):")
        print(f"Mic1: {dc_offset(mic1):.6f}, Ref: {dc_offset(ref):.6f}")
        print("\n总谐波失真 (THD):")
        # Assume test tone at 1kHz
        freq = 1000
        print(f"Mic1: {thd(mic1, sr, freq):.4f}, Ref: {thd(ref, sr, freq):.4f}")

    if args.test_case == 'white_noise':
        print("直流偏置 (DC Offset):")
        print(f"Mic1: {dc_offset(mic1):.6f}, Ref: {dc_offset(ref):.6f}")
        print("\nMic相干性 (Coherence):")
        f,Cxy = mic_coherence(mic1, mic2, ref, sr)
        print(f"Mean coherence (0-4kHz): {np.mean(Cxy[f < 4000]):.3f}")
        print("\n削波检测 (Signal Integrity):")
        clipped1 = clip_detect(mic1)
        clipped2 = clip_detect(mic2)
        print(f"Mic1: Clipped={clipped1}")
        print(f"Mic2: Clipped={clipped2}")
        print("\n延时稳定性 (Delay Stability)")
        plot_delay_stability_over_time(mic1, mic2, ref, sr)

    if args.test_case == 'silence':
        print("直流偏置 (DC Offset):")
        print(f"Mic1: {dc_offset(mic1):.6f}, Mic2: {dc_offset(mic2):.6f}, Ref: {dc_offset(ref):.6f}")
        print("\n底噪 (Noise Floor):")
        print(f"Mic1: {noise_floor(mic1):.6f}, Mic2: {noise_floor(mic2):.6f}")
    

if __name__ == "__main__":
    print("开始测试")
    main()
    print("结束测试")
 