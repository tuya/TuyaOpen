import serial
import serial.tools.list_ports
import threading
import time
import os
import sys

class SerialTerminal:
    def __init__(self):
        self.ser = None
        self.running = False
        self.dump_files = {
            '0': {'file': None, 'name': 'dump_mic.pcm', 'active': False, 'length': 0},
            '1': {'file': None, 'name': 'dump_ref.pcm', 'active': False, 'length': 0},
            '2': {'file': None, 'name': 'dump_aec.pcm', 'active': False, 'length': 0},
            '3': {'file': None, 'name': 'dump_asr.pcm', 'active': False, 'length': 0}
        }
        self.current_dump_channel = None
        self.last_data_time = 0
        self.timeout_timer = None
        self.overwrite_files = True  # 覆盖已存在的文件

    def list_ports(self):
        """列出所有可用串口"""
        ports = serial.tools.list_ports.comports()
        if not ports:
            print("没有找到可用串口！")
            return None
        print("\n可用串口列表:")
        for i, port in enumerate(ports):
            print(f"{i+1}. {port.device} - {port.description}")
        return ports

    def open_port(self, port_name):
        """打开指定串口"""
        try:
            self.ser = serial.Serial(
                port=port_name,
                baudrate=460800,
                timeout=0.1
            )
            time.sleep(0.5)  # 等待串口初始化
            return True
        except Exception as e:
            print(f"打开串口失败: {e}")
            return False

    def start_reading(self):
        """启动数据读取线程"""
        self.running = True
        self.read_thread = threading.Thread(target=self.read_from_serial)
        self.read_thread.daemon = True
        self.read_thread.start()

    def stop_reading(self):
        """停止数据读取"""
        self.running = False
        if hasattr(self, 'read_thread') and self.read_thread.is_alive():
            self.read_thread.join()

    def read_from_serial(self):
        """串口数据读取线程"""
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    if self.ser.in_waiting > 0:
                        data = self.ser.read(self.ser.in_waiting)
                        self.last_data_time = time.time()  # 更新最后接收时间
                        self.process_received_data(data)
                except Exception as e:
                    print(f"读取错误: {e}")
            
            # 检查dump超时
            if self.current_dump_channel:
                if time.time() - self.last_data_time > 0.1:  # 100ms超时
                    self.stop_dump(self.current_dump_channel)
            
            time.sleep(0.01)

    def process_received_data(self, data):
        """处理接收到的数据"""
        # 更新dump文件长度
        if self.current_dump_channel and self.dump_files[self.current_dump_channel]['active']:
            self.dump_files[self.current_dump_channel]['length'] += len(data)
            self.update_length_display()
        
        # 写入当前dump文件
        if self.current_dump_channel and self.dump_files[self.current_dump_channel]['file']:
            self.dump_files[self.current_dump_channel]['file'].write(data)
        
        # 非dump模式下显示接收数据
        if not self.current_dump_channel:
            try:
                print(data.decode('utf-8', errors='replace'), end='')
            except:
                pass  # 忽略非文本数据

    def update_length_display(self):
        """更新dump长度显示（同一行刷新）"""
        if self.current_dump_channel:
            channel = self.current_dump_channel
            length = self.dump_files[channel]['length']
            times = length/32000
            sys.stdout.write(f"\r接收数据长度: {length} 字节, 录音时间 {times:.3f} s")
            sys.stdout.flush()

    def send_command(self, cmd):
        """发送命令到串口"""
        if not self.ser or not self.ser.is_open:
            print("串口未连接！")
            return False
        
        try:
            # 添加"ao "前缀和回车换行
            full_cmd = f"ao {cmd}\r\n"
            self.ser.write(full_cmd.encode('utf-8'))
            print(f"已发送: ao {cmd}")
            return True
        except Exception as e:
            print(f"发送失败: {e}")
            return False

    def start_dump(self, channel):
        """启动数据转储"""
        if channel not in self.dump_files:
            print(f"无效通道: {channel}")
            return
        
        # 如果已有dump在运行，先停止
        if self.current_dump_channel:
            self.stop_dump(self.current_dump_channel)
        
        # 发送dump命令
        self.send_command(f"dump {channel}")
        
        try:
            # 打开文件（二进制写入），覆盖已存在的文件
            self.dump_files[channel]['file'] = open(self.dump_files[channel]['name'], 'wb')
            self.dump_files[channel]['active'] = True
            self.dump_files[channel]['length'] = 0
            self.current_dump_channel = channel
            self.last_data_time = time.time()  # 记录开始时间
            
            print(f"开始转储通道 {channel} -> {self.dump_files[channel]['name']}")
            print("等待接收数据...(100ms无数据自动停止)")
        except Exception as e:
            print(f"创建文件失败: {e}")

    def stop_dump(self, channel=None):
        """停止数据转储"""
        if not channel and self.current_dump_channel:
            channel = self.current_dump_channel
        
        if channel and channel in self.dump_files and self.dump_files[channel]['active']:
            # 停止指定通道
            if self.dump_files[channel]['file']:
                self.dump_files[channel]['file'].close()
            self.dump_files[channel]['file'] = None
            self.dump_files[channel]['active'] = False
            
            length = self.dump_files[channel]['length']
            print(f"\n已停止转储通道 {channel}, 接收长度: {length} 字节")
            print(f"数据已保存到: {self.dump_files[channel]['name']}")
            
            # 重置当前通道
            self.current_dump_channel = None

    def close_port(self):
        """关闭串口"""
        if self.current_dump_channel:
            self.stop_dump(self.current_dump_channel)
        
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("串口已关闭")

def main():
    terminal = SerialTerminal()
    
    # 列出并选择串口
    ports = terminal.list_ports()
    if not ports:
        return
    
    try:
        choice = int(input("\n请选择串口号: ")) - 1
        selected_port = ports[choice].device
    except (ValueError, IndexError):
        print("无效选择！")
        return
    
    # 打开串口
    if not terminal.open_port(selected_port):
        return
    print(f"已打开串口: {selected_port}, 波特率: 460800")
    
    # 启动读取线程
    terminal.start_reading()
    
    # 主命令循环
    print("\n支持命令:")
    print("start       - 启动录音")
    print("stop        - 停止录音")
    print("reset       - 重置录音")
    print("dump 0      - 转储麦克风通道到 dump_mic.pcm")
    print("dump 1      - 转储参考通道到 dump_ref.pcm")
    print("dump 2      - 转储AEC通道到 dump_aec.pcm")
    print("dump 3      - 转储ASR通道到 dump_asr.pcm")
    print("bg 0        - white noise")
    print("bg 1        - 1K-0dB (bg 1 1000)")
    print("bg 2        - sweep frequency constantly")
    print("bg 3        - sweep discrete frequency")
    print("bg 4        - min single frequency")
    print("volume 50   - 设置音量为 50%")
    print("micgain 50   - default micgain=70")
    print("alg set <para> <value> - 设置音频算法参数 (如: alg set aec_ec_depth 1)")
    print("alg set vad_SPthr <0-13> <value> - 设置音频算法参数 (如: alg set vad_SPthr 0 1000)")
    print("alg get <para> - 获取音频算法参数 (如: alg get aec_ec_depth)")
    print("alg get vad_SPthr <0-13> - 获取音频算法参数 (如: alg get vad_SPthr 0)")
    print("alg dump    - 转储音频算法参数")
    print("quit        - 退出程序")
    
    # 检查并覆盖已存在的文件
    for channel in terminal.dump_files:
        filename = terminal.dump_files[channel]['name']
        if os.path.exists(filename):
            print(f"注意: 已存在文件 {filename}，将被覆盖")
    
    try:
        while True:
            user_input = input("> ").strip()
            
            if user_input == 'quit':
                break
            
            # 处理dump命令
            elif user_input in ['dump 0', 'dump 1', 'dump 2', 'dump 3']:
                channel = user_input.split()[1]
                terminal.start_dump(channel)
            
            # 处理其他命令
            # 新增的命令处理放这里
            elif user_input in ['start', 'stop', 'reset', 'bg 0', 'bg 1', 'bg 2', 'bg 3', 'bg 4']:
                terminal.send_command(user_input)
            # 处理 bg 1 单频率命令
            elif user_input.startswith('bg 1 '):
                terminal.send_command(user_input)
            # 处理音量设置
            elif user_input.startswith('volume '):
                terminal.send_command(user_input)
            elif user_input.startswith('micgain '):
                terminal.send_command(user_input)
            elif user_input.startswith('alg set '):
                parts = user_input.split()
                if len(parts) == 4:
                    terminal.send_command(f"alg set {parts[2]} {parts[3]}")
                elif len(parts) == 5:
                    # SPthr 参数需要两个值
                    idx = parts[3] # 0~13
                    value = parts[4] # 0~65535
                    # check if idx is a number
                    if idx.isdigit() and 0 <= int(idx) <= 13:
                        # 拼接命令 spthr_idx[8] || rev[8] || value[16]
                        terminal.send_command(f"alg set {parts[2]} {(int(idx) << 24) | (int(value) & 0xFFFF)}")
                        print(f"设置 SPthr idx={idx}, value={value}, real value={(int(idx) << 24) | (int(value) & 0xFFFF)}")
                    else:
                        print("SPthr 参数错误: idx 应在 0~13 之间")
                        print("命令格式错误: alg set vad_SPthr <idx> <value>")
                else:
                    print("命令格式错误: alg set <para> <value>")
            elif user_input.startswith('alg get '):
                parts = user_input.split()
                if len(parts) == 3:
                    terminal.send_command(f"alg get {parts[2]}")
                elif len(parts) == 4 and parts[2] == 'vad_SPthr':
                    idx = parts[3]
                    if idx.isdigit() and 0 <= int(idx) <= 13:
                        terminal.send_command(f"alg get vad_SPthr {(int(idx) << 24)}")
                    else:
                        print("SPthr 参数错误: idx 应在 0~13 之间")
                else:
                    print("命令格式错误: alg get <para> 或 alg get vad_SPthr <idx>")
            elif user_input.startswith('alg dump'):
                terminal.send_command("alg dump")
            elif user_input in ['netdump 0', 'netdump 1', 'netdump 2']:
                terminal.send_command(user_input)
            # 提示支持命令
            else:
                print("支持的命令: start, stop, reset, dump 0, dump 1, dump 2, bg 0, bg 1, bg 2, bg 3, bg 4, etc. quit")
    except KeyboardInterrupt:
        print("\n程序中断")
    finally:
        # 清理资源
        terminal.stop_reading()
        terminal.close_port()

if __name__ == "__main__":
    main()