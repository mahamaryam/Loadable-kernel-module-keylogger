## LKM Keylogger

This project uses DNS exfiltration to send keystrokes (encoded in Base64) within the DNS query name field, using Loadable Kernel Modules (LKMs).

### How it works:

- Random data typed is captured.
- The captured characters are Base64-encoded.
- The encoded string is inserted into the DNS query name.
- This DNS traffic can then be observed and decoded using tools like Wireshark.

---

### Screenshots

1. **Random data typed on the terminal**  
   ![Screenshot 1](./images/Screenshot%20from%202025-05-11%2015-27-09.png)

2. **Base64-encoded keystrokes detected in a DNS packet**  
   ![Screenshot 2](./images/Screenshot%20from%202025-05-11%2015-23-02.png)

3. **Decoded keystrokes from DNS query**  
   ![Screenshot 3](./images/Screenshot%20from%202025-05-11%2015-26-52.png)
