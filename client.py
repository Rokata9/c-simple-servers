import socket

def encode_varint(value):
    """
    Converts a Python integer into our custom LEB128 Varint byte array.
    """
    encoded_bytes = bytearray()
    while True:
        # Grab the bottom 7 bits
        byte = value & 0x7F
        # Shift the value right by 7 to process the next chunk
        value >>= 7
        
        if value > 0:
            # There is more data! Set the MSB (Continuation Flag) to 1
            byte |= 0x80
            
        encoded_bytes.append(byte)
        
        if value == 0:
            break
            
    return encoded_bytes

def send_message(sock, text_message):
    """
    Packs the string into our protocol format (Varint Header + Raw String)
    and sends it over the socket.
    """
    # Convert Python string to raw bytes
    payload_bytes = text_message.encode('utf-8')
    payload_length = len(payload_bytes)
    
    # Generate the header
    header_bytes = encode_varint(payload_length)
    
    # Send Header + Payload
    sock.sendall(header_bytes + payload_bytes)
    
    print(f"Sent: '{text_message}'")
    print(f" -> Header took {len(header_bytes)} byte(s)")
    print(f" -> Payload took {payload_length} byte(s)\n")

# --- MAIN EXECUTION ---
if __name__ == "__main__":
    # Create a TCP socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        # Connect to our C server
        s.connect(('127.0.0.1', 8080))
        print("Connected to C Server!\n")
        
        # Test 1: A tiny message (Header should be exactly 1 byte)
        send_message(s, "Hi")
        
        # Test 2: A medium message (Header should still be 1 byte)
        send_message(s, "This is a normal sized string.")
        
        # Test 3: A large message > 127 bytes (Header will dynamically expand to 2 bytes!)
        long_string = "A" * 300
        send_message(s, f"Here is a massive payload: {long_string}")
        
    except ConnectionRefusedError:
        print("Error: Could not connect. Is the C server running?")
    finally:
        # comment for now, uncomment in case we hit a known WSL2 networking quirk,
        # where Sometimes, if a Python script on Windows sends data and instantly closes the socket before 
        # the Linux VM processes it, the OS forcefully drops the buffered data.
        # we add 1s delay so the server can process the data, before disconnecting

        # import time
        # time.sleep(1) # Give the C server 1 second to read the buffer!
        s.close()
        print("Disconnected.")
        