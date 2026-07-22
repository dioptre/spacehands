import mido
import time
import socket
import struct
import threading
import sys

print("=== Starting Virtual MIDI Hub & Delayed SuperDirt Trigger ===")
print("Creating virtual MIDI input and output ports named 'TidalMIDI'...")

try:
    inport = mido.open_input('TidalMIDI', virtual=True)
    outport = mido.open_output('TidalMIDI', virtual=True)
    print("Virtual MIDI port 'TidalMIDI' successfully created and listening!\n")
except Exception as e:
    print(f"Error opening virtual MIDI port: {e}", file=sys.stderr)
    sys.exit(1)

osc_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
DELAY_SECONDS = 3.0

def play_delayed_note(note_num, velocity):
    note_offset = note_num - 60
    print(f" >>> [+3.0s Trigger] Playing note {note_num} (offset {note_offset}) in SuperDirt via OSC...")
    
    # Send OSC packet /play_midi_note to SuperCollider (port 57120)
    sc_target = ('127.0.0.1', 57120)
    path = b"/play_midi_note\x00\x00\x00"
    typetag = b",i\x00\x00"
    args = struct.pack(">i", int(note_num))
    packet = path + typetag + args
    
    osc_sock.sendto(packet, sc_target)

def midi_listener():
    print("[MIDI Listener] Listening for incoming MIDI note_on events...")
    for msg in inport:
        if msg.type == 'note_on' and msg.velocity > 0:
            print(f" >>> [MIDI Received] NoteOn -> note: {msg.note}, velocity: {msg.velocity} | Scheduling play in {DELAY_SECONDS}s...")
            t = threading.Timer(DELAY_SECONDS, play_delayed_note, args=[msg.note, msg.velocity])
            t.daemon = True
            t.start()

listener_thread = threading.Thread(target=midi_listener, daemon=True)
listener_thread.start()

if __name__ == '__main__':
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting MIDI Hub.")
