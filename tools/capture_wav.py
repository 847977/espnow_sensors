import serial
import wave
import time

PORT = "COM6"
BAUD = 921600

DURATION = 5.0   # sekund
SAMPLE_RATE = 16000
CHANNELS = 2
WIDTH = 2        # 16-bit PCM = 2 bytes

ser = serial.Serial(PORT, BAUD)

wav = wave.open("recorded.wav", "wb")
wav.setnchannels(CHANNELS)
wav.setsampwidth(WIDTH)
wav.setframerate(SAMPLE_RATE)

print("Recording...")

start = time.time()
while time.time() - start < DURATION:
    data = ser.read(4096)   # čítame po kusoch
    wav.writeframesraw(data)

wav.close()
ser.close()
print("Done! Saved as recorded.wav")
