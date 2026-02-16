import serial
import wave

PORT = "COM5"
BAUD = 921600

SAMPLE_RATE = 16000
CHANNELS = 2
WIDTH = 2  # 16-bit PCM

MARK_START = b"###START###\n"
MARK_STOP  = b"###STOP###\n"

ser = serial.Serial(PORT, BAUD, timeout=0.2)

print("Waiting for START from ESP button...")

# 1) čakaj na START marker
buffer = b""
while True:
    chunk = ser.read(256)
    if chunk:
        buffer += chunk
        if MARK_START in buffer:
            # odstráň všetko pred markerom (aby sme nezačali v strede)
            buffer = buffer.split(MARK_START, 1)[1]
            break

print("START received. Writing recorded.wav ...")

wav = wave.open("recorded.wav", "wb")
wav.setnchannels(CHANNELS)
wav.setsampwidth(WIDTH)
wav.setframerate(SAMPLE_RATE)

# 2) zapisuj dáta až kým nepríde STOP marker
while True:
    chunk = ser.read(4096)
    if not chunk:
        continue

    buffer += chunk

    if MARK_STOP in buffer:
        audio_part, _ = buffer.split(MARK_STOP, 1)
        if audio_part:
            wav.writeframesraw(audio_part)
        break

    # aby buffer nerástol donekonečna, zapisuj priebežne:
    if len(buffer) > 16384:
        wav.writeframesraw(buffer)
        buffer = b""

wav.close()
ser.close()
print("STOP received. Done! Saved as recorded.wav")
